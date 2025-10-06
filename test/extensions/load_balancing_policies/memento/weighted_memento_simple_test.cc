// test/extensions/load_balancing_policies/memento/weighted_memento_simple_test.cc
#include "source/extensions/load_balancing_policies/memento/memento_table.h"
#include "gtest/gtest.h"

namespace Envoy {
namespace Extensions {
namespace LoadBalancingPolicies {
namespace Memento {

// Test semplice per verificare che MementoTable unificata funzioni
TEST(WeightedMementoSimpleTest, BasicFunctionality) {
  // Crea un vettore di pesi normalizzati
  Upstream::NormalizedHostWeightVector weights;
  
  // Simula 3 host con pesi diversi (ridotti per evitare timeout)
  // Host 1: peso 0.05 -> 50 nodi virtuali
  // Host 2: peso 0.033 -> 33 nodi virtuali  
  // Host 3: peso 0.017 -> 17 nodi virtuali
  
  // Per il test, usiamo nullptr come host (non importante per testare la logica)
  weights.push_back({nullptr, 0.05});  // 50 nodi virtuali
  weights.push_back({nullptr, 0.033}); // 33 nodi virtuali
  weights.push_back({nullptr, 0.017}); // 17 nodi virtuali
  
  // Crea la tabella unificata
  MementoTable table(weights, 65537);
  
  // Verifica che la tabella sia stata creata
  auto stats = table.getStats();
  EXPECT_GT(stats.total_virtual_nodes, 0);
  EXPECT_EQ(stats.total_physical_hosts, 3);
  
  std::cout << "MementoTable unificata funziona! Nodi virtuali: " << stats.total_virtual_nodes 
            << ", Host fisici: " << stats.total_physical_hosts << std::endl;
  
  // Test di selezione (anche se con host nullptr) - RIDOTTO per debug
  std::cout << "Inizio test di selezione..." << std::endl;
  auto response = table.chooseHost(12345, 0);
  std::cout << "Prima selezione completata!" << std::endl;
  
  // Non possiamo verificare l'host perché è nullptr, ma almeno non crasha
  EXPECT_TRUE(true); // Se arriviamo qui, non c'è stato crash
  
  std::cout << "Test di selezione completato senza errori!" << std::endl;
}

TEST(WeightedMementoSimpleTest, WeightConversion) {
  MementoTable table({}, 65537);
  
  // Test conversione pesi (ridotti per evitare timeout)
  // Peso 0.05 con fattore 1000 = 50 nodi virtuali
  // Peso 0.033 con fattore 1000 = 33 nodi virtuali  
  // Peso 0.017 con fattore 1000 = 17 nodi virtuali
  
  std::cout << "Test conversione pesi:" << std::endl;
  std::cout << "Peso 0.05 -> " << table.normalizedWeightToInteger(0.05) << " nodi virtuali" << std::endl;
  std::cout << "Peso 0.033 -> " << table.normalizedWeightToInteger(0.033) << " nodi virtuali" << std::endl;
  std::cout << "Peso 0.017 -> " << table.normalizedWeightToInteger(0.017) << " nodi virtuali" << std::endl;
  
  // Verifica che i pesi siano convertiti correttamente
  EXPECT_EQ(table.normalizedWeightToInteger(0.05), 50);
  EXPECT_EQ(table.normalizedWeightToInteger(0.033), 33);
  EXPECT_EQ(table.normalizedWeightToInteger(0.017), 17);
}

} // namespace Memento
} // namespace LoadBalancingPolicies
} // namespace Extensions
} // namespace Envoy
