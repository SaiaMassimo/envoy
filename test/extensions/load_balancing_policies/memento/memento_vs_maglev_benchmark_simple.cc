#include "source/extensions/load_balancing_policies/memento/memento_lb.h"
#include "source/extensions/load_balancing_policies/maglev/maglev_lb.h"

#include "test/benchmark/main.h"
#include "test/extensions/load_balancing_policies/common/benchmark_base_tester.h"

namespace Envoy {
namespace Upstream {
namespace {

// Tester per Memento
class MementoTester : public BaseTester {
public:
  MementoTester(uint64_t num_hosts, uint32_t weighted_subset_percent = 0, uint32_t weight = 0)
      : BaseTester(num_hosts, weighted_subset_percent, weight) {
    const uint64_t table_size = 65537;
    memento_lb_config_ = std::make_unique<Extensions::LoadBalancingPolicies::Memento::MementoLbConfig>(table_size);
    memento_lb_ = std::make_unique<Extensions::LoadBalancingPolicies::Memento::MementoLoadBalancer>(
        priority_set_, stats_, stats_scope_, runtime_, random_, 50, *memento_lb_config_);
  }

  std::unique_ptr<Extensions::LoadBalancingPolicies::Memento::MementoLbConfig> memento_lb_config_;
  std::shared_ptr<TestHashPolicy> hash_policy_ = std::make_shared<TestHashPolicy>();
  std::unique_ptr<Extensions::LoadBalancingPolicies::Memento::MementoLoadBalancer> memento_lb_;
};

// Tester per Maglev
class MaglevTester : public BaseTester {
public:
  MaglevTester(uint64_t num_hosts, uint32_t weighted_subset_percent = 0, uint32_t weight = 0)
      : BaseTester(num_hosts, weighted_subset_percent, weight) {
    envoy::extensions::load_balancing_policies::maglev::v3::Maglev config;
    maglev_lb_ = std::make_unique<MaglevLoadBalancer>(priority_set_, stats_, stats_scope_, runtime_,
                                                      random_, 50, config, hash_policy_);
  }

  std::shared_ptr<TestHashPolicy> hash_policy_ = std::make_shared<TestHashPolicy>();
  std::unique_ptr<MaglevLoadBalancer> maglev_lb_;
};

// Benchmark 1: Confronto performance di selezione host
void benchmarkChooseHostPerformance(::benchmark::State& state) {
  for (auto _ : state) {
    state.PauseTiming();
    const uint64_t num_hosts = state.range(0);
    const uint64_t keys_to_simulate = state.range(1);
    const bool use_memento = state.range(2);
    
    LoadBalancerPtr lb;
    std::shared_ptr<TestHashPolicy> hash_policy = std::make_shared<TestHashPolicy>();
    
    if (use_memento) {
      MementoTester tester(num_hosts);
      ASSERT_TRUE(tester.memento_lb_->initialize().ok());
      lb = tester.memento_lb_->factory()->create(tester.lb_params_);
      hash_policy = tester.hash_policy_;
    } else {
      MaglevTester tester(num_hosts);
      ASSERT_TRUE(tester.maglev_lb_->initialize().ok());
      lb = tester.maglev_lb_->factory()->create(tester.lb_params_);
      hash_policy = tester.hash_policy_;
    }
    
    absl::node_hash_map<std::string, uint64_t> hit_counter;
    TestLoadBalancerContext context;
    state.ResumeTiming();

    for (uint64_t i = 0; i < keys_to_simulate; i++) {
      hash_policy->hash_key_ = hashInt(i);
      hit_counter[lb->chooseHost(&context).host->address()->asString()] += 1;
    }

    state.PauseTiming();
    computeHitStats(state, hit_counter);
    state.counters["algorithm"] = use_memento ? 1 : 0; // 1 = Memento, 0 = Maglev
    state.ResumeTiming();
  }
}
BENCHMARK(benchmarkChooseHostPerformance)
    ->Args({100, 100000, 1})   // 100 host, 100k keys, Memento
    ->Args({100, 100000, 0})   // 100 host, 100k keys, Maglev
    ->Args({200, 100000, 1})   // 200 host, 100k keys, Memento
    ->Args({200, 100000, 0})   // 200 host, 100k keys, Maglev
    ->Args({500, 100000, 1})   // 500 host, 100k keys, Memento
    ->Args({500, 100000, 0})   // 500 host, 100k keys, Maglev
    ->Unit(::benchmark::kMillisecond);

// Benchmark 2: Confronto costruzione tabella
void benchmarkTableBuildPerformance(::benchmark::State& state) {
  for (auto _ : state) {
    state.PauseTiming();
    const uint64_t num_hosts = state.range(0);
    const bool use_memento = state.range(1);
    
    const size_t start_mem = Memory::Stats::totalCurrentlyAllocated();
    state.ResumeTiming();
    
    if (use_memento) {
      MementoTester tester(num_hosts);
      ASSERT_TRUE(tester.memento_lb_->initialize().ok());
    } else {
      MaglevTester tester(num_hosts);
      ASSERT_TRUE(tester.maglev_lb_->initialize().ok());
    }
    
    state.PauseTiming();
    const size_t end_mem = Memory::Stats::totalCurrentlyAllocated();
    state.counters["memory"] = end_mem - start_mem;
    state.counters["memory_per_host"] = (end_mem - start_mem) / num_hosts;
    state.counters["algorithm"] = use_memento ? 1 : 0;
    state.ResumeTiming();
  }
}
BENCHMARK(benchmarkTableBuildPerformance)
    ->Args({100, 1})   // 100 host, Memento
    ->Args({100, 0})   // 100 host, Maglev
    ->Args({200, 1})   // 200 host, Memento
    ->Args({200, 0})   // 200 host, Maglev
    ->Args({500, 1})   // 500 host, Memento
    ->Args({500, 0})   // 500 host, Maglev
    ->Args({1000, 1})   // 1000 host, Memento
    ->Args({1000, 0})   // 1000 host, Maglev
    ->Unit(::benchmark::kMillisecond);

// Benchmark 3: Confronto weighted load balancing
void benchmarkWeightedLoadBalancing(::benchmark::State& state) {
  for (auto _ : state) {
    state.PauseTiming();
    const uint64_t num_hosts = state.range(0);
    const uint64_t weighted_subset_percent = state.range(1);
    const uint64_t weight = state.range(2);
    const uint64_t keys_to_simulate = state.range(3);
    const bool use_memento = state.range(4);
    
    LoadBalancerPtr lb;
    std::shared_ptr<TestHashPolicy> hash_policy = std::make_shared<TestHashPolicy>();
    
    if (use_memento) {
      MementoTester tester(num_hosts, weighted_subset_percent, weight);
      ASSERT_TRUE(tester.memento_lb_->initialize().ok());
      lb = tester.memento_lb_->factory()->create(tester.lb_params_);
      hash_policy = tester.hash_policy_;
    } else {
      MaglevTester tester(num_hosts, weighted_subset_percent, weight);
      ASSERT_TRUE(tester.maglev_lb_->initialize().ok());
      lb = tester.maglev_lb_->factory()->create(tester.lb_params_);
      hash_policy = tester.hash_policy_;
    }
    
    absl::node_hash_map<std::string, uint64_t> hit_counter;
    TestLoadBalancerContext context;
    state.ResumeTiming();

    for (uint64_t i = 0; i < keys_to_simulate; i++) {
      hash_policy->hash_key_ = hashInt(i);
      hit_counter[lb->chooseHost(&context).host->address()->asString()] += 1;
    }

    state.PauseTiming();
    computeHitStats(state, hit_counter);
    state.counters["algorithm"] = use_memento ? 1 : 0;
    state.counters["weighted_subset_percent"] = weighted_subset_percent;
    state.counters["weight"] = weight;
    state.ResumeTiming();
  }
}
BENCHMARK(benchmarkWeightedLoadBalancing)
    // Test con 500 host
    ->Args({500, 5, 1, 10000, 1})    // 500 host, 5% weighted, weight=1, Memento
    ->Args({500, 5, 1, 10000, 0})    // 500 host, 5% weighted, weight=1, Maglev
    ->Args({500, 5, 127, 10000, 1})  // 500 host, 5% weighted, weight=127, Memento
    ->Args({500, 5, 127, 10000, 0})  // 500 host, 5% weighted, weight=127, Maglev
    ->Args({500, 50, 1, 10000, 1})   // 500 host, 50% weighted, weight=1, Memento
    ->Args({500, 50, 1, 10000, 0})   // 500 host, 50% weighted, weight=1, Maglev
    ->Args({500, 50, 127, 10000, 1}) // 500 host, 50% weighted, weight=127, Memento
    ->Args({500, 50, 127, 10000, 0}) // 500 host, 50% weighted, weight=127, Maglev
    
    // Test con 1000 host (più intensivi)
    ->Args({1000, 5, 1, 100000, 1})    // 1000 host, 5% weighted, weight=1, Memento
    ->Args({1000, 5, 1, 100000, 0})    // 1000 host, 5% weighted, weight=1, Maglev
    ->Args({1000, 5, 127, 100000, 1})  // 1000 host, 5% weighted, weight=127, Memento
    ->Args({1000, 5, 127, 100000, 0})  // 1000 host, 5% weighted, weight=127, Maglev
    ->Args({1000, 50, 1, 100000, 1})   // 1000 host, 50% weighted, weight=1, Memento
    ->Args({1000, 50, 1, 100000, 0})   // 1000 host, 50% weighted, weight=1, Maglev
    ->Args({1000, 50, 127, 100000, 1}) // 1000 host, 50% weighted, weight=127, Memento
    ->Args({1000, 50, 127, 100000, 0}) // 1000 host, 50% weighted, weight=127, Maglev
    ->Unit(::benchmark::kMillisecond);

// Benchmark 4: Scenario realistico con aggiornamenti incrementali (semplificato)
void benchmarkRealisticScenario(::benchmark::State& state) {
  for (auto _ : state) {
    state.PauseTiming();
    const uint64_t num_hosts = state.range(0);
    const uint64_t requests_before_removal = state.range(1);
    const uint64_t requests_after_removal = state.range(2);
    const bool use_memento = state.range(3);
    
    LoadBalancerPtr lb;
    std::shared_ptr<TestHashPolicy> hash_policy = std::make_shared<TestHashPolicy>();
    
    // Dichiarazione dei tester fuori dal blocco condizionale
    MementoTester memento_tester(num_hosts);
    MaglevTester maglev_tester(num_hosts);
    
    if (use_memento) {
      ASSERT_TRUE(memento_tester.memento_lb_->initialize().ok());
      lb = memento_tester.memento_lb_->factory()->create(memento_tester.lb_params_);
      hash_policy = memento_tester.hash_policy_;
    } else {
      ASSERT_TRUE(maglev_tester.maglev_lb_->initialize().ok());
      lb = maglev_tester.maglev_lb_->factory()->create(maglev_tester.lb_params_);
      hash_policy = maglev_tester.hash_policy_;
    }
    
    absl::node_hash_map<std::string, uint64_t> hit_counter;
    TestLoadBalancerContext context;
    
    // Fase 1: Richieste iniziali
    state.ResumeTiming();
    for (uint64_t i = 0; i < requests_before_removal; i++) {
      hash_policy->hash_key_ = hashInt(i);
      hit_counter[lb->chooseHost(&context).host->address()->asString()] += 1;
    }
    state.PauseTiming();
    
    // Fase 2: Simula rimozione host
    if (use_memento) {
      // Per Memento: usa update() incrementale
      // Crea un nuovo priority set con un host in meno
      Upstream::HostVector reduced_hosts;
      std::shared_ptr<Upstream::MockClusterInfo> info = std::make_shared<NiceMock<Upstream::MockClusterInfo>>();
      for (uint64_t i = 1; i < num_hosts; i++) { // Salta il primo host (indice 0)
        const std::string url = fmt::format("tcp://10.0.{}.{}:6379", i / 256, i % 256);
        reduced_hosts.push_back(Upstream::makeTestHost(info, url, 1));
      }
      
      Upstream::HostVectorConstSharedPtr updated_hosts = std::make_shared<Upstream::HostVector>(reduced_hosts);
      Upstream::HostsPerLocalityConstSharedPtr hosts_per_locality =
          Upstream::makeHostsPerLocality({reduced_hosts});
      
      // Aggiorna il priority set del tester originale
      memento_tester.priority_set_.updateHosts(
          0, Upstream::HostSetImpl::partitionHosts(updated_hosts, hosts_per_locality), {}, reduced_hosts, {},
          absl::nullopt);
      memento_tester.local_priority_set_.updateHosts(
          0, Upstream::HostSetImpl::partitionHosts(updated_hosts, hosts_per_locality), {}, reduced_hosts, {},
          absl::nullopt);
      
      // Crea un nuovo load balancer con il priority set aggiornato
      // Questo triggererà l'update() incrementale nel MementoLoadBalancer
      lb = memento_tester.memento_lb_->factory()->create(memento_tester.lb_params_);
      hash_policy = memento_tester.hash_policy_;
    } else {
      // Per Maglev: ricrea tutto (comportamento normale)
      MaglevTester tester_reduced(num_hosts - 1); // Un host in meno
      ASSERT_TRUE(tester_reduced.maglev_lb_->initialize().ok());
      lb = tester_reduced.maglev_lb_->factory()->create(tester_reduced.lb_params_);
      hash_policy = tester_reduced.hash_policy_;
    }
    
    // Fase 3: Richieste dopo rimozione
    state.ResumeTiming();
    for (uint64_t i = requests_before_removal; i < requests_before_removal + requests_after_removal; i++) {
      hash_policy->hash_key_ = hashInt(i);
      hit_counter[lb->chooseHost(&context).host->address()->asString()] += 1;
    }
    state.PauseTiming();
    
    computeHitStats(state, hit_counter);
    state.counters["algorithm"] = use_memento ? 1 : 0;
    state.counters["hosts_removed"] = 1;
    state.counters["total_requests"] = requests_before_removal + requests_after_removal;
    state.ResumeTiming();
  }
}
BENCHMARK(benchmarkRealisticScenario)
    // Scenario realistico: 20 host, 50 richieste, rimozione 1 host, 50 richieste
    ->Args({20, 50, 50, 1})   // 20 host, 50+50 richieste, Memento
    ->Args({20, 50, 50, 0})   // 20 host, 50+50 richieste, Maglev
    
    // Scenario più intensivo: 100 host, 100 richieste, rimozione 1 host, 100 richieste
    ->Args({100, 100, 100, 1}) // 100 host, 100+100 richieste, Memento
    ->Args({100, 100, 100, 0}) // 100 host, 100+100 richieste, Maglev
    ->Unit(::benchmark::kMillisecond);

// Benchmark 5: Scenario completo con aggiunta e rimozione host
void benchmarkCompleteScenario(::benchmark::State& state) {
  for (auto _ : state) {
    state.PauseTiming();
    const uint64_t num_hosts = state.range(0);
    const uint64_t requests_initial = state.range(1);
    const uint64_t requests_after_removal = state.range(2);
    const uint64_t requests_after_addition = state.range(3);
    const bool use_memento = state.range(4);
    
    LoadBalancerPtr lb;
    std::shared_ptr<TestHashPolicy> hash_policy = std::make_shared<TestHashPolicy>();
    
    // Dichiarazione dei tester fuori dal blocco condizionale
    MementoTester memento_tester(num_hosts);
    MaglevTester maglev_tester(num_hosts);
    
    if (use_memento) {
      ASSERT_TRUE(memento_tester.memento_lb_->initialize().ok());
      lb = memento_tester.memento_lb_->factory()->create(memento_tester.lb_params_);
      hash_policy = memento_tester.hash_policy_;
    } else {
      ASSERT_TRUE(maglev_tester.maglev_lb_->initialize().ok());
      lb = maglev_tester.maglev_lb_->factory()->create(maglev_tester.lb_params_);
      hash_policy = maglev_tester.hash_policy_;
    }
    
    absl::node_hash_map<std::string, uint64_t> hit_counter;
    TestLoadBalancerContext context;
    
    // Fase 1: Richieste iniziali
    state.ResumeTiming();
    for (uint64_t i = 0; i < requests_initial; i++) {
      hash_policy->hash_key_ = hashInt(i);
      hit_counter[lb->chooseHost(&context).host->address()->asString()] += 1;
    }
    state.PauseTiming();
    
    // Fase 2: Rimozione host
    if (use_memento) {
      // Per Memento: usa update() incrementale
      Upstream::HostVector reduced_hosts;
      std::shared_ptr<Upstream::MockClusterInfo> info = std::make_shared<NiceMock<Upstream::MockClusterInfo>>();
      for (uint64_t i = 1; i < num_hosts; i++) { // Salta il primo host (indice 0)
        const std::string url = fmt::format("tcp://10.0.{}.{}:6379", i / 256, i % 256);
        reduced_hosts.push_back(Upstream::makeTestHost(info, url, 1));
      }
      
      Upstream::HostVectorConstSharedPtr updated_hosts = std::make_shared<Upstream::HostVector>(reduced_hosts);
      Upstream::HostsPerLocalityConstSharedPtr hosts_per_locality =
          Upstream::makeHostsPerLocality({reduced_hosts});
      
      memento_tester.priority_set_.updateHosts(
          0, Upstream::HostSetImpl::partitionHosts(updated_hosts, hosts_per_locality), {}, reduced_hosts, {},
          absl::nullopt);
      memento_tester.local_priority_set_.updateHosts(
          0, Upstream::HostSetImpl::partitionHosts(updated_hosts, hosts_per_locality), {}, reduced_hosts, {},
          absl::nullopt);
      
      lb = memento_tester.memento_lb_->factory()->create(memento_tester.lb_params_);
      hash_policy = memento_tester.hash_policy_;
    } else {
      // Per Maglev: ricrea tutto
      MaglevTester tester_reduced(num_hosts - 1);
      ASSERT_TRUE(tester_reduced.maglev_lb_->initialize().ok());
      lb = tester_reduced.maglev_lb_->factory()->create(tester_reduced.lb_params_);
      hash_policy = tester_reduced.hash_policy_;
    }
    
    // Fase 3: Richieste dopo rimozione
    state.ResumeTiming();
    for (uint64_t i = requests_initial; i < requests_initial + requests_after_removal; i++) {
      hash_policy->hash_key_ = hashInt(i);
      hit_counter[lb->chooseHost(&context).host->address()->asString()] += 1;
    }
    state.PauseTiming();
    
    // Fase 4: Aggiunta host
    if (use_memento) {
      // Per Memento: usa update() incrementale
      Upstream::HostVector expanded_hosts;
      std::shared_ptr<Upstream::MockClusterInfo> info = std::make_shared<NiceMock<Upstream::MockClusterInfo>>();
      for (uint64_t i = 0; i < num_hosts; i++) { // Ripristina tutti gli host originali
        const std::string url = fmt::format("tcp://10.0.{}.{}:6379", i / 256, i % 256);
        expanded_hosts.push_back(Upstream::makeTestHost(info, url, 1));
      }
      
      Upstream::HostVectorConstSharedPtr updated_hosts = std::make_shared<Upstream::HostVector>(expanded_hosts);
      Upstream::HostsPerLocalityConstSharedPtr hosts_per_locality =
          Upstream::makeHostsPerLocality({expanded_hosts});
      
      memento_tester.priority_set_.updateHosts(
          0, Upstream::HostSetImpl::partitionHosts(updated_hosts, hosts_per_locality), {}, expanded_hosts, {},
          absl::nullopt);
      memento_tester.local_priority_set_.updateHosts(
          0, Upstream::HostSetImpl::partitionHosts(updated_hosts, hosts_per_locality), {}, expanded_hosts, {},
          absl::nullopt);
      
      lb = memento_tester.memento_lb_->factory()->create(memento_tester.lb_params_);
      hash_policy = memento_tester.hash_policy_;
    } else {
      // Per Maglev: ricrea tutto
      MaglevTester tester_expanded(num_hosts);
      ASSERT_TRUE(tester_expanded.maglev_lb_->initialize().ok());
      lb = tester_expanded.maglev_lb_->factory()->create(tester_expanded.lb_params_);
      hash_policy = tester_expanded.hash_policy_;
    }
    
    // Fase 5: Richieste dopo aggiunta
    state.ResumeTiming();
    for (uint64_t i = requests_initial + requests_after_removal; 
         i < requests_initial + requests_after_removal + requests_after_addition; i++) {
      hash_policy->hash_key_ = hashInt(i);
      hit_counter[lb->chooseHost(&context).host->address()->asString()] += 1;
    }
    state.PauseTiming();
    
    computeHitStats(state, hit_counter);
    state.counters["algorithm"] = use_memento ? 1 : 0;
    state.counters["hosts_removed"] = 1;
    state.counters["hosts_added"] = 1;
    state.counters["total_requests"] = requests_initial + requests_after_removal + requests_after_addition;
    state.ResumeTiming();
  }
}
BENCHMARK(benchmarkCompleteScenario)
    // Scenario completo: 20 host, 20 richieste, rimozione 1 host, 10 richieste, aggiunta 1 host, 20 richieste
    ->Args({20, 20, 10, 20, 1})   // 20 host, 20+10+20 richieste, Memento
    ->Args({20, 20, 10, 20, 0})   // 20 host, 20+10+20 richieste, Maglev
    
    // Scenario più intensivo: 100 host, 50 richieste, rimozione 1 host, 25 richieste, aggiunta 1 host, 50 richieste
    ->Args({100, 50, 25, 50, 1})  // 100 host, 50+25+50 richieste, Memento
    ->Args({100, 50, 25, 50, 0})  // 100 host, 50+25+50 richieste, Maglev
    ->Unit(::benchmark::kMillisecond);

} // namespace
} // namespace Upstream
} // namespace Envoy
