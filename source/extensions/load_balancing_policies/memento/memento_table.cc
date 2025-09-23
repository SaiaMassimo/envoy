// envoy/source/extensions/load_balancing_policies/memento/memento_table.cc
#include "source/extensions/load_balancing_policies/memento/memento_table.h"
#include "source/common/common/hash.h"

namespace Envoy {
namespace Extensions {
namespace LoadBalancingPolicies {
namespace Memento {

int64_t MementoTable::EnvoyHashFunction::hash(const std::string& key) const {
  return static_cast<int64_t>(HashUtil::xxHash64(key));
}

MementoTable::MementoTable(const Upstream::NormalizedHostWeightVector& normalized_host_weights, uint64_t /*table_size*/)
    : memento_engine_(normalized_host_weights.size(), hash_function_) {

  // Popola la tabella di host. L'indice del vettore corrisponderà al "bucket" di Memento.
  host_table_.reserve(normalized_host_weights.size());
  for (const auto& host_weight_pair : normalized_host_weights) {
    host_table_.push_back(host_weight_pair.first);
  }
}

void MementoTable::update(const Upstream::NormalizedHostWeightVector& normalized_host_weights) {
  host_table_.clear();
  host_table_.reserve(normalized_host_weights.size());
  for (const auto& host_weight_pair : normalized_host_weights) {
    host_table_.push_back(host_weight_pair.first);
  }
  memento_engine_.resize(static_cast<int>(normalized_host_weights.size()));
}

Upstream::HostSelectionResponse MementoTable::chooseHost(uint64_t hash, uint32_t attempt) const {
  if (host_table_.empty()) {
    return Upstream::HostSelectionResponse(Upstream::HostConstSharedPtr(nullptr));
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
    return Upstream::HostSelectionResponse(Upstream::HostConstSharedPtr(nullptr));
  }

  // 3. Restituisci l'host corrispondente all'indice.
  return Upstream::HostSelectionResponse(host_table_[bucket_index]);
}

} // namespace Memento
} // namespace LoadBalancingPolicies
} // namespace Extensions
} // namespace Envoy