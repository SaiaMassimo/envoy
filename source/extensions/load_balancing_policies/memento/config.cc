#include "source/extensions/load_balancing_policies/memento/config.h"

namespace Envoy {
namespace Extensions {
namespace LoadBalancingPolices {
namespace Memento {

Upstream::ThreadAwareLoadBalancerPtr Factory::create(
    OptRef<const Upstream::LoadBalancerConfig> lb_config, const Upstream::ClusterInfo& /*cluster_info*/,
    const Upstream::PrioritySet& priority_set, Runtime::Loader& runtime, Random::RandomGenerator& random,
    TimeSource& /*time_source*/) {
  const auto* typed = lb_config ? dynamic_cast<const Upstream::TypedHashLbConfigBase*>(&lb_config->get()) : nullptr;
  // Default table size if not provided
  uint64_t table_size = 65537;
  // hash_policy comes from TypedHashLbConfigBase
  auto hash_policy = typed ? typed->hash_policy_ : nullptr;
  // stats scope is not used in MementoLoadBalancer; provide a dummy scope from cluster later if needed
  // Construct lb
  return std::make_unique<Memento::MementoLoadBalancer>(priority_set, const_cast<Upstream::ClusterLbStats&>(priority_set.cluster_info()->stats()),
                                                        const_cast<Stats::Scope&>(priority_set.cluster_info()->statsScope()), runtime, random,
                                                        priority_set.cluster_info()->lbHealthyPanicThreshold(), Memento::MementoLbConfig(table_size));
}

absl::StatusOr<Upstream::LoadBalancerConfigPtr>
Factory::loadConfig(Server::Configuration::ServerFactoryContext& context, const Protobuf::Message& config) {
  ASSERT(dynamic_cast<const MementoLbProto*>(&config) != nullptr);
  const MementoLbProto& typed_proto = dynamic_cast<const MementoLbProto&>(config);
  absl::Status creation_status = absl::OkStatus();
  // For now, we only need the hash policy via TypedHashLbConfigBase
  auto typed_config = std::make_unique<Upstream::TypedHashLbConfigBase>(
      typed_proto.consistent_hashing_lb_config().hash_policy(), context.regexEngine(), creation_status);
  RETURN_IF_NOT_OK_REF(creation_status);
  return typed_config;
}

} // namespace Memento
} // namespace LoadBalancingPolices
} // namespace Extensions
} // namespace Envoy


