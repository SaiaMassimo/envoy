// envoy/source/extensions/load_balancing_policies/memento/weighted_memento_table.cc
#include "source/extensions/load_balancing_policies/memento/weighted_memento_table.h"
#include "source/common/common/hash.h"
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <iostream>

namespace Envoy {
namespace Extensions {
namespace LoadBalancingPolicies {
namespace Memento {

int64_t WeightedMementoTable::EnvoyHashFunction::hash(const std::string& key, uint64_t seed) const {
  return static_cast<int64_t>(HashUtil::xxHash64(key, seed));
}

WeightedMementoTable::WeightedMementoTable(
    const Upstream::NormalizedHostWeightVector& normalized_host_weights, 
    uint64_t /*table_size*/) 
    : memento_engine_(1, hash_function_) {  // Inizializza con 1 nodo per evitare problemi con filtri

  // Calcola il numero totale di nodi virtuali necessari
  size_t total_virtual_nodes = 0;
  for (const auto& host_weight_pair : normalized_host_weights) {
    uint32_t virtual_nodes = normalizedWeightToInteger(host_weight_pair.second);
    total_virtual_nodes += virtual_nodes;
  }

  // Aggiungi nodi virtuali all'engine (total_virtual_nodes - 1 perché partiamo da 1)
  for (size_t i = 1; i < total_virtual_nodes; ++i) {
    memento_engine_.addBucket();
  }
  
  // Popola la mappa virtuale -> fisica
  virtual_to_physical_.reserve(total_virtual_nodes);
  
  size_t virtual_node_index = 0;
  for (const auto& host_weight_pair : normalized_host_weights) {
    const auto& host = host_weight_pair.first;
    uint32_t virtual_nodes = normalizedWeightToInteger(host_weight_pair.second);
    
    // Aggiungi nodi virtuali per questo host
    for (uint32_t i = 0; i < virtual_nodes; ++i) {
      virtual_to_physical_.push_back(host);
    }
    
    // Salva il range di nodi virtuali per questo host
    physical_to_virtual_range_[host.get()] = {virtual_node_index, virtual_node_index + virtual_nodes};
    current_weights_[host.get()] = virtual_nodes;
    
    virtual_node_index += virtual_nodes;
  }
  
  std::cout << "WeightedMementoTable initialized with " << total_virtual_nodes 
            << " virtual nodes for " << normalized_host_weights.size() << " physical hosts" << std::endl;
}

uint32_t WeightedMementoTable::normalizedWeightToInteger(double normalized_weight) const {
  // Converte peso normalizzato in peso intero
  // Usa un fattore di scala per mantenere precisione
  uint32_t scaled_weight = static_cast<uint32_t>(normalized_weight * WEIGHT_SCALE_FACTOR);
  
  // Assicurati che sia almeno 1 e non superi il limite
  return std::max(MIN_VIRTUAL_NODES_PER_HOST, 
                  std::min(scaled_weight, MAX_VIRTUAL_NODES_PER_HOST));
}

void WeightedMementoTable::update(const Upstream::NormalizedHostWeightVector& normalized_host_weights) {
  std::cout << "WeightedMementoTable::update called with " << normalized_host_weights.size() << " hosts" << std::endl;
  
  // 1. Calcola nuovi pesi interi
  std::unordered_map<const Upstream::Host*, uint32_t> new_weights;
  for (const auto& hw : normalized_host_weights) {
    new_weights[hw.first.get()] = normalizedWeightToInteger(hw.second);
  }
  
  // 2. Identifica cambiamenti e aggiorna
  for (const auto& [host, new_weight] : new_weights) {
    auto it = current_weights_.find(host);
    if (it == current_weights_.end()) {
      // Nuovo host: aggiungi nodi virtuali
      std::cout << "Adding new host with " << new_weight << " virtual nodes" << std::endl;
      addVirtualNodes(host, new_weight);
    } else if (it->second != new_weight) {
      // Peso cambiato: aggiorna nodi virtuali
      std::cout << "Updating host weight from " << it->second << " to " << new_weight << std::endl;
      updateVirtualNodes(host, it->second, new_weight);
    }
  }
  
  // 3. Rimuovi host non più presenti
  for (auto it = current_weights_.begin(); it != current_weights_.end();) {
    if (new_weights.find(it->first) == new_weights.end()) {
      std::cout << "Removing host with " << it->second << " virtual nodes" << std::endl;
      removeVirtualNodes(it->first);
      it = current_weights_.erase(it);
    } else {
      ++it;
    }
  }
  
  std::cout << "Update completed. Total virtual nodes: " << virtual_to_physical_.size() << std::endl;
}

void WeightedMementoTable::addVirtualNodes(const Upstream::Host* host, uint32_t weight) {
  // Aggiungi nodi virtuali alla fine del vettore
  size_t start_index = virtual_to_physical_.size();
  
  for (uint32_t i = 0; i < weight; ++i) {
    virtual_to_physical_.push_back(Upstream::HostConstSharedPtr(host, [](const Upstream::Host*){}));
    memento_engine_.addBucket();
  }
  
  // Salva il range per questo host
  physical_to_virtual_range_[host] = {start_index, start_index + weight};
  current_weights_[host] = weight;
}

void WeightedMementoTable::removeVirtualNodes(const Upstream::Host* host) {
  auto it = physical_to_virtual_range_.find(host);
  if (it == physical_to_virtual_range_.end()) {
    return; // Host non trovato
  }
  
  auto [start, end] = it->second;
  size_t num_nodes = end - start;
  
  // Rimuovi nodi virtuali dall'engine (in ordine inverso per mantenere indici validi)
  for (size_t i = 0; i < num_nodes; ++i) {
    memento_engine_.removeBucket();
  }
  
  // Rimuovi dal vettore (in ordine inverso)
  virtual_to_physical_.erase(virtual_to_physical_.begin() + static_cast<long>(start), 
                            virtual_to_physical_.begin() + static_cast<long>(end));
  
  // Aggiorna i range degli altri host
  for (auto& [other_host, range] : physical_to_virtual_range_) {
    if (other_host != host && range.first > start) {
      range.first -= num_nodes;
      range.second -= num_nodes;
    }
  }
  
  // Rimuovi l'host dalla mappa
  physical_to_virtual_range_.erase(it);
  current_weights_.erase(host);
}

void WeightedMementoTable::updateVirtualNodes(const Upstream::Host* host, 
                                             uint32_t old_weight, uint32_t new_weight) {
  if (old_weight == new_weight) {
    return; // Nessun cambiamento
  }
  
  if (new_weight > old_weight) {
    // Aggiungi nodi virtuali
    size_t start_index = virtual_to_physical_.size();
    uint32_t nodes_to_add = new_weight - old_weight;
    
    for (uint32_t i = 0; i < nodes_to_add; ++i) {
      virtual_to_physical_.push_back(Upstream::HostConstSharedPtr(host, [](const Upstream::Host*){}));
      memento_engine_.addBucket();
    }
    
    // Aggiorna il range
    auto& range = physical_to_virtual_range_[host];
    range.second = start_index + nodes_to_add;
  } else {
    // Rimuovi nodi virtuali
    uint32_t nodes_to_remove = old_weight - new_weight;
    auto& range = physical_to_virtual_range_[host];
    
    // Rimuovi dall'engine
    for (uint32_t i = 0; i < nodes_to_remove; ++i) {
      memento_engine_.removeBucket();
    }
    
    // Rimuovi dal vettore
    virtual_to_physical_.erase(virtual_to_physical_.begin() + static_cast<long>(range.second - nodes_to_remove),
                              virtual_to_physical_.begin() + static_cast<long>(range.second));
    
    // Aggiorna il range
    range.second -= nodes_to_remove;
    
    // Aggiorna i range degli altri host
    for (auto& [other_host, other_range] : physical_to_virtual_range_) {
      if (other_host != host && other_range.first > range.second) {
        other_range.first -= nodes_to_remove;
        other_range.second -= nodes_to_remove;
      }
    }
  }
  
  current_weights_[host] = new_weight;
}

std::pair<size_t, size_t> WeightedMementoTable::getVirtualNodeRange(const Upstream::HostConstSharedPtr& host) const {
  auto it = physical_to_virtual_range_.find(host.get());
  if (it != physical_to_virtual_range_.end()) {
    return it->second;
  }
  return {0, 0}; // Host non trovato
}

Upstream::HostSelectionResponse WeightedMementoTable::chooseHost(uint64_t hash, uint32_t attempt) const {
  if (virtual_to_physical_.empty()) {
    return Upstream::HostSelectionResponse(Upstream::HostConstSharedPtr(nullptr));
  }

  // Converte l'hash a 64 bit in una stringa per l'API di Memento
  const std::string key = std::to_string(hash) + std::to_string(attempt);

  // Usa Memento per ottenere l'indice del nodo virtuale
  const int virtual_node_index = memento_engine_.getBucket(key);

  // Controlla che l'indice sia valido
  if (virtual_node_index < 0 || static_cast<size_t>(virtual_node_index) >= virtual_to_physical_.size()) {
    return Upstream::HostSelectionResponse(Upstream::HostConstSharedPtr(nullptr));
  }

  // Restituisci l'host fisico corrispondente al nodo virtuale
  return Upstream::HostSelectionResponse(virtual_to_physical_[virtual_node_index]);
}

WeightedMementoTable::Stats WeightedMementoTable::getStats() const {
  Stats stats;
  stats.total_virtual_nodes = virtual_to_physical_.size();
  stats.total_physical_hosts = current_weights_.size();
  
  if (!current_weights_.empty()) {
    auto minmax = std::minmax_element(current_weights_.begin(), current_weights_.end(),
                                     [](const auto& a, const auto& b) { return a.second < b.second; });
    stats.min_virtual_nodes_per_host = minmax.first->second;
    stats.max_virtual_nodes_per_host = minmax.second->second;
  }
  
  return stats;
}

} // namespace Memento
} // namespace LoadBalancingPolicies
} // namespace Extensions
} // namespace Envoy
