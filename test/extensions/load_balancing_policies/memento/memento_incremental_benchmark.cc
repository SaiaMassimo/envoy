#include "source/extensions/load_balancing_policies/memento/memento_lb.h"

#include "test/benchmark/main.h"
#include "test/extensions/load_balancing_policies/common/benchmark_base_tester.h"

namespace Envoy {
namespace Upstream {
namespace {

class MementoIncrementalTester : public BaseTester {
public:
  MementoIncrementalTester(uint64_t num_hosts, uint32_t weighted_subset_percent = 0, uint32_t weight = 0)
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

// Benchmark 1: Performance degli aggiornamenti incrementali weighted
void benchmarkIncrementalWeightedUpdates(::benchmark::State& state) {
  for (auto _ : state) {
    state.PauseTiming();
    const uint64_t num_hosts = state.range(0);
    const uint64_t updates_to_perform = state.range(1);
    const uint64_t keys_to_simulate = state.range(2);
    
    MementoIncrementalTester tester(num_hosts);
    ASSERT_TRUE(tester.memento_lb_->initialize().ok());
    LoadBalancerPtr lb = tester.memento_lb_->factory()->create(tester.lb_params_);
    
    // Test iniziale per baseline
    absl::node_hash_map<std::string, uint64_t> initial_hit_counter;
    TestLoadBalancerContext context;
    
    for (uint64_t i = 0; i < keys_to_simulate; i++) {
      tester.hash_policy_->hash_key_ = hashInt(i);
      initial_hit_counter[lb->chooseHost(&context).host->address()->asString()] += 1;
    }
    
    state.ResumeTiming();
    
    // Esegui aggiornamenti incrementali weighted
    for (uint64_t update = 0; update < updates_to_perform; update++) {
      // Simula aggiunta di un nuovo host con peso diverso
      if (update % 2 == 0) {
        // Aggiungi host con peso alto
        auto new_host = makeTestHost(tester.info_, "tcp://127.0.0.1:" + std::to_string(9000 + update), 127);
        auto current_hosts = tester.priority_set_.hostSetsPerPriority()[0]->hostsPtr();
        auto updated_hosts = std::make_shared<HostVector>(*current_hosts);
        updated_hosts->push_back(new_host);
        auto params = HostSetImpl::updateHostsParams(*tester.priority_set_.hostSetsPerPriority()[0]);
        tester.priority_set_.updateHosts(0, std::move(params), nullptr, {new_host}, {}, absl::nullopt, absl::nullopt, nullptr);
      } else {
        // Rimuovi host (se ce ne sono abbastanza)
        auto current_hosts = tester.priority_set_.hostSetsPerPriority()[0]->hostsPtr();
        if (current_hosts->size() > 1) {
          auto host_to_remove = current_hosts->back();
          auto params = HostSetImpl::updateHostsParams(*tester.priority_set_.hostSetsPerPriority()[0]);
          tester.priority_set_.updateHosts(0, std::move(params), nullptr, {}, {host_to_remove}, absl::nullopt, absl::nullopt, nullptr);
        }
      }
      
      // Aggiorna il load balancer in-place
      lb = tester.memento_lb_->factory()->create(tester.lb_params_);
    }
    
    state.PauseTiming();
    
    // Test finale per verificare che tutto funzioni ancora
    absl::node_hash_map<std::string, uint64_t> final_hit_counter;
    for (uint64_t i = 0; i < keys_to_simulate; i++) {
      tester.hash_policy_->hash_key_ = hashInt(i);
      final_hit_counter[lb->chooseHost(&context).host->address()->asString()] += 1;
    }
    
    // Calcola statistiche
    state.counters["initial_hosts"] = initial_hit_counter.size();
    state.counters["final_hosts"] = final_hit_counter.size();
    state.counters["updates_performed"] = updates_to_perform;
    
    state.ResumeTiming();
  }
}
BENCHMARK(benchmarkIncrementalWeightedUpdates)
    ->Args({100, 10, 10000})   // 100 host, 10 aggiornamenti, 10k chiavi
    ->Args({200, 20, 10000})   // 200 host, 20 aggiornamenti, 10k chiavi
    ->Args({500, 50, 10000})   // 500 host, 50 aggiornamenti, 10k chiavi
    ->Args({1000, 100, 10000}) // 1000 host, 100 aggiornamenti, 10k chiavi
    ->Unit(::benchmark::kMillisecond);

// Benchmark 2: Confronto aggiornamenti incrementali vs ricostruzione completa
void benchmarkIncrementalVsFullRebuild(::benchmark::State& state) {
  for (auto _ : state) {
    state.PauseTiming();
    const uint64_t num_hosts = state.range(0);
    const uint64_t updates_to_perform = state.range(1);
    const bool use_incremental = state.range(2);
    
    MementoIncrementalTester tester(num_hosts);
    ASSERT_TRUE(tester.memento_lb_->initialize().ok());
    LoadBalancerPtr lb = tester.memento_lb_->factory()->create(tester.lb_params_);
    
    state.ResumeTiming();
    
    for (uint64_t update = 0; update < updates_to_perform; update++) {
      if (use_incremental) {
        // Aggiornamento incrementale: riutilizza la tabella esistente
        lb = tester.memento_lb_->factory()->create(tester.lb_params_);
      } else {
        // Ricostruzione completa: crea nuovo tester e load balancer
        MementoIncrementalTester new_tester(num_hosts);
        ASSERT_TRUE(new_tester.memento_lb_->initialize().ok());
        lb = new_tester.memento_lb_->factory()->create(new_tester.lb_params_);
      }
    }
    
    state.PauseTiming();
    state.counters["method"] = use_incremental ? 1 : 0; // 1 = incremental, 0 = full rebuild
    state.counters["updates_performed"] = updates_to_perform;
    state.ResumeTiming();
  }
}
BENCHMARK(benchmarkIncrementalVsFullRebuild)
    ->Args({100, 10, 1})   // 100 host, 10 aggiornamenti, incremental
    ->Args({100, 10, 0})   // 100 host, 10 aggiornamenti, full rebuild
    ->Args({200, 20, 1})   // 200 host, 20 aggiornamenti, incremental
    ->Args({200, 20, 0})   // 200 host, 20 aggiornamenti, full rebuild
    ->Args({500, 50, 1})   // 500 host, 50 aggiornamenti, incremental
    ->Args({500, 50, 0})   // 500 host, 50 aggiornamenti, full rebuild
    ->Unit(::benchmark::kMillisecond);

// Benchmark 3: Performance degli aggiornamenti di peso
void benchmarkWeightUpdates(::benchmark::State& state) {
  for (auto _ : state) {
    state.PauseTiming();
    const uint64_t num_hosts = state.range(0);
    const uint64_t weight_updates = state.range(1);
    const uint64_t keys_to_simulate = state.range(2);
    
    MementoIncrementalTester tester(num_hosts);
    ASSERT_TRUE(tester.memento_lb_->initialize().ok());
    LoadBalancerPtr lb = tester.memento_lb_->factory()->create(tester.lb_params_);
    
    state.ResumeTiming();
    
    // Esegui aggiornamenti di peso
    for (uint64_t update = 0; update < weight_updates; update++) {
      // Simula cambio di peso per un host esistente
      auto current_hosts = tester.priority_set_.hostSetsPerPriority()[0]->hostsPtr();
      if (!current_hosts->empty()) {
        auto host_to_update = current_hosts->front();
        // Cambia il peso dell'host (simula aggiornamento)
        auto updated_host = makeTestHost(tester.info_, host_to_update->address()->asString(), 
                                       (update % 2 == 0) ? 127 : 1);
        auto params = HostSetImpl::updateHostsParams(*tester.priority_set_.hostSetsPerPriority()[0]);
        tester.priority_set_.updateHosts(0, std::move(params), nullptr, {updated_host}, {host_to_update}, absl::nullopt, absl::nullopt, nullptr);
        
        // Aggiorna il load balancer
        lb = tester.memento_lb_->factory()->create(tester.lb_params_);
      }
    }
    
    state.PauseTiming();
    
    // Test finale per verificare distribuzione
    absl::node_hash_map<std::string, uint64_t> final_hit_counter;
    TestLoadBalancerContext context;
    for (uint64_t i = 0; i < keys_to_simulate; i++) {
      tester.hash_policy_->hash_key_ = hashInt(i);
      final_hit_counter[lb->chooseHost(&context).host->address()->asString()] += 1;
    }
    
    computeHitStats(state, final_hit_counter);
    state.counters["weight_updates"] = weight_updates;
    
    state.ResumeTiming();
  }
}
BENCHMARK(benchmarkWeightUpdates)
    ->Args({100, 10, 10000})   // 100 host, 10 aggiornamenti peso, 10k chiavi
    ->Args({200, 20, 10000})   // 200 host, 20 aggiornamenti peso, 10k chiavi
    ->Args({500, 50, 10000})   // 500 host, 50 aggiornamenti peso, 10k chiavi
    ->Unit(::benchmark::kMillisecond);

// Benchmark 4: Consistenza durante aggiornamenti incrementali
void benchmarkIncrementalConsistency(::benchmark::State& state) {
  for (auto _ : state) {
    state.PauseTiming();
    const uint64_t num_hosts = state.range(0);
    const uint64_t keys_to_test = state.range(1);
    
    MementoIncrementalTester tester(num_hosts);
    ASSERT_TRUE(tester.memento_lb_->initialize().ok());
    LoadBalancerPtr lb = tester.memento_lb_->factory()->create(tester.lb_params_);
    
    // Salva le selezioni iniziali
    std::vector<std::string> initial_selections;
    TestLoadBalancerContext context;
    
    for (uint64_t i = 0; i < keys_to_test; i++) {
      tester.hash_policy_->hash_key_ = hashInt(i);
      initial_selections.push_back(lb->chooseHost(&context).host->address()->asString());
    }
    
    state.ResumeTiming();
    
    // Esegui aggiornamenti e verifica consistenza
    uint64_t consistent_selections = 0;
    for (uint64_t update = 0; update < 10; update++) {
      // Aggiungi un host
      auto new_host = makeTestHost(tester.info_, "tcp://127.0.0.1:" + std::to_string(9000 + update), 1);
      auto params = HostSetImpl::updateHostsParams(*tester.priority_set_.hostSetsPerPriority()[0]);
      tester.priority_set_.updateHosts(0, std::move(params), nullptr, {new_host}, {}, absl::nullopt, absl::nullopt, nullptr);
      
      // Aggiorna load balancer
      lb = tester.memento_lb_->factory()->create(tester.lb_params_);
      
      // Verifica consistenza per un subset di chiavi
      for (uint64_t i = 0; i < keys_to_test / 20; i++) {
        tester.hash_policy_->hash_key_ = hashInt(i);
        std::string current_selection = lb->chooseHost(&context).host->address()->asString();
        
        // Verifica se la selezione è ancora valida (host esiste ancora)
        bool host_still_exists = false;
        auto current_hosts = tester.priority_set_.hostSetsPerPriority()[0]->hostsPtr();
        for (const auto& host : *current_hosts) {
          if (host->address()->asString() == current_selection) {
            host_still_exists = true;
            break;
          }
        }
        
        if (host_still_exists) {
          consistent_selections++;
        }
      }
    }
    
    state.PauseTiming();
    
    state.counters["consistency_rate"] = static_cast<double>(consistent_selections) / (keys_to_test / 20 * 10);
    state.counters["total_checks"] = keys_to_test / 20 * 10;
    
    state.ResumeTiming();
  }
}
BENCHMARK(benchmarkIncrementalConsistency)
    ->Args({100, 1000})   // 100 host, 1000 chiavi da testare
    ->Args({200, 2000})   // 200 host, 2000 chiavi da testare
    ->Args({500, 5000})   // 500 host, 5000 chiavi da testare
    ->Unit(::benchmark::kMillisecond);

// Benchmark 5: Performance con diversi tipi di aggiornamenti
void benchmarkMixedUpdates(::benchmark::State& state) {
  for (auto _ : state) {
    state.PauseTiming();
    const uint64_t num_hosts = state.range(0);
    const uint64_t updates_to_perform = state.range(1);
    const uint64_t keys_to_simulate = state.range(2);
    
    MementoIncrementalTester tester(num_hosts);
    ASSERT_TRUE(tester.memento_lb_->initialize().ok());
    LoadBalancerPtr lb = tester.memento_lb_->factory()->create(tester.lb_params_);
    
    state.ResumeTiming();
    
    // Esegui aggiornamenti misti (aggiunta, rimozione, cambio peso)
    for (uint64_t update = 0; update < updates_to_perform; update++) {
      auto current_hosts = tester.priority_set_.hostSetsPerPriority()[0]->hostsPtr();
      
      switch (update % 3) {
        case 0: {
          // Aggiungi host
          auto new_host = makeTestHost(tester.info_, "tcp://127.0.0.1:" + std::to_string(9000 + update), 1);
          auto params = HostSetImpl::updateHostsParams(*tester.priority_set_.hostSetsPerPriority()[0]);
          tester.priority_set_.updateHosts(0, std::move(params), nullptr, {new_host}, {}, absl::nullopt, absl::nullopt, nullptr);
          break;
        }
        case 1: {
          // Rimuovi host (se ce ne sono abbastanza)
          if (current_hosts->size() > 1) {
            auto host_to_remove = current_hosts->back();
            auto params = HostSetImpl::updateHostsParams(*tester.priority_set_.hostSetsPerPriority()[0]);
            tester.priority_set_.updateHosts(0, std::move(params), nullptr, {}, {host_to_remove}, absl::nullopt, absl::nullopt, nullptr);
          }
          break;
        }
        case 2: {
          // Cambia peso di un host esistente
          if (!current_hosts->empty()) {
            auto host_to_update = current_hosts->front();
            auto updated_host = makeTestHost(tester.info_, host_to_update->address()->asString(), 
                                           (update % 2 == 0) ? 127 : 1);
            auto params = HostSetImpl::updateHostsParams(*tester.priority_set_.hostSetsPerPriority()[0]);
            tester.priority_set_.updateHosts(0, std::move(params), nullptr, {updated_host}, {host_to_update}, absl::nullopt, absl::nullopt, nullptr);
          }
          break;
        }
      }
      
      // Aggiorna il load balancer
      lb = tester.memento_lb_->factory()->create(tester.lb_params_);
    }
    
    state.PauseTiming();
    
    // Test finale per verificare distribuzione
    absl::node_hash_map<std::string, uint64_t> final_hit_counter;
    TestLoadBalancerContext context;
    for (uint64_t i = 0; i < keys_to_simulate; i++) {
      tester.hash_policy_->hash_key_ = hashInt(i);
      final_hit_counter[lb->chooseHost(&context).host->address()->asString()] += 1;
    }
    
    computeHitStats(state, final_hit_counter);
    state.counters["updates_performed"] = updates_to_perform;
    
    state.ResumeTiming();
  }
}
BENCHMARK(benchmarkMixedUpdates)
    ->Args({100, 15, 10000})   // 100 host, 15 aggiornamenti misti, 10k chiavi
    ->Args({200, 30, 10000})   // 200 host, 30 aggiornamenti misti, 10k chiavi
    ->Args({500, 75, 10000})   // 500 host, 75 aggiornamenti misti, 10k chiavi
    ->Unit(::benchmark::kMillisecond);

} // namespace
} // namespace Upstream
} // namespace Envoy
