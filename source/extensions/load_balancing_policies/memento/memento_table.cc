// envoy/source/extensions/load_balancing_policies/memento/memento_table.cc
#include "source/extensions/load_balancing_policies/memento/memento_table.h"
#include "source/common/common/hash.h"
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <iostream>

namespace Envoy {
namespace Extensions {
namespace LoadBalancingPolicies {
namespace Memento {

int64_t MementoTable::EnvoyHashFunction::hash(const std::string& key, uint64_t seed) const {
  return static_cast<int64_t>(HashUtil::xxHash64(key, seed));
}

bool MementoTable::areWeightsUniform(const Upstream::NormalizedHostWeightVector& normalized_host_weights) const {
  if (normalized_host_weights.empty()) {
    return true;
  }
  
  const double first_weight = normalized_host_weights[0].second;
  const double tolerance = 1e-3;  // Increased tolerance for normalized weights
  
  for (const auto& host_weight_pair : normalized_host_weights) {
    if (std::abs(host_weight_pair.second - first_weight) > tolerance) {
      return false;  // Found different weight
    }
  }
  
  return true;  // All weights are uniform
}

uint32_t MementoTable::normalizedWeightToInteger(double normalized_weight) const {
  return static_cast<uint32_t>(std::round(normalized_weight * WEIGHT_SCALE_FACTOR));
}

MementoTable::MementoTable(const Upstream::NormalizedHostWeightVector& normalized_host_weights, uint64_t /*table_size*/)
    : memento_engine_(std::make_unique<MementoWithBinomialEngine>(1, hash_function_)) {  // Inizializza con 1 per evitare problemi con filtri

  // Auto-detect se usare modalità weighted o unweighted
  is_weighted_mode_ = !areWeightsUniform(normalized_host_weights);
  
  if (is_weighted_mode_) {
    initializeWeighted(normalized_host_weights);
  } else {
    initializeUnweighted(normalized_host_weights);
  }
}

void MementoTable::initializeUnweighted(const Upstream::NormalizedHostWeightVector& normalized_host_weights) {
  // Modalità unweighted: mapping diretto indice -> host
  const size_t num_hosts = normalized_host_weights.size();
  
  // Aggiungi bucket all'engine per arrivare al numero di host
  for (size_t i = 1; i < num_hosts; ++i) {
    memento_engine_->addBucket();
  }
  
  // Popola la tabella di host
  host_table_.clear();
  host_table_.reserve(num_hosts);
  for (const auto& host_weight_pair : normalized_host_weights) {
    host_table_.push_back(host_weight_pair.first);
  }
  
}

void MementoTable::initializeWeighted(const Upstream::NormalizedHostWeightVector& normalized_host_weights) {
  // Modalità weighted: usa nodi virtuali
  
  // Calcola il numero totale di nodi virtuali necessari
  size_t total_virtual_nodes = 0;
  for (const auto& host_weight_pair : normalized_host_weights) {
    uint32_t virtual_nodes = normalizedWeightToInteger(host_weight_pair.second);
    total_virtual_nodes += virtual_nodes;
  }

  // Aggiungi nodi virtuali all'engine (total_virtual_nodes - 1 perché partiamo da 1)
  for (size_t i = 1; i < total_virtual_nodes; ++i) {
    memento_engine_->addBucket();
  }
  
  // Popola la mappa virtuale -> fisica
  virtual_to_physical_.clear();
  virtual_to_physical_.reserve(total_virtual_nodes);
  physical_to_virtual_range_.clear();
  current_weights_.clear();
  
  size_t virtual_node_index = 0;
  for (const auto& host_weight_pair : normalized_host_weights) {
    const auto& host = host_weight_pair.first;
    uint32_t virtual_nodes = normalizedWeightToInteger(host_weight_pair.second);
    
    // Mappa range di nodi virtuali per questo host
    size_t start_index = virtual_node_index;
    size_t end_index = virtual_node_index + virtual_nodes;
    
    physical_to_virtual_range_[host.get()] = {start_index, end_index};
    current_weights_[host.get()] = virtual_nodes;
    
    // Aggiungi nodi virtuali alla mappa
    for (uint32_t i = 0; i < virtual_nodes; ++i) {
      virtual_to_physical_.push_back(host);
    }
    
    virtual_node_index += virtual_nodes;
  }
  
}

MementoTable::Stats MementoTable::getStats() const {
  Stats stats;
  
  if (is_weighted_mode_) {
    stats.total_virtual_nodes = virtual_to_physical_.size();
    stats.total_physical_hosts = current_weights_.size();
  } else {
    stats.total_virtual_nodes = host_table_.size();  // In unweighted mode, 1:1 mapping
    stats.total_physical_hosts = host_table_.size();
  }
  
  stats.is_weighted_mode = is_weighted_mode_;
  return stats;
}

void MementoTable::update(const Upstream::NormalizedHostWeightVector& normalized_host_weights) {
  // Verifica se dobbiamo cambiare modalità
  bool new_mode_is_weighted = !areWeightsUniform(normalized_host_weights);
  
  if (new_mode_is_weighted != is_weighted_mode_) {
    
    // Reset engine and reinitialize in new mode
    memento_engine_ = std::make_unique<MementoWithBinomialEngine>(1, hash_function_);
    is_weighted_mode_ = new_mode_is_weighted;
    
    if (is_weighted_mode_) {
      initializeWeighted(normalized_host_weights);
    } else {
      initializeUnweighted(normalized_host_weights);
    }
    return;
  }
  
  // Stesso modo: aggiorna incrementalmente
  if (is_weighted_mode_) {
    updateWeighted(normalized_host_weights);
  } else {
    updateUnweighted(normalized_host_weights);
  }
}

void MementoTable::updateUnweighted(const Upstream::NormalizedHostWeightVector& normalized_host_weights) {
  // Implementazione originale di update per modalità unweighted
  std::vector<Upstream::HostConstSharedPtr> new_hosts;
  new_hosts.reserve(normalized_host_weights.size());
  for (const auto& hw : normalized_host_weights) {
    new_hosts.push_back(hw.first);
  }

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

  // 1) Remove hosts that disappeared
  std::vector<size_t> to_remove;
  to_remove.reserve(host_table_.size());
  for (size_t i = 0; i < host_table_.size(); ++i) {
    if (new_set.find(host_table_[i].get()) == new_set.end()) {
      to_remove.push_back(i);
    }
  }
  std::sort(to_remove.begin(), to_remove.end(), std::greater<size_t>());
  for (size_t idx : to_remove) {
    memento_engine_->removeBucket();
    host_table_.erase(host_table_.begin() + static_cast<long>(idx));
  }

  // 2) Add new hosts
  for (const auto& h : new_hosts) {
    if (old_index.find(h.get()) == old_index.end()) {
      const int idx = memento_engine_->addBucket();
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

void MementoTable::updateWeighted(const Upstream::NormalizedHostWeightVector& normalized_host_weights) {
  // Implementazione semplificata per modalità weighted: reinizializza
  // TODO: implementare aggiornamenti incrementali più sofisticati
  
  // Reset e reinizializza
  virtual_to_physical_.clear();
  physical_to_virtual_range_.clear();
  current_weights_.clear();
  
  // Reset engine
  memento_engine_ = std::make_unique<MementoWithBinomialEngine>(1, hash_function_);
  
  // Reinizializza con nuovi pesi
  initializeWeighted(normalized_host_weights);
}

Upstream::HostSelectionResponse MementoTable::chooseHost(uint64_t hash, uint32_t attempt) const {
  // Converte l'hash a 64 bit in una stringa per l'API di Memento.
  const std::string key = std::to_string(hash) + std::to_string(attempt);

  // Usa Memento per ottenere l'indice (il "bucket")
  const int bucket_index = memento_engine_->getBucket(key);

  // Controlla che l'indice sia valido
  if (bucket_index < 0) {
    return Upstream::HostSelectionResponse(Upstream::HostConstSharedPtr(nullptr));
  }

  if (is_weighted_mode_) {
    // Modalità weighted: usa nodi virtuali
    if (virtual_to_physical_.empty() || static_cast<size_t>(bucket_index) >= virtual_to_physical_.size()) {
      return Upstream::HostSelectionResponse(Upstream::HostConstSharedPtr(nullptr));
    }
    return Upstream::HostSelectionResponse(virtual_to_physical_[bucket_index]);
  } else {
    // Modalità unweighted: mapping diretto con modulo per gestire indici fuori range
    if (host_table_.empty()) {
      return Upstream::HostSelectionResponse(Upstream::HostConstSharedPtr(nullptr));
    }
    
    // Controllo di sicurezza: assicurati che l'indice sia valido
    if (static_cast<size_t>(bucket_index) >= host_table_.size()) {
      // Fallback: usa modulo per mappare l'indice fuori range
      const size_t host_index = static_cast<size_t>(bucket_index) % host_table_.size();
      return Upstream::HostSelectionResponse(host_table_[host_index]);
    }
    return Upstream::HostSelectionResponse(host_table_[bucket_index]);
  }
}

} // namespace Memento
} // namespace LoadBalancingPolicies
} // namespace Extensions
} // namespace Envoy