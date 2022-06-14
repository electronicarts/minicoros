/// Copyright (C) 2022 Electronic Arts Inc.  All rights reserved.
/// Minimal testing framework similar to gtest

#ifndef MINICOROS_TOOLS_TESTING_H_
#define MINICOROS_TOOLS_TESTING_H_

#include <cstdlib>
#include <iostream>
#include <map>
#include <vector>
#include <unordered_set>
#include <unordered_map>

namespace testing {

#define MINICOROS_STR(s) #s
#define MINICOROS_XSTR(s) MINICOROS_STR(s)

#define ASSERT_EQ(val1, val2) if (!((val1) == (val2))) {std::cerr << __FILE__ ":" MINICOROS_XSTR(__LINE__) " FAIL: `" << MINICOROS_XSTR(val1) << "` was " << val1 << " but expected " << val2 << std::endl; std::abort(); }
#define ASSERT_EQ_NOPRINT(val1, val2) if (!((val1) == (val2))) {std::cerr << __FILE__ ":" MINICOROS_XSTR(__LINE__) " comparison FAIL" << std::endl; std::abort(); }
#define ASSERT_TRUE(val1) if (!((val1))) {std::cerr << __FILE__ ":" MINICOROS_XSTR(__LINE__) " FAIL: `" << MINICOROS_XSTR(val1) << "` was " << val1 << " but expected to be truthy" << std::endl; std::abort(); }
#define ASSERT_FALSE(val1) if ((val1)) {std::cerr << __FILE__ ":" MINICOROS_XSTR(__LINE__) " FAIL: `" << MINICOROS_XSTR(val1) << "` was " << val1 << " but expected to be falsy" << std::endl; std::abort(); }
#define TEST_FAIL(text) {std::cerr << __FILE__ ":" MINICOROS_XSTR(__LINE__) " FAIL: " << (text) << std::endl; std::abort(); }

#define TEST_FUNCTION_NAME(suite_name, test_name) test_##suite_name##_##test_name

#define TEST(suite_name, test_name) \
  void TEST_FUNCTION_NAME(suite_name, test_name)(void); \
  static test_registration suite_name##_##test_name##_registration(MINICOROS_STR(suite_name), MINICOROS_STR(test_name), TEST_FUNCTION_NAME(suite_name, test_name)); \
  void TEST_FUNCTION_NAME(suite_name, test_name)(void)

class test_suite {
public:
  test_suite(const char* name) : name_(name) {}

  void add_test(const char* name, void(*fun)());
  void run_tests();

private:
  const char* name_;
  std::vector<std::pair<const char*, void(*)()>> tests_;
};

class test_system {
public:
  test_suite& get_suite(const char* name);
  void run_suites();

  static test_system& instance();

private:
  std::map<std::string, test_suite> suites_;
};

class test_registration {
public:
  test_registration(const char* suite_name, const char* test_name, void(*fun)())
  {
    test_system::instance().get_suite(suite_name).add_test(test_name, fun);
  }
};

class alloc_counter;

class alloc_system {
public:
  void add_counter(alloc_counter* counter);
  void remove_counter(alloc_counter* counter);

  void add_allocation(void* ptr, size_t size);
  void remove_allocation(void* ptr);

  static alloc_system& instance();

private:
  std::unordered_set<alloc_counter*> active_counters_;
};

class alloc_counter {
public:
  alloc_counter();
  ~alloc_counter();

  void add_allocation(void* ptr, size_t size);
  void remove_allocation(void* ptr);

  std::vector<size_t> active_allocations() const;
  int total_allocation_count() const;

private:
  int total_allocation_count_ = 0;
  std::unordered_map<void*, size_t> active_allocations_;
};

} // testing

#endif // !MINICOROS_TOOLS_TESTING_H_
