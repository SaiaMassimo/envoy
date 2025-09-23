// envoy/source/extensions/load_balancing_policies/memento/memento_table.h
#pragma once

#include "envoy/upstream/load_balancer.h"
#include "source/extensions/load_balancing_policies/common/load_balancer_impl.h"
#include "source/extensions/load_balancing_policies/memento/cpuls/MementoWithBinomialEngine.h"

namespace Envoy {
namespace Extensions {
namespace LoadBalancingPolicies {
namespace Memento {

// Questa classe è l'equivalente della MaglevTable.
// Contiene la logica di Memento e la tabella di host.
class MementoTable : public Upstream::HashingLoadBalancer {
public:
  MementoTable(const Upstream::NormalizedHostWeightVector& normalized_host_weights, uint64_t table_size);

  // Metodo principale per la selezione dell'host.
  Upstream::HostConstSharedPtr chooseHost(uint64_t hash, uint32_t attempt) const override;

private:
  // Funzione hash richiesta da MementoWithBinomialEngine.
  // Puoi adattarla o renderla più robusta.
  static uint64_t mementoHash(const std::string& key, int i) {
      return HashUtil::xxHash64(key, i);
  }

  // Vettore che mappa gli indici di Memento agli host di Envoy.
  std::vector<Upstream::HostConstSharedPtr> host_table_;

  // L'istanza del tuo algoritmo.
  mutable MementoWithBinomialEngine memento_engine_;
};

} // namespace Memento
} // namespace LoadBalancingPolicies
} // namespace Extensions
} // namespace Envoy