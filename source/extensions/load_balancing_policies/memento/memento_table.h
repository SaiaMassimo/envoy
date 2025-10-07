// envoy/source/extensions/load_balancing_policies/memento/memento_table.h
#pragma once

#include "envoy/upstream/load_balancer.h"
#include "source/extensions/load_balancing_policies/common/load_balancer_impl.h"
#include "source/extensions/load_balancing_policies/common/thread_aware_lb_impl.h"
#include "source/extensions/load_balancing_policies/memento/cpuls/MementoWithBinomialEngine.h"
#include <memory>
#include <unordered_map>

namespace Envoy {
namespace Extensions {
namespace LoadBalancingPolicies {
namespace Memento {

// Unified Memento table that handles both weighted and unweighted scenarios
// Automatically detects if weights are uniform and switches modes accordingly
class MementoTable : public Upstream::ThreadAwareLoadBalancerBase::HashingLoadBalancer {
public:
  MementoTable(const Upstream::NormalizedHostWeightVector& normalized_host_weights, uint64_t table_size);

  // Metodo principale per la selezione dell'host.
  Upstream::HostSelectionResponse chooseHost(uint64_t hash, uint32_t attempt) const override;

  // Aggiorna la tabella in base al nuovo insieme di host/weight senza ricreare l'istanza.
  void update(const Upstream::NormalizedHostWeightVector& normalized_host_weights);

  // Statistics for debugging
  struct Stats {
    size_t total_virtual_nodes = 0;
    size_t total_physical_hosts = 0;
    bool is_weighted_mode = false;
  };
  
  Stats getStats() const;

private:
  // Adattatore per usare l'hash di Envoy con l'interfaccia HashFunction del core cpuls.
  class EnvoyHashFunction : public ::HashFunction {
  public:
    int64_t hash(const std::string& key, uint64_t seed = 0) const override;
  };

  // Determina se i pesi sono uniformi (tutti uguali)
  bool areWeightsUniform(const Upstream::NormalizedHostWeightVector& normalized_host_weights) const;
  
public:
  // Converte peso normalizzato in peso intero per nodi virtuali
  uint32_t normalizedWeightToInteger(double normalized_weight) const;

private:

  // Initializza modalità unweighted (mapping diretto)
  void initializeUnweighted(const Upstream::NormalizedHostWeightVector& normalized_host_weights);
  
  // Initializza modalità weighted (nodi virtuali)  
  void initializeWeighted(const Upstream::NormalizedHostWeightVector& normalized_host_weights);

  // Aggiornamenti per modalità unweighted
  void updateUnweighted(const Upstream::NormalizedHostWeightVector& normalized_host_weights);
  
  // Aggiornamenti per modalità weighted
  void updateWeighted(const Upstream::NormalizedHostWeightVector& normalized_host_weights);
  
  // Helper methods per aggiornamenti incrementali weighted
  void addVirtualNodes(const Upstream::Host* host, uint32_t weight);
  void removeVirtualNodes(const Upstream::Host* host);
  void updateVirtualNodes(const Upstream::Host* host, uint32_t old_weight, uint32_t new_weight);

  // === State ===
  
  // Modalità corrente
  bool is_weighted_mode_ = false;
  
  // Per modalità unweighted: mapping diretto indice -> host
  std::vector<Upstream::HostConstSharedPtr> host_table_;

  // Per modalità weighted: mapping nodi virtuali -> host fisico
  std::vector<Upstream::HostConstSharedPtr> virtual_to_physical_;
  std::unordered_map<const Upstream::Host*, std::pair<size_t, size_t>> physical_to_virtual_range_;
  std::unordered_map<const Upstream::Host*, uint32_t> current_weights_;

  // L'istanza del tuo algoritmo (usando unique_ptr per permettere re-inizializzazione)
  mutable std::unique_ptr<MementoWithBinomialEngine> memento_engine_;
  EnvoyHashFunction hash_function_;
  
  // Fattore di scala per conversione pesi
  static constexpr uint32_t WEIGHT_SCALE_FACTOR = 1000;
};

} // namespace Memento
} // namespace LoadBalancingPolicies
} // namespace Extensions
} // namespace Envoy