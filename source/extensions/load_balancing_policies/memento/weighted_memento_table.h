// envoy/source/extensions/load_balancing_policies/memento/weighted_memento_table.h
#pragma once

#include "envoy/upstream/load_balancer.h"
#include "source/extensions/load_balancing_policies/common/load_balancer_impl.h"
#include "source/extensions/load_balancing_policies/common/thread_aware_lb_impl.h"
#include "source/extensions/load_balancing_policies/memento/cpuls/MementoWithBinomialEngine.h"

namespace Envoy {
namespace Extensions {
namespace LoadBalancingPolicies {
namespace Memento {

// Classe per gestire i pesi con nodi virtuali in Memento
class WeightedMementoTable : public Upstream::ThreadAwareLoadBalancerBase::HashingLoadBalancer {
public:
  WeightedMementoTable(const Upstream::NormalizedHostWeightVector& normalized_host_weights, 
                      uint64_t table_size);

  // Metodo principale per la selezione dell'host
  Upstream::HostSelectionResponse chooseHost(uint64_t hash, uint32_t attempt) const override;

  // Aggiorna la tabella in base al nuovo insieme di host/weight senza ricreare l'istanza
  void update(const Upstream::NormalizedHostWeightVector& normalized_host_weights);

  // Statistiche per debugging e monitoring
  struct Stats {
    size_t total_virtual_nodes = 0;
    size_t total_physical_hosts = 0;
    size_t max_virtual_nodes_per_host = 0;
    size_t min_virtual_nodes_per_host = 0;
  };
  
  Stats getStats() const;

private:
  // Adattatore per usare l'hash di Envoy con l'interfaccia HashFunction del core cpuls
  class EnvoyHashFunction : public ::HashFunction {
  public:
    int64_t hash(const std::string& key, uint64_t seed = 0) const override;
  };

public:
  // Converte peso normalizzato in peso intero per nodi virtuali
  uint32_t normalizedWeightToInteger(double normalized_weight) const;

private:
  
  // Aggiunge nodi virtuali per un host
  void addVirtualNodes(const Upstream::Host* host, uint32_t weight);
  
  // Rimuove nodi virtuali per un host
  void removeVirtualNodes(const Upstream::Host* host);
  
  // Aggiorna il numero di nodi virtuali per un host
  void updateVirtualNodes(const Upstream::Host* host, uint32_t old_weight, uint32_t new_weight);
  
  // Trova il range di nodi virtuali per un host
  std::pair<size_t, size_t> getVirtualNodeRange(const Upstream::HostConstSharedPtr& host) const;

  // Mappa nodi virtuali -> host fisico
  // L'indice del vettore corrisponde al "bucket" virtuale di Memento
  std::vector<Upstream::HostConstSharedPtr> virtual_to_physical_;
  
  // Mappa host fisico -> range di nodi virtuali [start, end)
  std::unordered_map<const Upstream::Host*, std::pair<size_t, size_t>> physical_to_virtual_range_;
  
  // Engine Memento che lavora sui nodi virtuali
  mutable MementoWithBinomialEngine memento_engine_;
  EnvoyHashFunction hash_function_;
  
  // Pesi attuali per tracking
  std::unordered_map<const Upstream::Host*, uint32_t> current_weights_;
  
  // Configurazione
  static constexpr uint32_t WEIGHT_SCALE_FACTOR = 1000;  // Scala per convertire pesi normalizzati
  static constexpr uint32_t MAX_VIRTUAL_NODES_PER_HOST = 10000;  // Limite per evitare overflow
  static constexpr uint32_t MIN_VIRTUAL_NODES_PER_HOST = 1;  // Minimo per ogni host
};

} // namespace Memento
} // namespace LoadBalancingPolicies
} // namespace Extensions
} // namespace Envoy
