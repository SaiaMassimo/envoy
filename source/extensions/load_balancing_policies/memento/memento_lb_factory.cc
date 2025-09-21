// envoy/source/extensions/load_balancing_policies/memento/memento_lb_factory.cc
#include "source/extensions/load_balancing_policies/memento/memento_lb.h"
#include "source/extensions/load_balancing_policies/memento/memento_lb_config.h"

#include "envoy/extensions/load_balancing_policies/memento/v3/memento.pb.h"
#include "envoy/extensions/load_balancing_policies/memento/v3/memento.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace LoadBalancingPolicies {
namespace Memento {

class MementoLoadBalancerFactory : public Upstream::TypedLoadBalancerFactory {
public:
    std::string name() const override { return "envoy.load_balancing_policies.memento_lb"; }

    Upstream::ThreadAwareLoadBalancerPtr create(const Upstream::ClusterInfo& cluster,
                                                const Upstream::PrioritySet& priority_set,
                                                Upstream::ClusterLbStats& stats,
                                                Runtime::Loader& runtime,
                                                Random::RandomGenerator& random,
                                                const envoy::config::cluster::v3::Cluster::CommonLbConfig&,
                                                const Protobuf::Message* lb_config,
                                                Server::Configuration::ServerFactoryContext&,
                                                Server::Configuration::TransportSocketFactoryContext&) override {
        // Converte il messaggio Protobuf generico nella nostra configurazione specifica.
        auto memento_config = MessageUtil::downcastAndValidate<const envoy::extensions::load_balancing_policies::memento::v3::MementoLbConfig&>(
            *lb_config, ProtobufMessage::getStrictValidationVisitor());

        // Crea l'istanza principale del load balancer.
        return std::make_unique<MementoLoadBalancer>(
            priority_set, stats, *cluster.info()->statsScope(), runtime, random,
            cluster.info()->panicThreshold(), memento_config);
    }

    ProtobufTypes::MessagePtr createLbConfigProto() override {
        return std::make_unique<envoy::extensions::load_balancing_policies::memento::v3::MementoLbConfig>();
    }
};

// Registra la factory in Envoy.
REGISTER_FACTORY(MementoLoadBalancerFactory, Upstream::TypedLoadBalancerFactory);

} // namespace Memento
} // namespace LoadBalancingPolicies
} // namespace Extensions
} // namespace Envoy