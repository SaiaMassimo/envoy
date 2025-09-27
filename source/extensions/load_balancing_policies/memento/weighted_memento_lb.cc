// envoy/source/extensions/load_balancing_policies/memento/weighted_memento_lb.cc
#include "source/extensions/load_balancing_policies/memento/weighted_memento_lb.h"

namespace Envoy {
namespace Extensions {
namespace LoadBalancingPolicies {
namespace Memento {

WeightedMementoLbConfig::WeightedMementoLbConfig(uint64_t table_size, uint32_t weight_scale_factor)
    : table_size_(table_size), weight_scale_factor_(weight_scale_factor) {
}

WeightedMementoLoadBalancer::WeightedMementoLoadBalancer(
    const Upstream::PrioritySet& priority_set,
    Upstream::ClusterLbStats& stats,
    Stats::Scope& /* scope */,
    Runtime::Loader& runtime,
    Random::RandomGenerator& random,
    uint32_t healthy_panic_threshold,
    const WeightedMementoLbConfig& config)
    : ThreadAwareLoadBalancerBase(priority_set, stats, runtime, random, healthy_panic_threshold, false, nullptr),
      config_(config) {
}

Upstream::ThreadAwareLoadBalancerBase::HashingLoadBalancerSharedPtr
WeightedMementoLoadBalancer::createLoadBalancer(
    const Upstream::NormalizedHostWeightVector& normalized_host_weights,
    double /* min_normalized_weight */, 
    double /* max_normalized_weight */) {
  
  if (!table_) {
    table_ = std::make_shared<WeightedMementoTable>(normalized_host_weights, config_.tableSize());
  } else {
    table_->update(normalized_host_weights);
  }
  
  return table_;
}

WeightedMementoTable::Stats WeightedMementoLoadBalancer::getTableStats() const {
  if (table_) {
    return table_->getStats();
  }
  return WeightedMementoTable::Stats{};
}

} // namespace Memento
} // namespace LoadBalancingPolicies
} // namespace Extensions
} // namespace Envoy
