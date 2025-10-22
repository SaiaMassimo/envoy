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

} // namespace
} // namespace Upstream
} // namespace Envoy
