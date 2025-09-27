// test/extensions/load_balancing_policies/memento/weighted_memento_lb_test.cc
#include "source/extensions/load_balancing_policies/memento/weighted_memento_lb.h"
#include "test/test_common/test_runtime.h"

#include "gtest/gtest.h"

namespace Envoy {
namespace Extensions {
namespace LoadBalancingPolicies {
namespace Memento {

class WeightedMementoTest : public testing::Test {
public:
  void SetUp() override {
    // Setup mock cluster info
    info_ = std::make_shared<Upstream::MockClusterInfo>();
    
    // Setup test hosts with different weights
    hosts_.clear();
    hosts_.push_back(makeTestHost(info_, "tcp://127.0.0.1:8000", 3));  // Weight 3
    hosts_.push_back(makeTestHost(info_, "tcp://127.0.0.1:8001", 2));  // Weight 2  
    hosts_.push_back(makeTestHost(info_, "tcp://127.0.0.1:8002", 1));  // Weight 1
  }

  // Helper function to create test hosts
  Upstream::HostConstSharedPtr makeTestHost(Upstream::ClusterInfoConstSharedPtr cluster_info,
                                           const std::string& address, uint32_t weight) {
    auto addr = Network::Utility::parseInternetAddressAndPort(address);
    return std::make_shared<Upstream::HostImpl>(
        cluster_info, "", addr, nullptr, weight, envoy::config::core::v3::Locality(),
        envoy::config::endpoint::v3::Endpoint::HealthCheckConfig(), 0,
        envoy::config::core::v3::HealthStatus::UNKNOWN, 0);
  }

  Upstream::NormalizedHostWeightVector createNormalizedWeights() {
    Upstream::NormalizedHostWeightVector weights;
    weights.reserve(hosts_.size());
    
    // Simula normalizzazione: peso 3, 2, 1 -> 0.5, 0.33, 0.17
    weights.push_back({hosts_[0], 0.5});   // 3/6 = 0.5
    weights.push_back({hosts_[1], 0.33});  // 2/6 = 0.33
    weights.push_back({hosts_[2], 0.17});  // 1/6 = 0.17
    
    return weights;
  }

  Upstream::NormalizedHostWeightVector createEqualWeights() {
    Upstream::NormalizedHostWeightVector weights;
    weights.reserve(hosts_.size());
    
    // Pesi uguali: 0.33, 0.33, 0.33
    weights.push_back({hosts_[0], 0.33});
    weights.push_back({hosts_[1], 0.33});
    weights.push_back({hosts_[2], 0.33});
    
    return weights;
  }

protected:
  std::shared_ptr<Upstream::MockClusterInfo> info_;
  std::vector<Upstream::HostConstSharedPtr> hosts_;
};

TEST_F(WeightedMementoTest, BasicWeightedSelection) {
  WeightedMementoLbConfig config(65537);
  WeightedMementoTable table(createNormalizedWeights(), config.tableSize());
  
  // Test multiple selections to verify weight distribution
  std::unordered_map<std::string, int> selection_counts;
  const int num_selections = 10000;
  
  for (int i = 0; i < num_selections; ++i) {
    auto response = table.chooseHost(i, 0);
    ASSERT_TRUE(response.host != nullptr);
    selection_counts[response.host->address()->asString()]++;
  }
  
  // Verify all hosts were selected
  EXPECT_EQ(selection_counts.size(), 3);
  
  // Print distribution for manual verification
  std::cout << "Weighted selection distribution:" << std::endl;
  for (const auto& [address, count] : selection_counts) {
    double percentage = (static_cast<double>(count) / num_selections) * 100.0;
    std::cout << "  " << address << ": " << count << " (" << percentage << "%)" << std::endl;
  }
  
  // Host with weight 3 should be selected most often
  // Host with weight 1 should be selected least often
  auto it_8000 = selection_counts.find("tcp://127.0.0.1:8000");
  auto it_8002 = selection_counts.find("tcp://127.0.0.1:8002");
  
  ASSERT_TRUE(it_8000 != selection_counts.end());
  ASSERT_TRUE(it_8002 != selection_counts.end());
  
  EXPECT_GT(it_8000->second, it_8002->second);
}

TEST_F(WeightedMementoTest, EqualWeightSelection) {
  WeightedMementoLbConfig config(65537);
  WeightedMementoTable table(createEqualWeights(), config.tableSize());
  
  // Test multiple selections to verify equal distribution
  std::unordered_map<std::string, int> selection_counts;
  const int num_selections = 9000;  // Divisible by 3
  
  for (int i = 0; i < num_selections; ++i) {
    auto response = table.chooseHost(i, 0);
    ASSERT_TRUE(response.host != nullptr);
    selection_counts[response.host->address()->asString()]++;
  }
  
  // Verify all hosts were selected
  EXPECT_EQ(selection_counts.size(), 3);
  
  // Print distribution for manual verification
  std::cout << "Equal weight selection distribution:" << std::endl;
  for (const auto& [address, count] : selection_counts) {
    double percentage = (static_cast<double>(count) / num_selections) * 100.0;
    std::cout << "  " << address << ": " << count << " (" << percentage << "%)" << std::endl;
  }
  
  // With equal weights, distribution should be roughly equal
  // Allow for some variance due to hashing
  for (const auto& [address, count] : selection_counts) {
    double percentage = (static_cast<double>(count) / num_selections) * 100.0;
    EXPECT_GE(percentage, 25.0);  // At least 25%
    EXPECT_LE(percentage, 45.0);  // At most 45%
  }
}

TEST_F(WeightedMementoTest, InPlaceWeightUpdate) {
  WeightedMementoLbConfig config(65537);
  WeightedMementoTable table(createNormalizedWeights(), config.tableSize());
  
  // Get initial stats
  auto initial_stats = table.getStats();
  std::cout << "Initial stats: " << initial_stats.total_virtual_nodes 
            << " virtual nodes, " << initial_stats.total_physical_hosts << " physical hosts" << std::endl;
  
  // Update weights: change host 0 from weight 3 to weight 5
  Upstream::NormalizedHostWeightVector updated_weights;
  updated_weights.push_back({hosts_[0], 0.625});  // 5/8 = 0.625
  updated_weights.push_back({hosts_[1], 0.25});   // 2/8 = 0.25
  updated_weights.push_back({hosts_[2], 0.125});  // 1/8 = 0.125
  
  table.update(updated_weights);
  
  // Get updated stats
  auto updated_stats = table.getStats();
  std::cout << "Updated stats: " << updated_stats.total_virtual_nodes 
            << " virtual nodes, " << updated_stats.total_physical_hosts << " physical hosts" << std::endl;
  
  // Verify stats changed
  EXPECT_GT(updated_stats.total_virtual_nodes, initial_stats.total_virtual_nodes);
  EXPECT_EQ(updated_stats.total_physical_hosts, initial_stats.total_physical_hosts);
  
  // Test selection after update
  std::unordered_map<std::string, int> selection_counts;
  const int num_selections = 10000;
  
  for (int i = 0; i < num_selections; ++i) {
    auto response = table.chooseHost(i, 0);
    ASSERT_TRUE(response.host != nullptr);
    selection_counts[response.host->address()->asString()]++;
  }
  
  std::cout << "After weight update distribution:" << std::endl;
  for (const auto& [address, count] : selection_counts) {
    double percentage = (static_cast<double>(count) / num_selections) * 100.0;
    std::cout << "  " << address << ": " << count << " (" << percentage << "%)" << std::endl;
  }
  
  // Host with increased weight should be selected more often
  auto it_8000 = selection_counts.find("tcp://127.0.0.1:8000");
  ASSERT_TRUE(it_8000 != selection_counts.end());
  
  double percentage_8000 = (static_cast<double>(it_8000->second) / num_selections) * 100.0;
  EXPECT_GT(percentage_8000, 50.0);  // Should be > 50% with weight 5/8
}

TEST_F(WeightedMementoTest, HostAdditionAndRemoval) {
  WeightedMementoLbConfig config(65537);
  WeightedMementoTable table(createNormalizedWeights(), config.tableSize());
  
  auto initial_stats = table.getStats();
  std::cout << "Initial: " << initial_stats.total_virtual_nodes << " virtual nodes" << std::endl;
  
  // Add a new host
  auto new_host = makeTestHost(info_, "tcp://127.0.0.1:8003", 4);
  Upstream::NormalizedHostWeightVector weights_with_new_host;
  weights_with_new_host.push_back({hosts_[0], 0.3});   // 3/10
  weights_with_new_host.push_back({hosts_[1], 0.2});   // 2/10
  weights_with_new_host.push_back({hosts_[2], 0.1});   // 1/10
  weights_with_new_host.push_back({new_host, 0.4});    // 4/10
  
  table.update(weights_with_new_host);
  
  auto stats_after_add = table.getStats();
  std::cout << "After adding host: " << stats_after_add.total_virtual_nodes << " virtual nodes" << std::endl;
  
  EXPECT_GT(stats_after_add.total_virtual_nodes, initial_stats.total_virtual_nodes);
  EXPECT_EQ(stats_after_add.total_physical_hosts, 4);
  
  // Remove a host
  Upstream::NormalizedHostWeightVector weights_without_host;
  weights_without_host.push_back({hosts_[0], 0.375});  // 3/8
  weights_without_host.push_back({hosts_[1], 0.25});   // 2/8
  weights_without_host.push_back({new_host, 0.375});   // 3/8 (hosts_[2] removed)
  
  table.update(weights_without_host);
  
  auto stats_after_remove = table.getStats();
  std::cout << "After removing host: " << stats_after_remove.total_virtual_nodes << " virtual nodes" << std::endl;
  
  EXPECT_LT(stats_after_remove.total_virtual_nodes, stats_after_add.total_virtual_nodes);
  EXPECT_EQ(stats_after_remove.total_physical_hosts, 3);
}

TEST_F(WeightedMementoTest, LoadBalancerIntegration) {
  WeightedMementoLbConfig config(65537);
  
  // Create a mock priority set and other dependencies
  Upstream::MockPrioritySet priority_set;
  Upstream::ClusterLbStats stats;
  Stats::IsolatedStoreImpl store;
  Stats::Scope& scope = store.rootScope();
  TestRandomGenerator random;
  TestRuntime runtime;
  
  WeightedMementoLoadBalancer lb(priority_set, stats, scope, runtime, random, 50, config);
  
  // Test load balancer creation
  auto weights = createNormalizedWeights();
  auto load_balancer = lb.createLoadBalancer(weights, 0.0, 1.0);
  
  ASSERT_NE(load_balancer, nullptr);
  
  // Test selection
  Upstream::MockLoadBalancerContext context;
  auto response = load_balancer->chooseHost(&context);
  ASSERT_TRUE(response.host != nullptr);
  
  // Test stats
  auto table_stats = lb.getTableStats();
  EXPECT_GT(table_stats.total_virtual_nodes, 0);
  EXPECT_EQ(table_stats.total_physical_hosts, 3);
  
  std::cout << "Load balancer stats: " << table_stats.total_virtual_nodes 
            << " virtual nodes, " << table_stats.total_physical_hosts << " physical hosts" << std::endl;
}

} // namespace Memento
} // namespace LoadBalancingPolicies
} // namespace Extensions
} // namespace Envoy
