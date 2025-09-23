// envoy/source/extensions/load_balancing_policies/memento/memento_lb.cc
#include "source/extensions/load_balancing_policies/memento/memento_lb.h"
#include "source/extensions/load_balancing_policies/common/load_balancer_impl.h"

namespace Envoy {
namespace Extensions {
namespace LoadBalancingPolicies {
namespace Memento {

// --- Implementazione di MementoLbConfig ---
MementoLbConfig::MementoLbConfig(uint64_t table_size) { table_size_ = table_size; }

// --- Implementazione di MementoLoadBalancer ---
MementoLoadBalancer::MementoLoadBalancer(
    const Upstream::PrioritySet& priority_set,
    Upstream::ClusterLbStats& stats,
    Stats::Scope& scope,
    Runtime::Loader& runtime,
    Random::RandomGenerator& random,
    uint32_t healthy_panic_threshold,
    const MementoLbConfig& config)
    : ThreadAwareLoadBalancerBase(priority_set, stats, runtime, random, healthy_panic_threshold,
                                  /*locality_weighted_balancing=*/false, /*hash_policy=*/nullptr),
      config_(config) {
  (void)scope;
}

Upstream::ThreadAwareLoadBalancerBase::HashingLoadBalancerSharedPtr
MementoLoadBalancer::createLoadBalancer(const Upstream::NormalizedHostWeightVector& normalized_host_weights,
                                        double /* min_normalized_weight */, double /* max_normalized_weight */) {
  if (!table_) {
    table_ = std::make_shared<MementoTable>(normalized_host_weights, config_.tableSize());
  } else {
    table_->update(normalized_host_weights);
  }
  return table_;
}

} // namespace Memento
} // namespace LoadBalancingPolicies
} // namespace Extensions
} // namespace Envoy