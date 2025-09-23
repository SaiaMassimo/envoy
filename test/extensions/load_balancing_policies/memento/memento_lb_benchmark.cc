#include "source/extensions/load_balancing_policies/memento/memento_lb.h"

#include "test/benchmark/main.h"
#include "test/extensions/load_balancing_policies/common/benchmark_base_tester.h"

namespace Envoy {
namespace Upstream {
namespace {

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

void benchmarkMementoLoadBalancerChooseHost(::benchmark::State& state) {
  for (auto _ : state) {
    state.PauseTiming();
    const uint64_t num_hosts = state.range(0);
    const uint64_t keys_to_simulate = state.range(1);
    MementoTester tester(num_hosts);
    ASSERT_TRUE(tester.memento_lb_->initialize().ok());
    LoadBalancerPtr lb = tester.memento_lb_->factory()->create(tester.lb_params_);
    absl::node_hash_map<std::string, uint64_t> hit_counter;
    TestLoadBalancerContext context;
    state.ResumeTiming();

    for (uint64_t i = 0; i < keys_to_simulate; i++) {
      tester.hash_policy_->hash_key_ = hashInt(i);
      hit_counter[lb->chooseHost(&context).host->address()->asString()] += 1;
    }

    state.PauseTiming();
    computeHitStats(state, hit_counter);
    state.ResumeTiming();
  }
}
BENCHMARK(benchmarkMementoLoadBalancerChooseHost)
    ->Args({100, 100000})
    ->Args({200, 100000})
    ->Args({500, 100000})
    ->Unit(::benchmark::kMillisecond);

void benchmarkMementoLoadBalancerBuildTable(::benchmark::State& state) {
  for (auto _ : state) {
    state.PauseTiming();
    const uint64_t num_hosts = state.range(0);
    MementoTester tester(num_hosts);

    const size_t start_mem = Memory::Stats::totalCurrentlyAllocated();

    state.ResumeTiming();
    ASSERT_TRUE(tester.memento_lb_->initialize().ok());
    state.PauseTiming();
    const size_t end_mem = Memory::Stats::totalCurrentlyAllocated();
    state.counters["memory"] = end_mem - start_mem;
    state.counters["memory_per_host"] = (end_mem - start_mem) / num_hosts;
    state.ResumeTiming();
  }
}
BENCHMARK(benchmarkMementoLoadBalancerBuildTable)
    ->Arg(100)
    ->Arg(200)
    ->Arg(500)
    ->Unit(::benchmark::kMillisecond);

void benchmarkMementoLoadBalancerHostLoss(::benchmark::State& state) {
  for (auto _ : state) {
    const uint64_t num_hosts = state.range(0);
    const uint64_t hosts_to_lose = state.range(1);
    const uint64_t keys_to_simulate = state.range(2);

    MementoTester tester(num_hosts);
    ASSERT_TRUE(tester.memento_lb_->initialize().ok());
    LoadBalancerPtr lb = tester.memento_lb_->factory()->create(tester.lb_params_);
    std::vector<HostConstSharedPtr> hosts;
    TestLoadBalancerContext context;
    for (uint64_t i = 0; i < keys_to_simulate; i++) {
      tester.hash_policy_->hash_key_ = hashInt(i);
      hosts.push_back(lb->chooseHost(&context).host);
    }

    MementoTester tester2(num_hosts - hosts_to_lose);
    ASSERT_TRUE(tester2.memento_lb_->initialize().ok());
    lb = tester2.memento_lb_->factory()->create(tester2.lb_params_);
    std::vector<HostConstSharedPtr> hosts2;
    for (uint64_t i = 0; i < keys_to_simulate; i++) {
      tester.hash_policy_->hash_key_ = hashInt(i);
      hosts2.push_back(lb->chooseHost(&context).host);
    }

    ASSERT(hosts.size() == hosts2.size());
    uint64_t num_different_hosts = 0;
    for (uint64_t i = 0; i < hosts.size(); i++) {
      if (hosts[i]->address()->asString() != hosts2[i]->address()->asString()) {
        num_different_hosts++;
      }
    }

    state.counters["percent_different"] =
        (static_cast<double>(num_different_hosts) / hosts.size()) * 100;
    state.counters["host_loss_over_N_optimal"] =
        (static_cast<double>(hosts_to_lose) / num_hosts) * 100;
  }
}
BENCHMARK(benchmarkMementoLoadBalancerHostLoss)
    ->Args({500, 1, 10000})
    ->Args({500, 2, 10000})
    ->Args({500, 3, 10000})
    ->Unit(::benchmark::kMillisecond);

} // namespace
} // namespace Upstream
} // namespace Envoy


