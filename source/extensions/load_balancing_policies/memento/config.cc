#include "source/extensions/load_balancing_policies/memento/config.h"

namespace Envoy {
namespace Extensions {
namespace LoadBalancingPolices {
namespace Memento {

Upstream::ThreadAwareLoadBalancerPtr Factory::create(
    OptRef<const Upstream::LoadBalancerConfig> lb_config, const Upstream::ClusterInfo& cluster_info,
    const Upstream::PrioritySet& priority_set, Runtime::Loader& runtime, Random::RandomGenerator& random,
    TimeSource& /*time_source*/) {
  const auto* typed = dynamic_cast<const Upstream::TypedHashLbConfigBase*>(lb_config.ptr());
  // Default table size if not provided
  uint64_t table_size = 65537;
  // hash_policy comes from TypedHashLbConfigBase
  auto hash_policy = typed ? typed->hash_policy_ : nullptr;
  // stats scope is not used in MementoLoadBalancer; provide a dummy scope from cluster later if needed
  // Construct lb
  return std::make_unique<Envoy::Extensions::LoadBalancingPolicies::Memento::MementoLoadBalancer>(
      priority_set, const_cast<Upstream::ClusterLbStats&>(cluster_info.lbStats()),
      const_cast<Stats::Scope&>(cluster_info.statsScope()), runtime, random,
      static_cast<uint32_t>(PROTOBUF_PERCENT_TO_ROUNDED_INTEGER_OR_DEFAULT(
          cluster_info.lbConfig(), healthy_panic_threshold, 100, 50)),
      Envoy::Extensions::LoadBalancingPolicies::Memento::MementoLbConfig(table_size));
}

absl::StatusOr<Upstream::LoadBalancerConfigPtr>
Factory::loadConfig(Server::Configuration::ServerFactoryContext& /*context*/, const Protobuf::Message& config) {
  ASSERT(dynamic_cast<const MementoLbProto*>(&config) != nullptr);
  // Non carichiamo una hash policy specifica al momento.
  return std::make_unique<Upstream::TypedHashLbConfigBase>();
}

// Static registration for the Factory. @see RegisterFactory.
REGISTER_FACTORY(Factory, Upstream::TypedLoadBalancerFactory);

} // namespace Memento
} // namespace LoadBalancingPolices
} // namespace Extensions
} // namespace Envoy
