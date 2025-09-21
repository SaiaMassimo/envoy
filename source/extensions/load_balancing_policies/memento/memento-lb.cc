// envoy/source/extensions/load_balancing_policies/memento/memento_lb.cc
#include "source/extensions/load_balancing_policies/memento/memento_lb.h"
#include "source/common/upstream/load_balancer_impl.h"

namespace Envoy {
namespace Extensions {
namespace LoadBalancingPolicies {
namespace Memento {

// --- Implementazione di MementoLbConfig ---
MementoLbConfig::MementoLbConfig(const envoy::config::cluster::v3::Cluster::MementoLbConfig& config) {
    // Imposta una dimensione di tabella predefinita se non specificata, come fa Maglev.
    // NOTA: La dimensione della tabella per Memento non è un requisito, a differenza di Maglev.
    // Questo è solo un esempio di come potresti passare la configurazione.
    table_size_ = PROTOBUF_GET_WRAPPED_OR_DEFAULT(config, table_size, 65537);
}

// --- Implementazione di MementoLoadBalancer ---
MementoLoadBalancer::MementoLoadBalancer(
    const Upstream::PrioritySet& priority_set,
    Upstream::ClusterLbStats& stats,
    Stats::Scope& scope,
    Runtime::Loader& runtime,
    Random::RandomGenerator& random,
    uint32_t healthy_panic_threshold,
    const MementoLbConfig& config)
    : ThreadAwareLoadBalancerBase(priority_set, stats, runtime, random, healthy_panic_threshold, absl::nullopt),
      config_(config) {
    factory_ = std::make_shared<MementoLbFactory>(*this, config_.tableSize());
}

// --- Implementazione di MementoLbFactory ---
Upstream::LoadBalancerPtr MementoLoadBalancer::MementoLbFactory::create(Upstream::LoadBalancerParamsConstSharedPtr params) {
    // Questa funzione viene chiamata per creare un'istanza del LB per ogni worker thread.
    const auto& hosts = params->host_set.healthyHosts();
    if (hosts.empty()) {
        return std::make_unique<Upstream::DeterministicLoadBalancer>(params->host_set, nullptr);
    }

    Upstream::NormalizedHostWeightVector normalized_weights;
    normalized_weights.reserve(hosts.size());
    for (const auto& host : hosts) {
        normalized_weights.emplace_back(host, 1.0); // Memento non usa i pesi
    }

    auto table = std::make_shared<MementoTable>(normalized_weights, table_size_);
    return std::make_unique<Upstream::DeterministicLoadBalancer>(params->host_set, std::move(table));
}

} // namespace Memento
} // namespace LoadBalancingPolicies
} // namespace Extensions
} // namespace Envoy