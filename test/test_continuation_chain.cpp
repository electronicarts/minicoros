/// Copyright (C) 2022 Electronic Arts Inc.  All rights reserved.

#include "testing.h"
#include <minicoros/continuation_chain.h>
#include <memory>
#include <string>

using namespace testing;

TEST(continuation_chain, chain_of_1_element_evalutes_directly_into_the_sink) {
  auto result = std::make_shared<int>();

  mc::continuation_chain<int>([](mc::continuation<int> promise) {
    promise(12345);
  })
  .evaluate_into([result](int value) {
    *result = std::move(value);
  });

  ASSERT_EQ(*result, 12345);
}

TEST(continuation_chain, chain_of_1_element_evalutes_into_sink_when_promise_is_set) {
  auto result = std::make_shared<int>(0);
  mc::continuation<int> saved_promise;

  mc::continuation_chain<int>([&saved_promise](mc::continuation<int> promise) {
    saved_promise = std::move(promise);
  })
  .evaluate_into([result](int value) {
    *result = std::move(value);
  });

  // Nothing should've happened since the promise wasn't triggered
  ASSERT_EQ(*result, 0);

  saved_promise(4433);

  ASSERT_EQ(*result, 4433);
}

TEST(continuation_chain, not_evaluated_when_destructed) {
  auto count = std::make_shared<int>(0);

  {
    auto c = mc::continuation_chain<int>([count](mc::continuation<int> promise) {
        ++*count;
        promise(12345);
      })
      .transform<std::string>([count](int value, mc::continuation<std::string> promise) {
        ASSERT_EQ(value, 12345);
        ++*count;
        promise("hello");
      })
      .transform<std::string>([count](std::string value, mc::continuation<std::string> promise) {
        ASSERT_EQ(value, "hello");
        ++*count;
        promise("moof");
      });

    ASSERT_EQ(*count, 0);
  }

  ASSERT_EQ(*count, 0);
}

TEST(continuation_chain, can_be_moved_into_scope) {
  auto count = std::make_shared<int>(0);
  auto c = mc::continuation_chain<int>([count](mc::continuation<int> promise) {
      ++*count;
      promise(12345);
    });

  {
    std::move(c)
      .transform<std::string>([count](int value, mc::continuation<std::string> promise) {
        ASSERT_EQ(value, 12345);
        ++*count;
        promise("hello");
      })
      .evaluate_into([](auto){});

    ASSERT_EQ(*count, 2);
  }
}

TEST(continuation_chain, evaluation_can_be_disrupted) {
  auto count = std::make_shared<int>(0);
  mc::continuation<std::string> saved_promise;

  mc::continuation_chain<int>([count](mc::continuation<int> promise) {
    ++*count;
    promise(12345);
  })
  .transform<std::string>([count, &saved_promise](int value, mc::continuation<std::string> promise) {
    ASSERT_EQ(value, 12345);
    ++*count;
    saved_promise = std::move(promise);
  })
  .transform<std::string>([count](std::string value, mc::continuation<std::string> promise) {
    ASSERT_EQ(value, "hello");
    ++*count;
    promise("moof");
  })
  .evaluate_into([](auto){});

  // Chain is stuck since the promise we saved hasn't been triggered
  ASSERT_EQ(*count, 2);

  saved_promise("hello");
  ASSERT_EQ(*count, 3);
}
