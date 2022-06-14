/// Copyright (C) 2022 Electronic Arts Inc.  All rights reserved.

#include "testing.h"

static bool alloc_reporting_enabled = true;

int main() {
  testing::test_system::instance().run_suites();
}

void* operator new(size_t size) {
  void* ptr = malloc(size);

  if (alloc_reporting_enabled)
    testing::alloc_system::instance().add_allocation(ptr, size);

  return ptr;
}

void operator delete(void* ptr) {
  if (alloc_reporting_enabled)
    testing::alloc_system::instance().remove_allocation(ptr);

  free(ptr);
}

namespace testing {

test_system& test_system::instance() {
  static test_system system;
  return system;
}

void test_system::run_suites() {
  for (auto& [_, suite] : suites_)
    suite.run_tests();
}

test_suite& test_system::get_suite(const char* name) {
  return suites_.emplace(std::string{name}, test_suite{name}).first->second;
}

void test_suite::add_test(const char* name, void(*fun)()) {
  tests_.emplace_back(name, fun);
}

void test_suite::run_tests() {
  std::cout << "[-------------] " << tests_.size() << " tests from " << name_ << std::endl;

  for (auto& [name, fun] : tests_) {
    std::cout << "[ RUN         ] " << name_ << "." << name << std::endl;
    fun();
    std::cout << "[          OK ] " << name_ << "." << name << std::endl;
  }

  std::cout << "[-------------] " << tests_.size() << " tests from " << name_ << std::endl;
}

alloc_system& alloc_system::instance() {
  static alloc_system system;
  return system;
}

void alloc_system::add_counter(alloc_counter* counter) {
  alloc_reporting_enabled = false;
  active_counters_.insert(counter);
  alloc_reporting_enabled = true;
}

void alloc_system::remove_counter(alloc_counter* counter) {
  alloc_reporting_enabled = false;
  active_counters_.erase(counter);
  alloc_reporting_enabled = true;
}

alloc_counter::alloc_counter() {
  alloc_system::instance().add_counter(this);
}

alloc_counter::~alloc_counter() {
  alloc_system::instance().remove_counter(this);
}

void alloc_system::add_allocation(void* ptr, size_t size) {
  alloc_reporting_enabled = false;
  for (alloc_counter* counter : active_counters_) {
    counter->add_allocation(ptr, size);
  }
  alloc_reporting_enabled = true;
}

void alloc_system::remove_allocation(void* ptr) {
  alloc_reporting_enabled = false;
  for (alloc_counter* counter : active_counters_) {
    counter->remove_allocation(ptr);
  }
  alloc_reporting_enabled = true;
}

void alloc_counter::add_allocation(void* ptr, size_t size) {
  active_allocations_.emplace(ptr, size);
  ++total_allocation_count_;
}

void alloc_counter::remove_allocation(void* ptr) {
  active_allocations_.erase(ptr);
}

std::vector<size_t> alloc_counter::active_allocations() const {
  std::vector<size_t> result;
  result.reserve(active_allocations_.size());

  for (auto& [_, size] : active_allocations_)
    result.push_back(size);
  return result;
}

int alloc_counter::total_allocation_count() const {
  return total_allocation_count_;
}

} // testing
