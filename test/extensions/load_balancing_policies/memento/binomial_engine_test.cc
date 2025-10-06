#include "source/extensions/load_balancing_policies/memento/cpuls/BinomialEngine.h"
#include "test/test_common/simulated_time_system.h"
#include "gtest/gtest.h"

namespace Envoy {
namespace Extensions {
namespace LoadBalancingPolicies {
namespace Memento {

// Hash function semplice per il test
class SimpleHashFunction : public ::HashFunction {
public:
  int64_t hash(const std::string& key, uint64_t seed = 0) const override {
    // Hash semplice basato su std::hash
    std::hash<std::string> hasher;
    return static_cast<int64_t>(hasher(key + std::to_string(seed)));
  }
};

class BinomialEngineTest : public Event::TestUsingSimulatedTime, public testing::Test {
protected:
  void SetUp() override {
    hash_function_ = std::make_unique<SimpleHashFunction>();
  }

  std::unique_ptr<SimpleHashFunction> hash_function_;
};

TEST_F(BinomialEngineTest, TestBucketRangeWith6Hosts) {
  // Test con 6 host come nel test Basic che fallisce
  BinomialEngine engine(6, *hash_function_);
  
  std::cout << "Testing BinomialEngine with 6 hosts" << std::endl;
  std::cout << "Engine size: " << engine.size() << std::endl;
  std::cout << "Enclosing tree filter: " << engine.enclosingTreeFilter() << std::endl;
  std::cout << "Minor tree filter: " << engine.minorTreeFilter() << std::endl;
  
  // Test 1000 chiavi per vedere se restituisce indici fuori range
  std::set<int> bucket_indices;
  int out_of_range_count = 0;
  
  for (int i = 0; i < 1000; ++i) {
    std::string key = std::to_string(i);
    int bucket = engine.getBucket(key);
    bucket_indices.insert(bucket);
    
    if (bucket >= 6) {
      out_of_range_count++;
      std::cout << "OUT OF RANGE: key=" << key << " bucket=" << bucket << " (>= 6)" << std::endl;
    }
  }
  
  std::cout << "Total unique bucket indices: " << bucket_indices.size() << std::endl;
  std::cout << "Bucket indices: ";
  for (int b : bucket_indices) {
    std::cout << b << " ";
  }
  std::cout << std::endl;
  std::cout << "Out of range count: " << out_of_range_count << " out of 1000" << std::endl;
  
  // Verifica che non ci siano indici fuori range
  EXPECT_EQ(0, out_of_range_count) << "BinomialEngine should not return bucket indices >= size";
  
  // Verifica che tutti gli indici siano validi (0-5)
  for (int b : bucket_indices) {
    EXPECT_GE(b, 0) << "Bucket index should be >= 0";
    EXPECT_LT(b, 6) << "Bucket index should be < 6";
  }
}

TEST_F(BinomialEngineTest, TestBucketRangeWith4Hosts) {
  // Test con 4 host come nel test DeltaUpdatesInPlace che fallisce
  BinomialEngine engine(4, *hash_function_);
  
  std::cout << "Testing BinomialEngine with 4 hosts" << std::endl;
  std::cout << "Engine size: " << engine.size() << std::endl;
  std::cout << "Enclosing tree filter: " << engine.enclosingTreeFilter() << std::endl;
  std::cout << "Minor tree filter: " << engine.minorTreeFilter() << std::endl;
  
  // Test 1000 chiavi per vedere se restituisce indici fuori range
  std::set<int> bucket_indices;
  int out_of_range_count = 0;
  
  for (int i = 0; i < 1000; ++i) {
    std::string key = std::to_string(i);
    int bucket = engine.getBucket(key);
    bucket_indices.insert(bucket);
    
    if (bucket >= 4) {
      out_of_range_count++;
      std::cout << "OUT OF RANGE: key=" << key << " bucket=" << bucket << " (>= 4)" << std::endl;
    }
  }
  
  std::cout << "Total unique bucket indices: " << bucket_indices.size() << std::endl;
  std::cout << "Bucket indices: ";
  for (int b : bucket_indices) {
    std::cout << b << " ";
  }
  std::cout << std::endl;
  std::cout << "Out of range count: " << out_of_range_count << " out of 1000" << std::endl;
  
  // Verifica che non ci siano indici fuori range
  EXPECT_EQ(0, out_of_range_count) << "BinomialEngine should not return bucket indices >= size";
  
  // Verifica che tutti gli indici siano validi (0-3)
  for (int b : bucket_indices) {
    EXPECT_GE(b, 0) << "Bucket index should be >= 0";
    EXPECT_LT(b, 4) << "Bucket index should be < 4";
  }
}

TEST_F(BinomialEngineTest, TestAddBucketSequence) {
  // Test la sequenza di addBucket per capire come cresce l'engine
  BinomialEngine engine(1, *hash_function_);
  
  std::cout << "Testing addBucket sequence starting from size=1" << std::endl;
  
  for (int i = 1; i < 6; ++i) {
    int new_bucket = engine.addBucket();
    std::cout << "addBucket() returned: " << new_bucket << ", engine size: " << engine.size() 
              << ", enclosing filter: " << engine.enclosingTreeFilter() 
              << ", minor filter: " << engine.minorTreeFilter() << std::endl;
    
    // Verifica che il nuovo bucket sia valido
    EXPECT_EQ(i, new_bucket) << "addBucket should return the current size as new bucket index";
    EXPECT_EQ(i + 1, engine.size()) << "Engine size should increase by 1";
  }
  
  // Ora testa getBucket con la dimensione finale
  std::cout << "Final engine state - size: " << engine.size() 
            << ", enclosing filter: " << engine.enclosingTreeFilter() 
            << ", minor filter: " << engine.minorTreeFilter() << std::endl;
  
  // Test alcune chiavi per vedere se restituisce indici validi
  for (int i = 0; i < 100; ++i) {
    std::string key = std::to_string(i);
    int bucket = engine.getBucket(key);
    
    if (bucket >= 6) {
      std::cout << "PROBLEM: key=" << key << " bucket=" << bucket << " (>= 6)" << std::endl;
    }
    
    EXPECT_GE(bucket, 0) << "Bucket index should be >= 0";
    EXPECT_LT(bucket, 6) << "Bucket index should be < 6";
  }
}

} // namespace Memento
} // namespace LoadBalancingPolicies
} // namespace Extensions
} // namespace Envoy
