/// Copyright (C) 2022 Electronic Arts Inc.  All rights reserved.

#ifndef MINICOROS_TESTING_H_
#define MINICOROS_TESTING_H_

#ifdef MINICOROS_USE_EASTL
  #include <eastl/shared_ptr.h>

  #ifndef MINICOROS_STD
    #define MINICOROS_STD eastl
  #endif
#else
  #include <memory>

  #ifndef MINICOROS_STD
    #define MINICOROS_STD std
  #endif
#endif

namespace mc {

template<typename T>
void assert_successful_result_eq(mc::future<T>&& coro, T&& value) {
  auto called = MINICOROS_STD::make_shared<bool>();
  MINICOROS_STD::move(coro).chain().evaluate_into([expected_value = MINICOROS_STD::move(value), called](mc::concrete_result<T>&& value) {
    *called = true;
    ASSERT_TRUE(value.success());
    ASSERT_EQ_NOPRINT(*value.get_value(), expected_value);
  });

  ASSERT_TRUE(*called);
}

inline void assert_successful_result(mc::future<void>&& coro) {
  auto called = MINICOROS_STD::make_shared<bool>();
  MINICOROS_STD::move(coro).chain().evaluate_into([called](mc::concrete_result<void>&& value) {
    *called = true;
    ASSERT_TRUE(value.success());
  });

  ASSERT_TRUE(*called);
}

template<typename T>
void assert_fail_eq(mc::future<T>&& coro, MINICOROS_ERROR_TYPE&& expected_error) {
  auto called = MINICOROS_STD::make_shared<bool>();
  MINICOROS_STD::move(coro).chain().evaluate_into([expected_error = MINICOROS_STD::move(expected_error), called](mc::concrete_result<T>&& value) {
    *called = true;
    ASSERT_FALSE(value.success());
    ASSERT_EQ_NOPRINT(value.get_failure()->error, expected_error);
  });

  ASSERT_TRUE(*called);
}

} // mc

#endif // MINICOROS_TESTING_H_