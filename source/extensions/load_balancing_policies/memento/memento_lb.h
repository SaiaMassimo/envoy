// envoy/source/extensions/load_balancing_policies/memento/memento_lb.h
#pragma once

#include "envoy/upstream/load_balancer.h"
#include "source/extensions/load_balancing_policies/common/thread_aware_lb_impl.h"
#include "source/extensions/load_balancing_policies/memento/memento_table.h"
#include "envoy/config/cluster/v3/cluster.pb.h"

namespace Envoy {
namespace Extensions {
namespace LoadBalancingPolicies {
namespace Memento {

// La configurazione specifica per Memento. Per ora, solo la dimensione della tabella.
class MementoLbConfig {
public:
    explicit MementoLbConfig(uint64_t table_size);
    uint64_t tableSize() const { return table_size_; }

private:
    uint64_t table_size_;
};

// MementoLoadBalancer ora eredita da ThreadAwareLoadBalancerBase.
class MementoLoadBalancer : public Upstream::ThreadAwareLoadBalancerBase {
public:
    MementoLoadBalancer(const Upstream::PrioritySet& priority_set,
                        Upstream::ClusterLbStats& stats,
                        Stats::Scope& scope,
                        Runtime::Loader& runtime,
                        Random::RandomGenerator& random,
                        uint32_t healthy_panic_threshold,
                        const MementoLbConfig& config);

private:
    // ThreadAwareLoadBalancerBase
    Upstream::ThreadAwareLoadBalancerBase::HashingLoadBalancerSharedPtr
    createLoadBalancer(const Upstream::NormalizedHostWeightVector& normalized_host_weights,
                       double /* min_normalized_weight */, double /* max_normalized_weight */) override;

    const MementoLbConfig& config_;
    std::shared_ptr<MementoTable> table_;
};

} // namespace Memento
} // namespace LoadBalancingPolicies
} // namespace Extensions
} // namespace Envoy