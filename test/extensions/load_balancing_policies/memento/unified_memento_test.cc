// test/extensions/load_balancing_policies/memento/unified_memento_test.cc
#include "source/extensions/load_balancing_policies/memento/memento_table.h"
#include "test/mocks/upstream/cluster_info.h"
#include "test/mocks/upstream/host.h"
#include "gtest/gtest.h"

namespace Envoy {
namespace Extensions {
namespace LoadBalancingPolicies {
namespace Memento {

class UnifiedMementoTest : public testing::Test {
public:
  void SetUp() override {
    // Setup mock cluster info
    info_ = std::make_shared<Upstream::MockClusterInfo>();
    
    // Create test hosts
    host1_ = std::make_shared<Upstream::MockHost>();
    host2_ = std::make_shared<Upstream::MockHost>();
    host3_ = std::make_shared<Upstream::MockHost>();
    
    // Setup host weights
    EXPECT_CALL(*host1_, weight()).WillRepeatedly(testing::Return(1));
    EXPECT_CALL(*host2_, weight()).WillRepeatedly(testing::Return(1));
    EXPECT_CALL(*host3_, weight()).WillRepeatedly(testing::Return(1));
  }

  Upstream::NormalizedHostWeightVector createUniformWeights() {
    Upstream::NormalizedHostWeightVector weights;
    weights.push_back({host1_, 0.333333});  // Uniform weights
    weights.push_back({host2_, 0.333333});
    weights.push_back({host3_, 0.333334});
    return weights;
  }
  
  Upstream::NormalizedHostWeightVector createNonUniformWeights() {
    Upstream::NormalizedHostWeightVector weights;
    weights.push_back({host1_, 0.5});   // Different weights
    weights.push_back({host2_, 0.3});
    weights.push_back({host3_, 0.2});
    return weights;
  }

protected:
  std::shared_ptr<Upstream::MockClusterInfo> info_;
  std::shared_ptr<Upstream::MockHost> host1_, host2_, host3_;
};

TEST_F(UnifiedMementoTest, UnweightedMode) {
  auto weights = createUniformWeights();
  MementoTable table(weights, 65537);
  
  auto stats = table.getStats();
  EXPECT_FALSE(stats.is_weighted_mode);
  EXPECT_EQ(stats.total_physical_hosts, 3);
  EXPECT_EQ(stats.total_virtual_nodes, 3); // 1:1 mapping in unweighted mode
  
  // Test selection
  for (int i = 0; i < 10; ++i) {
    auto response = table.chooseHost(i, 0);
    EXPECT_NE(response.host, nullptr);
  }
}

TEST_F(UnifiedMementoTest, WeightedMode) {
  auto weights = createNonUniformWeights();
  MementoTable table(weights, 65537);
  
  auto stats = table.getStats();
  EXPECT_TRUE(stats.is_weighted_mode);
  EXPECT_EQ(stats.total_physical_hosts, 3);
  EXPECT_GT(stats.total_virtual_nodes, 3); // More virtual nodes in weighted mode
  
  // Test selection
  for (int i = 0; i < 10; ++i) {
    auto response = table.chooseHost(i, 0);
    EXPECT_NE(response.host, nullptr);
  }
}

TEST_F(UnifiedMementoTest, ModeSwitch) {
  // Start with uniform weights (unweighted mode)
  auto uniform_weights = createUniformWeights();
  MementoTable table(uniform_weights, 65537);
  
  auto stats = table.getStats();
  EXPECT_FALSE(stats.is_weighted_mode);
  
  // Switch to non-uniform weights (weighted mode)
  auto non_uniform_weights = createNonUniformWeights();
  table.update(non_uniform_weights);
  
  stats = table.getStats();
  EXPECT_TRUE(stats.is_weighted_mode);
  EXPECT_EQ(stats.total_physical_hosts, 3);
  
  // Switch back to uniform weights (unweighted mode)
  table.update(uniform_weights);
  
  stats = table.getStats();
  EXPECT_FALSE(stats.is_weighted_mode);
  EXPECT_EQ(stats.total_physical_hosts, 3);
}

} // namespace Memento
} // namespace LoadBalancingPolicies
} // namespace Extensions
} // namespace Envoy
