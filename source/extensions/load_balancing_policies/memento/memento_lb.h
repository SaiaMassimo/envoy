// envoy/source/extensions/load_balancing_policies/memento/memento_lb.h
#pragma once

#include "envoy/upstream/load_balancer.h"
#include "source/common/upstream/thread_aware_lb_impl.h"
#include "source/extensions/load_balancing_policies/memento/memento_table.h"
#include "envoy/config/cluster/v3/cluster.pb.h"

namespace Envoy {
namespace Extensions {
namespace LoadBalancingPolicies {
namespace Memento {

// La configurazione specifica per Memento. Per ora, solo la dimensione della tabella.
class MementoLbConfig {
public:
    MementoLbConfig(const envoy::config::cluster::v3::Cluster::MementoLbConfig& config);
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
    // Factory per creare un MementoTable per ogni thread.
    class MementoLbFactory : public Upstream::LoadBalancerFactory {
    public:
        MementoLbFactory(const MementoLoadBalancer& parent, uint64_t table_size)
            : parent_(parent), table_size_(table_size) {}

        Upstream::LoadBalancerPtr create(Upstream::LoadBalancerParamsConstSharedPtr params) override;

    private:
        const MementoLoadBalancer& parent_;
        const uint64_t table_size_;
    };

    // Implementazione di ThreadAwareLoadBalancerBase
    Upstream::LoadBalancerFactorySharedPtr factory() override { return factory_; }

    const MementoLbConfig& config_;
    Upstream::LoadBalancerFactorySharedPtr factory_;
};

} // namespace Memento
} // namespace LoadBalancingPolicies
} // namespace Extensions
} // namespace Envoy