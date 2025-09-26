// envoy/source/extensions/load_balancing_policies/memento/memento_table.h
#pragma once

#include "envoy/upstream/load_balancer.h"
#include "source/extensions/load_balancing_policies/common/load_balancer_impl.h"
#include "source/extensions/load_balancing_policies/common/thread_aware_lb_impl.h"
#include "source/extensions/load_balancing_policies/memento/cpuls/MementoWithBinomialEngine.h"

namespace Envoy {
namespace Extensions {
namespace LoadBalancingPolicies {
namespace Memento {

// Questa classe è l'equivalente della MaglevTable.
// Contiene la logica di Memento e la tabella di host.
class MementoTable : public Upstream::ThreadAwareLoadBalancerBase::HashingLoadBalancer {
public:
  MementoTable(const Upstream::NormalizedHostWeightVector& normalized_host_weights, uint64_t table_size);

  // Metodo principale per la selezione dell'host.
  Upstream::HostSelectionResponse chooseHost(uint64_t hash, uint32_t attempt) const override;

  // Aggiorna la tabella in base al nuovo insieme di host/weight senza ricreare l'istanza.
  void update(const Upstream::NormalizedHostWeightVector& normalized_host_weights);

private:
  // Adattatore per usare l'hash di Envoy con l'interfaccia HashFunction del core cpuls.
  class EnvoyHashFunction : public ::HashFunction {
  public:
    int64_t hash(const std::string& key, uint64_t seed = 0) const override;
  };

  // Vettore che mappa gli indici di Memento agli host di Envoy.
  std::vector<Upstream::HostConstSharedPtr> host_table_;

  // L'istanza del tuo algoritmo.
  mutable MementoWithBinomialEngine memento_engine_;
  EnvoyHashFunction hash_function_;
};

} // namespace Memento
} // namespace LoadBalancingPolicies
} // namespace Extensions
} // namespace Envoy