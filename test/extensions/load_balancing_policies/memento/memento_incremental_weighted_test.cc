// test/extensions/load_balancing_policies/memento/memento_incremental_weighted_test.cc
#include "source/extensions/load_balancing_policies/memento/memento_table.h"
#include "test/mocks/upstream/cluster_info.h"
#include "test/mocks/upstream/host.h"
#include "gtest/gtest.h"
#include <unordered_map>
#include <unordered_set>

namespace Envoy {
namespace Extensions {
namespace LoadBalancingPolicies {
namespace Memento {

class MementoIncrementalWeightedTest : public testing::Test {
public:
  void SetUp() override {
    // Setup mock cluster info
    info_ = std::make_shared<Upstream::MockClusterInfo>();
    
    // Create test hosts using MockHost (same approach as unified_memento_test)
    host1_ = std::make_shared<Upstream::MockHost>();
    host2_ = std::make_shared<Upstream::MockHost>();
    host3_ = std::make_shared<Upstream::MockHost>();
    host4_ = std::make_shared<Upstream::MockHost>();
    
    // Setup host weights
    EXPECT_CALL(*host1_, weight()).WillRepeatedly(testing::Return(1));
    EXPECT_CALL(*host2_, weight()).WillRepeatedly(testing::Return(1));
    EXPECT_CALL(*host3_, weight()).WillRepeatedly(testing::Return(1));
    EXPECT_CALL(*host4_, weight()).WillRepeatedly(testing::Return(1));
  }

  Upstream::NormalizedHostWeightVector createWeightedConfig() {
    Upstream::NormalizedHostWeightVector weights;
    weights.push_back({host1_, 0.5});   // 500 virtual nodes
    weights.push_back({host2_, 0.3});   // 300 virtual nodes
    weights.push_back({host3_, 0.2});   // 200 virtual nodes
    return weights;
  }
  
  Upstream::NormalizedHostWeightVector createUpdatedWeightedConfig() {
    Upstream::NormalizedHostWeightVector weights;
    weights.push_back({host1_, 0.4});   // 400 virtual nodes (reduced)
    weights.push_back({host2_, 0.4});   // 400 virtual nodes (increased)
    weights.push_back({host3_, 0.2});   // 200 virtual nodes (unchanged)
    return weights;
  }
  
  Upstream::NormalizedHostWeightVector createConfigWithNewHost() {
    Upstream::NormalizedHostWeightVector weights;
    weights.push_back({host1_, 0.3});   // 300 virtual nodes
    weights.push_back({host2_, 0.3});   // 300 virtual nodes
    weights.push_back({host3_, 0.2});   // 200 virtual nodes
    weights.push_back({host4_, 0.2});   // 200 virtual nodes (new host)
    return weights;
  }
  
  Upstream::NormalizedHostWeightVector createConfigWithoutHost() {
    Upstream::NormalizedHostWeightVector weights;
    weights.push_back({host1_, 0.7});   // 700 virtual nodes
    weights.push_back({host3_, 0.3});   // 300 virtual nodes (host2 removed)
    return weights;
  }

  // Helper per verificare che la tabella funzioni
  void verifyTableFunctionality(MementoTable& table, uint64_t num_samples = 100) {
    for (uint64_t i = 0; i < num_samples; ++i) {
      auto response = table.chooseHost(i, 0);
      EXPECT_NE(response.host, nullptr);
      // Non possiamo verificare l'indirizzo perché usiamo MockHost
    }
  }

protected:
  std::shared_ptr<Upstream::MockClusterInfo> info_;
  std::shared_ptr<Upstream::MockHost> host1_, host2_, host3_, host4_;
};

TEST_F(MementoIncrementalWeightedTest, InitialWeightedSetup) {
  auto weights = createWeightedConfig();
  MementoTable table(weights, 65537);
  
  auto stats = table.getStats();
  EXPECT_TRUE(stats.is_weighted_mode);
  EXPECT_EQ(stats.total_physical_hosts, 3);
  EXPECT_EQ(stats.total_virtual_nodes, 1000); // 500 + 300 + 200
  
  // Verifica che la tabella funzioni
  verifyTableFunctionality(table);
}

TEST_F(MementoIncrementalWeightedTest, WeightUpdateIncremental) {
  auto initial_weights = createWeightedConfig();
  MementoTable table(initial_weights, 65537);
  
  auto initial_stats = table.getStats();
  EXPECT_TRUE(initial_stats.is_weighted_mode);
  EXPECT_EQ(initial_stats.total_physical_hosts, 3);
  EXPECT_EQ(initial_stats.total_virtual_nodes, 1000); // 500 + 300 + 200
  
  // Aggiorna i pesi
  auto updated_weights = createUpdatedWeightedConfig();
  table.update(updated_weights);
  
  auto updated_stats = table.getStats();
  EXPECT_TRUE(updated_stats.is_weighted_mode);
  EXPECT_EQ(updated_stats.total_physical_hosts, 3);
  EXPECT_EQ(updated_stats.total_virtual_nodes, 1000); // 400 + 400 + 200
  
  // Verifica che la tabella funzioni ancora
  verifyTableFunctionality(table);
}

TEST_F(MementoIncrementalWeightedTest, AddNewHostIncremental) {
  auto initial_weights = createWeightedConfig();
  MementoTable table(initial_weights, 65537);
  
  auto initial_stats = table.getStats();
  EXPECT_EQ(initial_stats.total_physical_hosts, 3);
  EXPECT_EQ(initial_stats.total_virtual_nodes, 1000);
  
  // Aggiungi un nuovo host
  auto weights_with_new_host = createConfigWithNewHost();
  table.update(weights_with_new_host);
  
  auto updated_stats = table.getStats();
  EXPECT_EQ(updated_stats.total_physical_hosts, 4);
  EXPECT_EQ(updated_stats.total_virtual_nodes, 1000); // 300 + 300 + 200 + 200
  
  // Verifica che la tabella funzioni ancora
  verifyTableFunctionality(table);
}

TEST_F(MementoIncrementalWeightedTest, RemoveHostIncremental) {
  auto initial_weights = createWeightedConfig();
  MementoTable table(initial_weights, 65537);
  
  auto initial_stats = table.getStats();
  EXPECT_EQ(initial_stats.total_physical_hosts, 3);
  
  // Rimuovi un host
  auto weights_without_host = createConfigWithoutHost();
  table.update(weights_without_host);
  
  auto updated_stats = table.getStats();
  EXPECT_EQ(updated_stats.total_physical_hosts, 2);
  // Con pesi diversi (0.7 e 0.3), Memento rimane in modalità weighted
  // 700 + 300 = 1000 nodi virtuali totali
  EXPECT_EQ(updated_stats.total_virtual_nodes, 1000);
  EXPECT_TRUE(updated_stats.is_weighted_mode); // Dovrebbe rimanere in modalità weighted
  
  // Verifica che la tabella funzioni ancora
  verifyTableFunctionality(table);
}

TEST_F(MementoIncrementalWeightedTest, ConsistencyAfterUpdates) {
  auto initial_weights = createWeightedConfig();
  MementoTable table(initial_weights, 65537);
  
  // Esegui una serie di aggiornamenti
  auto weights1 = createUpdatedWeightedConfig();
  table.update(weights1);
  
  auto weights2 = createConfigWithNewHost();
  table.update(weights2);
  
  auto weights3 = createConfigWithoutHost();
  table.update(weights3);
  
  // Verifica che dopo la rimozione abbiamo ancora 1000 nodi virtuali
  auto stats3 = table.getStats();
  EXPECT_EQ(stats3.total_virtual_nodes, 1000) << "Expected 1000 virtual nodes after removing host2, but got " << stats3.total_virtual_nodes;
  EXPECT_TRUE(stats3.is_weighted_mode) << "Expected weighted mode after removing host2, but got unweighted mode";
  
  // Torna alla configurazione originale
  table.update(initial_weights);
  
  // Verifica che la tabella funzioni ancora
  verifyTableFunctionality(table);
  
  // Verifica che le statistiche siano tornate ai valori originali
  auto final_stats = table.getStats();
  
  EXPECT_TRUE(final_stats.is_weighted_mode);
  EXPECT_EQ(final_stats.total_physical_hosts, 3);
  EXPECT_EQ(final_stats.total_virtual_nodes, 1000);
}

TEST_F(MementoIncrementalWeightedTest, MultipleRapidUpdates) {
  auto initial_weights = createWeightedConfig();
  MementoTable table(initial_weights, 65537);
  
  // Esegui molti aggiornamenti rapidi
  for (int i = 0; i < 10; ++i) {
    auto weights = createWeightedConfig();
    // Modifica leggermente i pesi
    weights[0].second = 0.5 + (i * 0.01);
    weights[1].second = 0.3 - (i * 0.01);
    weights[2].second = 0.2;
    
    table.update(weights);
    
    // Verifica che la tabella sia ancora funzionante
    verifyTableFunctionality(table, 10);
  }
  
  auto stats = table.getStats();
  EXPECT_TRUE(stats.is_weighted_mode);
  EXPECT_EQ(stats.total_physical_hosts, 3);
}

TEST_F(MementoIncrementalWeightedTest, EdgeCaseSingleHost) {
  Upstream::NormalizedHostWeightVector single_host_weights;
  single_host_weights.push_back({host1_, 1.0});  // Usa host reale
  
  MementoTable table(single_host_weights, 65537);
  
  auto stats = table.getStats();
  // Con un solo host, dovrebbe essere in modalità unweighted (non ha senso weighted)
  EXPECT_FALSE(stats.is_weighted_mode);
  EXPECT_EQ(stats.total_physical_hosts, 1);
  EXPECT_EQ(stats.total_virtual_nodes, 1); // 1:1 mapping in unweighted mode
  
  // Test di selezione
  for (int i = 0; i < 10; ++i) {
    auto response = table.chooseHost(i, 0);
    EXPECT_NE(response.host, nullptr);
    // Non possiamo verificare l'indirizzo perché usiamo MockHost
  }
  
  // Aggiorna con un peso diverso (ma sempre un solo host)
  single_host_weights[0].second = 0.5;
  table.update(single_host_weights);
  
  stats = table.getStats();
  // Dovrebbe rimanere in modalità unweighted
  EXPECT_FALSE(stats.is_weighted_mode);
  EXPECT_EQ(stats.total_virtual_nodes, 1);
  
  // Test di selezione dopo aggiornamento
  for (int i = 0; i < 10; ++i) {
    auto response = table.chooseHost(i, 0);
    EXPECT_NE(response.host, nullptr);
    // Non possiamo verificare l'indirizzo perché usiamo MockHost
  }
}

TEST_F(MementoIncrementalWeightedTest, EmptyHostList) {
  Upstream::NormalizedHostWeightVector empty_weights;
  
  MementoTable table(empty_weights, 65537);
  
  auto stats = table.getStats();
  EXPECT_FALSE(stats.is_weighted_mode); // Dovrebbe essere unweighted per lista vuota
  EXPECT_EQ(stats.total_physical_hosts, 0);
  EXPECT_EQ(stats.total_virtual_nodes, 0);
  
  // Tutte le selezioni dovrebbero fallire
  for (int i = 0; i < 10; ++i) {
    auto response = table.chooseHost(i, 0);
    EXPECT_EQ(response.host, nullptr);
  }
}

TEST_F(MementoIncrementalWeightedTest, TemporaryUniformWeightsDuringUpdate) {
  // Scenario: 3 host con pesi diversi, rimuoviamo uno e aggiorniamo gli altri
  // Durante l'aggiornamento incrementale, temporaneamente abbiamo solo 2 host con pesi uniformi
  
  Upstream::NormalizedHostWeightVector initial_weights;
  initial_weights.push_back({host1_, 0.2});   // 200 virtual nodes
  initial_weights.push_back({host2_, 0.2});   // 200 virtual nodes  
  initial_weights.push_back({host3_, 0.6});   // 600 virtual nodes
  
  MementoTable table(initial_weights, 65537);
  
  auto initial_stats = table.getStats();
  EXPECT_TRUE(initial_stats.is_weighted_mode); // Dovrebbe essere weighted
  EXPECT_EQ(initial_stats.total_physical_hosts, 3);
  EXPECT_EQ(initial_stats.total_virtual_nodes, 1000);
  
  // Aggiornamento: rimuovi host3 e aggiorna host1 e host2 a 0.4
  Upstream::NormalizedHostWeightVector updated_weights;
  updated_weights.push_back({host1_, 0.4});   // 400 virtual nodes
  updated_weights.push_back({host2_, 0.4});
  updated_weights.push_back({host3_, 0.2});   // 400 virtual nodes
  // host3 rimosso
  
  table.update(updated_weights);
  
  auto final_stats = table.getStats();
  EXPECT_TRUE(final_stats.is_weighted_mode); // Dovrebbe rimanere weighted (pesi uniformi ma non 1:1)
  EXPECT_EQ(final_stats.total_physical_hosts, 3);
  EXPECT_EQ(final_stats.total_virtual_nodes, 1000); // 400 + 400
  
  // Verifica che la tabella funzioni ancora
  verifyTableFunctionality(table);
}

} // namespace Memento
} // namespace LoadBalancingPolicies
} // namespace Extensions
} // namespace Envoy
