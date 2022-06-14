/// Copyright (C) 2022 Electronic Arts Inc.  All rights reserved.

#include "testing.h"
#include <minicoros/operations.h>
#include <minicoros/testing.h>
#include <memory>
#include <string>
#include <vector>

using namespace testing;
using namespace mc;

TEST(operations_when_all, vector_of_successful_futures_returns_successfully) {
  std::vector<future<int>> v;
  v.push_back(make_successful_future<int>(123));
  v.push_back(make_successful_future<int>(444));
  assert_successful_result_eq(when_all(std::move(v)), {123, 444});
}

TEST(operations_when_all, resolving_promises_in_reverse_order_remaps_values) {
  std::vector<future<int>> v;
  promise<int> p1, p2;
  auto called = std::make_shared<bool>();

  v.push_back(future<int>([&](promise<int> p) {p1 = std::move(p); }));
  v.push_back(future<int>([&](promise<int> p) {p2 = std::move(p); }));

  when_all(std::move(v))
    .then([called](std::vector<int> result) {
      bool eq = result == std::vector<int>{123, 444};
      ASSERT_TRUE(eq);
      *called = true;
    })
    .done([] (auto) {});

  ASSERT_FALSE(*called);

  p2(444);

  ASSERT_FALSE(*called);

  p1(123);

  ASSERT_TRUE(*called);
}

TEST(operations_when_all, empty_vector_returns_immediately) {
  std::vector<future<int>> v;
  assert_successful_result_eq(when_all(std::move(v)), {});
}

TEST(operations_when_all, failure_is_propagated) {
  std::vector<future<int>> v;
  v.push_back(make_successful_future<int>(4));
  v.push_back(make_failed_future<int>(444));
  v.push_back(make_failed_future<int>(456));
  v.push_back(make_successful_future<int>(5));
  assert_fail_eq(when_all(std::move(v)), 444);
}

TEST(operations_when_all, takes_void) {
  std::vector<future<void>> v;
  v.push_back(make_successful_future<void>());
  assert_successful_result(when_all(std::move(v)));
}

TEST(operations_when_any, resolves_to_first_value) {
  promise<int> p1, p2;
  bool called = false;

  std::vector<future<int>> c;
  c.push_back(future<int>([&](promise<int> p) {p1 = std::move(p); }));
  c.push_back(future<int>([&](promise<int> p) {p2 = std::move(p); }));

  future<int> first = when_any(std::move(c));

  std::move(first)
    .then([&](int result) {
      ASSERT_EQ(result, 444);
      called = true;
    })
    .done([] (auto) {});

  ASSERT_FALSE(called);

  p1(444);

  ASSERT_TRUE(called);

  p2(123); // Check that it doesn't crash
}

TEST(operations_when_any, resolves_to_first_result_even_when_it_is_a_failure) {
  promise<int> p1, p2;
  bool called = false;

  std::vector<future<int>> c;
  c.push_back(future<int>([&](promise<int> p) {p1 = std::move(p); }));
  c.push_back(future<int>([&](promise<int> p) {p2 = std::move(p); }));

  future<int> first = when_any(std::move(c));

  std::move(first)
    .fail([&](int error_code) {
      ASSERT_EQ(error_code, 445);
      called = true;
      return failure(std::move(error_code));
    })
    .done([] (auto) {});

  ASSERT_FALSE(called);

  p1(failure{445});

  ASSERT_TRUE(called);

  p2(123); // Check that it doesn't crash
}

TEST(operations_when_any, supports_void) {
  std::vector<future<void>> futures;
  futures.push_back(make_successful_future<void>());
  futures.push_back(make_failed_future<void>(123));

  future<void> fut = when_any(std::move(futures));

  assert_successful_result(std::move(fut));
}

TEST(operations_when_seq, vector_of_successful_futures_returns_successfully) {
  std::vector<future<int>> v;
  v.push_back(make_successful_future<int>(123));
  v.push_back(make_successful_future<int>(444));
  assert_successful_result_eq(when_seq(std::move(v)), {123, 444});
}

TEST(operations_when_seq, futures_are_evaluated_in_order) {
  std::vector<future<int>> v;
  promise<int> p1, p2;
  bool called;

  v.push_back(future<int>([&](promise<int> p) {p1 = std::move(p); }));
  v.push_back(future<int>([&](promise<int> p) {p2 = std::move(p); }));

  when_seq(std::move(v))
    .then([&](std::vector<int> result) {
      bool eq = result == std::vector<int>{444, 123};
      ASSERT_TRUE(eq);
      called = true;
    })
    .done([] (auto) {});

  ASSERT_FALSE(called);
  ASSERT_TRUE(bool{p1});
  ASSERT_FALSE(bool{p2});
  p1(444);

  ASSERT_FALSE(called);
  ASSERT_TRUE(bool{p2});
  p2(123);

  ASSERT_TRUE(called);
}

TEST(operations_when_seq, empty_vector_returns_immediately) {
  std::vector<future<int>> v;
  assert_successful_result_eq(when_seq(std::move(v)), {});
}

TEST(operations_when_seq, failure_is_propagated) {
  std::vector<future<int>> v;
  v.push_back(make_successful_future<int>(4));
  v.push_back(make_failed_future<int>(444));
  v.push_back(make_failed_future<int>(456));
  v.push_back(make_successful_future<int>(5));
  assert_fail_eq(when_seq(std::move(v)), 444);
}

TEST(operations_when_seq, takes_void) {
  std::vector<future<void>> v;
  v.push_back(make_successful_future<void>());
  assert_successful_result(when_seq(std::move(v)));
}
