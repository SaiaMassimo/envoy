// envoy/source/extensions/load_balancing_policies/memento/memento_table.cc
#include "source/extensions/load_balancing_policies/memento/memento_table.h"

namespace Envoy {
namespace Extensions {
namespace LoadBalancingPolicies {
namespace Memento {

MementoTable::MementoTable(const Upstream::NormalizedHostWeightVector& normalized_host_weights, uint64_t /*table_size*/)
    : memento_engine_(normalized_host_weights.size(), mementoHash) {

  // Popola la tabella di host. L'indice del vettore corrisponderà al "bucket" di Memento.
  host_table_.reserve(normalized_host_weights.size());
  for (const auto& host_weight_pair : normalized_host_weights) {
    host_table_.push_back(host_weight_pair.first);
  }
}

Upstream::HostConstSharedPtr MementoTable::chooseHost(uint64_t hash, uint32_t attempt) const {
  if (host_table_.empty()) {
    return nullptr;
  }

  // Converte l'hash a 64 bit in una stringa per l'API di Memento.
  // Aggiungiamo 'attempt' per cambiare la chiave nei tentativi successivi.
  const std::string key = std::to_string(hash) + std::to_string(attempt);

  // 1. Usa Memento per ottenere l'indice dell'host (il "bucket").
  const int bucket_index = memento_engine_.getBucket(key);

  // 2. Controlla che l'indice sia valido.
  if (bucket_index < 0 || static_cast<size_t>(bucket_index) >= host_table_.size()) {
    // Logica di fallback se l'indice non è valido.
    // Potresti restituire un host casuale o nullptr.
    return nullptr;
  }

  // 3. Restituisci l'host corrispondente all'indice.
  return host_table_[bucket_index];
}

} // namespace Memento
} // namespace LoadBalancingPolicies
} // namespace Extensions
} // namespace Envoy