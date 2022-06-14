/// Copyright (C) 2022 Electronic Arts Inc.  All rights reserved.

#ifndef MINICOROS_TESTING_H_
#define MINICOROS_TESTING_H_

namespace mc {

template<typename T>
void assert_successful_result_eq(mc::future<T>&& coro, T&& value) {
  auto called = std::make_shared<bool>();
  std::move(coro).chain().evaluate_into([expected_value = std::move(value), called](mc::concrete_result<T>&& value) {
    *called = true;
    ASSERT_TRUE(value.success());
    ASSERT_EQ_NOPRINT(*value.get_value(), expected_value);
  });

  ASSERT_TRUE(*called);
}

inline void assert_successful_result(mc::future<void>&& coro) {
  auto called = std::make_shared<bool>();
  std::move(coro).chain().evaluate_into([called](mc::concrete_result<void>&& value) {
    *called = true;
    ASSERT_TRUE(value.success());
  });

  ASSERT_TRUE(*called);
}

template<typename T>
void assert_fail_eq(mc::future<T>&& coro, MINICOROS_ERROR_TYPE&& expected_error) {
  auto called = std::make_shared<bool>();
  std::move(coro).chain().evaluate_into([expected_error = std::move(expected_error), called](mc::concrete_result<T>&& value) {
    *called = true;
    ASSERT_FALSE(value.success());
    ASSERT_EQ_NOPRINT(value.get_failure()->error, expected_error);
  });

  ASSERT_TRUE(*called);
}

} // mc

#endif // MINICOROS_TESTING_H_