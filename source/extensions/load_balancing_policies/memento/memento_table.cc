// envoy/source/extensions/load_balancing_policies/memento/memento_table.cc
#include "source/extensions/load_balancing_policies/memento/memento_table.h"
#include "source/common/common/hash.h"
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace Envoy {
namespace Extensions {
namespace LoadBalancingPolicies {
namespace Memento {

int64_t MementoTable::EnvoyHashFunction::hash(const std::string& key, uint64_t seed) const {
  return static_cast<int64_t>(HashUtil::xxHash64(key, seed));
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
  // Build new host set
  std::vector<Upstream::HostConstSharedPtr> new_hosts;
  new_hosts.reserve(normalized_host_weights.size());
  for (const auto& hw : normalized_host_weights) {
    new_hosts.push_back(hw.first);
  }

  // Fast lookup sets/maps
  std::unordered_set<const Upstream::Host*> new_set;
  new_set.reserve(new_hosts.size());
  for (const auto& h : new_hosts) {
    new_set.insert(h.get());
  }

  std::unordered_map<const Upstream::Host*, size_t> old_index;
  old_index.reserve(host_table_.size());
  for (size_t i = 0; i < host_table_.size(); ++i) {
    old_index.emplace(host_table_[i].get(), i);
  }

  // 1) Remove hosts that disappeared (descending index order to keep indices valid)
  std::vector<size_t> to_remove;
  to_remove.reserve(host_table_.size());
  for (size_t i = 0; i < host_table_.size(); ++i) {
    if (new_set.find(host_table_[i].get()) == new_set.end()) {
      to_remove.push_back(i);
    }
  }
  std::sort(to_remove.begin(), to_remove.end(), std::greater<size_t>());
  for (size_t idx : to_remove) {
    // Remove from engine (always remove the last bucket)
    memento_engine_.removeBucket();
    // Remove from table
    host_table_.erase(host_table_.begin() + static_cast<long>(idx));
  }

  // 2) Add new hosts
  for (const auto& h : new_hosts) {
    if (old_index.find(h.get()) == old_index.end()) {
      const int idx = memento_engine_.addBucket();
      if (idx < 0) {
        continue;
      }
      const size_t sidx = static_cast<size_t>(idx);
      if (sidx >= host_table_.size()) {
        host_table_.push_back(h);
      } else {
        host_table_.insert(host_table_.begin() + static_cast<long>(sidx), h);
      }
    }
  }
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

  // Debug: log delle dimensioni
  // std::cout << "DEBUG: host_table_.size()=" << host_table_.size() 
  //           << ", bucket_index=" << bucket_index << std::endl;

  // 2. Controlla che l'indice sia valido.
  if (bucket_index < 0 || static_cast<size_t>(bucket_index) >= static_cast<size_t>(memento_engine_.size())) {
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