#pragma once

#include <memory>

#include "envoy/extensions/load_balancing_policies/memento/v3/memento.pb.h"
#include "envoy/extensions/load_balancing_policies/memento/v3/memento.pb.validate.h"
#include "envoy/upstream/load_balancer.h"

#include "source/common/upstream/load_balancer_factory_base.h"
#include "source/extensions/load_balancing_policies/common/factory_base.h"
#include "source/extensions/load_balancing_policies/memento/memento_lb.h"

namespace Envoy {
namespace Extensions {
namespace LoadBalancingPolices {
namespace Memento {

using MementoLbProto = envoy::extensions::load_balancing_policies::memento::v3::Memento;

class Factory : public Upstream::TypedLoadBalancerFactoryBase<MementoLbProto> {
public:
  Factory() : TypedLoadBalancerFactoryBase("envoy.load_balancing_policies.memento") {}

  Upstream::ThreadAwareLoadBalancerPtr create(OptRef<const Upstream::LoadBalancerConfig> lb_config,
                                              const Upstream::ClusterInfo& cluster_info,
                                              const Upstream::PrioritySet& priority_set,
                                              Runtime::Loader& runtime,
                                              Random::RandomGenerator& random,
                                              TimeSource& /*time_source*/) override;

  absl::StatusOr<Upstream::LoadBalancerConfigPtr>
  loadConfig(Server::Configuration::ServerFactoryContext& context,
             const Protobuf::Message& config) override;
};

} // namespace Memento
} // namespace LoadBalancingPolices
} // namespace Extensions
} // namespace Envoy


