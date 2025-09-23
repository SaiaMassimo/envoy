#include <memory>

#include "envoy/config/cluster/v3/cluster.pb.h"

#include "source/extensions/load_balancing_policies/memento/memento_lb.h"

#include "test/common/upstream/utility.h"
#include "test/mocks/common.h"
#include "test/mocks/server/server_factory_context.h"
#include "test/mocks/upstream/cluster_info.h"
#include "test/mocks/upstream/host.h"
#include "test/mocks/upstream/host_set.h"
#include "test/mocks/upstream/load_balancer_context.h"
#include "test/mocks/upstream/priority_set.h"
#include "test/test_common/simulated_time_system.h"
#include "test/test_common/test_runtime.h"

namespace Envoy {
namespace Upstream {
namespace {

using testing::Return;

class TestLoadBalancerContext : public LoadBalancerContextBase {
public:
  using HostPredicate = std::function<bool(const Host&)>;

  TestLoadBalancerContext(uint64_t hash_key)
      : TestLoadBalancerContext(hash_key, 0, [](const Host&) { return false; }) {}
  TestLoadBalancerContext(uint64_t hash_key, uint32_t retry_count,
                          HostPredicate should_select_another_host)
      : hash_key_(hash_key), retry_count_(retry_count),
        should_select_another_host_(should_select_another_host) {}

  absl::optional<uint64_t> computeHashKey() override { return hash_key_; }
  uint32_t hostSelectionRetryCount() const override { return retry_count_; };
  bool shouldSelectAnotherHost(const Host& host) override {
    return should_select_another_host_(host);
  }

  absl::optional<uint64_t> hash_key_;
  uint32_t retry_count_;
  HostPredicate should_select_another_host_;
};

class MementoLoadBalancerTest : public Event::TestUsingSimulatedTime, public testing::Test {
public:
  MementoLoadBalancerTest()
      : stat_names_(stats_store_.symbolTable()), stats_(stat_names_, *stats_store_.rootScope()) {}

  void createLb() {
    envoy::config::cluster::v3::Cluster::MementoLbConfig legacy;
    Memento::MementoLbConfig typed_config(legacy);

    lb_ = std::make_unique<Memento::MementoLoadBalancer>(priority_set_, stats_,
                                                         *stats_store_.rootScope(),
                                                         context_.runtime_loader_,
                                                         context_.api_.random_, 50, typed_config);
  }

  void init() {
    createLb();
    EXPECT_TRUE(lb_->initialize().ok());
  }

  NiceMock<MockPrioritySet> priority_set_;
  NiceMock<MockPrioritySet> worker_priority_set_;
  LoadBalancerParams lb_params_{worker_priority_set_, {}};

  MockHostSet& host_set_ = *priority_set_.getMockHostSet(0);
  std::shared_ptr<MockClusterInfo> info_{new NiceMock<MockClusterInfo>()};
  Stats::IsolatedStoreImpl stats_store_;
  ClusterLbStatNames stat_names_;
  ClusterLbStats stats_;
  NiceMock<Server::Configuration::MockServerFactoryContext> context_;

  std::unique_ptr<Memento::MementoLoadBalancer> lb_;
};

TEST_F(MementoLoadBalancerTest, NoHost) {
  init();
  EXPECT_EQ(nullptr, lb_->factory()->create(lb_params_)->chooseHost(nullptr).host);
}

TEST_F(MementoLoadBalancerTest, LbDestructedBeforeFactory) {
  init();

  auto factory = lb_->factory();
  lb_.reset();

  EXPECT_NE(nullptr, factory->create(lb_params_));
}

TEST_F(MementoLoadBalancerTest, Basic) {
  host_set_.hosts_ = {
      makeTestHost(info_, "tcp://127.0.0.1:90"), makeTestHost(info_, "tcp://127.0.0.1:91"),
      makeTestHost(info_, "tcp://127.0.0.1:92"), makeTestHost(info_, "tcp://127.0.0.1:93"),
      makeTestHost(info_, "tcp://127.0.0.1:94"), makeTestHost(info_, "tcp://127.0.0.1:95")};
  host_set_.healthy_hosts_ = host_set_.hosts_;
  host_set_.runCallbacks({}, {});
  init();

  LoadBalancerPtr lb = lb_->factory()->create(lb_params_);
  for (uint32_t i = 0; i < 32; ++i) {
    TestLoadBalancerContext context(i);
    EXPECT_NE(nullptr, lb->chooseHost(&context).host);
  }
}

TEST_F(MementoLoadBalancerTest, BasicWithRetryHostPredicate) {
  host_set_.hosts_ = {
      makeTestHost(info_, "tcp://127.0.0.1:90"), makeTestHost(info_, "tcp://127.0.0.1:91"),
      makeTestHost(info_, "tcp://127.0.0.1:92"), makeTestHost(info_, "tcp://127.0.0.1:93"),
      makeTestHost(info_, "tcp://127.0.0.1:94"), makeTestHost(info_, "tcp://127.0.0.1:95")};
  host_set_.healthy_hosts_ = host_set_.hosts_;
  host_set_.runCallbacks({}, {});
  init();

  LoadBalancerPtr lb = lb_->factory()->create(lb_params_);
  {
    TestLoadBalancerContext context(10);
    EXPECT_NE(nullptr, lb->chooseHost(&context).host);
  }
  {
    TestLoadBalancerContext context(10, 2, [](const Host&) { return false; });
    EXPECT_NE(nullptr, lb->chooseHost(&context).host);
  }
  {
    TestLoadBalancerContext context(10, 2, [&](const Host& host) { return &host == host_set_.hosts_[1].get(); });
    EXPECT_NE(nullptr, lb->chooseHost(&context).host);
  }
  {
    TestLoadBalancerContext context(10, 2, [](const Host&) { return true; });
    EXPECT_NE(nullptr, lb->chooseHost(&context).host);
  }
}

} // namespace
} // namespace Upstream
} // namespace Envoy


