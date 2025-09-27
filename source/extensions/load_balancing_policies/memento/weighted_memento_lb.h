// envoy/source/extensions/load_balancing_policies/memento/weighted_memento_lb.h
#pragma once

#include "envoy/upstream/load_balancer.h"
#include "source/extensions/load_balancing_policies/common/thread_aware_lb_impl.h"
#include "source/extensions/load_balancing_policies/memento/weighted_memento_table.h"
#include "envoy/config/cluster/v3/cluster.pb.h"

namespace Envoy {
namespace Extensions {
namespace LoadBalancingPolicies {
namespace Memento {

// La configurazione specifica per WeightedMemento. Estende MementoLbConfig con opzioni per i pesi.
class WeightedMementoLbConfig {
public:
    explicit WeightedMementoLbConfig(uint64_t table_size, uint32_t weight_scale_factor = 1000);
    
    uint64_t tableSize() const { return table_size_; }
    uint32_t weightScaleFactor() const { return weight_scale_factor_; }

private:
    uint64_t table_size_;
    uint32_t weight_scale_factor_;
};

// WeightedMementoLoadBalancer che supporta pesi con nodi virtuali
class WeightedMementoLoadBalancer : public Upstream::ThreadAwareLoadBalancerBase {
public:
    WeightedMementoLoadBalancer(const Upstream::PrioritySet& priority_set,
                               Upstream::ClusterLbStats& stats,
                               Stats::Scope& scope,
                               Runtime::Loader& runtime,
                               Random::RandomGenerator& random,
                               uint32_t healthy_panic_threshold,
                               const WeightedMementoLbConfig& config);

    // Metodo per ottenere statistiche del load balancer
    WeightedMementoTable::Stats getTableStats() const;

private:
    // ThreadAwareLoadBalancerBase
    Upstream::ThreadAwareLoadBalancerBase::HashingLoadBalancerSharedPtr
    createLoadBalancer(const Upstream::NormalizedHostWeightVector& normalized_host_weights,
                      double /* min_normalized_weight */, double /* max_normalized_weight */) override;

    const WeightedMementoLbConfig& config_;
    std::shared_ptr<WeightedMementoTable> table_;
};

} // namespace Memento
} // namespace LoadBalancingPolicies
} // namespace Extensions
} // namespace Envoy
