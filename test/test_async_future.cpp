/// Copyright (C) 2022 Electronic Arts Inc.  All rights reserved.

#include "testing.h"
#include <minicoros/async_future.h>
#include <minicoros/testing.h>
#include <memory>
#include <string>
#include <vector>

using namespace testing;

class work_queue {
public:
  void enqueue_work(std::function<void()> item) {
    work_items_.push_back(std::move(item));
  }

  void execute() {
    std::vector<std::function<void()>> items = std::move(work_items_);

    for (auto& item : items)
      item();
  }

private:
  std::vector<std::function<void()>> work_items_;
};

#define ASSERT_TYPE(value, expected) static_assert(std::is_same_v<decltype(value), expected>);

TEST(async_future, enqueue_creates_future) {
  mc::async_future<int> afut = mc::make_successful_future<int>(123);

  auto result = std::move(afut).enqueue([] (std::function<void()>) {});
  ASSERT_TYPE(result, mc::future<int>);
}

TEST(async_future, andand_async_future_with_future_returns_async_future) {
  mc::async_future<int> afut = mc::make_successful_future<int>(123);
  mc::future<std::string> fut = mc::make_successful_future<std::string>("123");
  
  auto result = std::move(afut) && std::move(fut);
  using ExpectedType = mc::async_future<std::tuple<int, std::string>>;
  ASSERT_TYPE(result, ExpectedType);
}

TEST(async_future, andand_future_with_async_future_returns_async_future) {
  mc::async_future<int> afut = mc::make_successful_future<int>(123);
  mc::future<std::string> fut = mc::make_successful_future<std::string>("123");
  
  auto result = std::move(fut) && std::move(afut);
  using ExpectedType = mc::async_future<std::tuple<std::string, int>>;
  ASSERT_TYPE(result, ExpectedType);
}

TEST(async_future, andand_async_future_with_async_future_returns_async_future) {
  mc::async_future<int> afut = mc::make_successful_future<int>(123);
  mc::async_future<std::string> afut2 = mc::make_successful_future<std::string>("123");
  
  auto result = std::move(afut) && std::move(afut2);
  using ExpectedType = mc::async_future<std::tuple<int, std::string>>;
  ASSERT_TYPE(result, ExpectedType);
}

TEST(async_future, oror_async_future_with_future_returns_async_future) {
  mc::async_future<int> afut = mc::make_successful_future<int>(123);
  mc::future<int> fut = mc::make_successful_future<int>(123);
  
  auto result = std::move(afut) || std::move(fut);
  ASSERT_TYPE(result, mc::async_future<int>);
}

TEST(async_future, oror_future_with_async_future_returns_async_future) {
  mc::async_future<int> afut = mc::make_successful_future<int>(123);
  mc::future<int> fut = mc::make_successful_future<int>(123);
  
  auto result = std::move(fut) || std::move(afut);
  ASSERT_TYPE(result, mc::async_future<int>);
}

TEST(async_future, oror_async_future_with_async_future_returns_async_future) {
  mc::async_future<int> afut = mc::make_successful_future<int>(123);
  mc::async_future<int> afut2 = mc::make_successful_future<int>(123);
  
  auto result = std::move(afut) || std::move(afut2);
  ASSERT_TYPE(result, mc::async_future<int>);
}

