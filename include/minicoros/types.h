/// Copyright (C) 2022 Electronic Arts Inc.  All rights reserved.

#ifndef MINICOROS_TYPES_H_
#define MINICOROS_TYPES_H_

#ifdef MINICOROS_CUSTOM_INCLUDE
  #include MINICOROS_CUSTOM_INCLUDE
#endif

#include <utility>
#include <variant>
#include <optional>
#include <type_traits>
#include <cassert>
#include <tuple>

#ifndef MINICOROS_ERROR_TYPE
  #define MINICOROS_ERROR_TYPE int
#endif

namespace mc {
namespace detail {
// TODO: a less silly way of getting the return type of a lambda; merge with the lambda_helper
template<typename LambdaType>
auto return_type(LambdaType&& lambda) -> decltype(lambda({}, {}, {}, {}, {}, {}, {}, {}));

template<typename LambdaType>
auto return_type(LambdaType&& lambda) -> decltype(lambda({}, {}, {}, {}, {}, {}, {}));

template<typename LambdaType>
auto return_type(LambdaType&& lambda) -> decltype(lambda({}, {}, {}, {}, {}, {}));

template<typename LambdaType>
auto return_type(LambdaType&& lambda) -> decltype(lambda({}, {}, {}, {}, {}));

template<typename LambdaType>
auto return_type(LambdaType&& lambda) -> decltype(lambda({}, {}, {}, {}));

template<typename LambdaType>
auto return_type(LambdaType&& lambda) -> decltype(lambda({}, {}, {}));

template<typename LambdaType>
auto return_type(LambdaType&& lambda) -> decltype(lambda({}, {}));

template<typename LambdaType>
auto return_type(LambdaType&& lambda) -> decltype(lambda({}));

template<typename LambdaType>
auto return_type(LambdaType&& lambda) -> decltype(lambda());

template <typename LambdaType>
struct lambda_helper_impl;

template <typename Ret, typename F, typename... Args>
struct lambda_helper_impl<Ret(F::*)(Args...) const> {
  using return_type = Ret;
  static constexpr int num_arguments = sizeof...(Args);
};

template <typename Ret, typename F, typename... Args>
struct lambda_helper_impl<Ret(F::*)(Args...)> {
  using return_type = Ret;
  static constexpr int num_arguments = sizeof...(Args);
};

template <typename LambdaType, typename = void>
struct lambda_helper : lambda_helper_impl<LambdaType> {};

template <typename LambdaType>
struct lambda_helper<LambdaType, decltype(void(&LambdaType::operator()))> : lambda_helper_impl<decltype(&LambdaType::operator())> {};

/// Helper for counting the number of arguments a callable (most often a lambda in our case) takes
template<typename LambdaType>
struct num_arguments_helper {};

template<typename ReturnType, typename... ArgTypes, typename F>
struct num_arguments_helper<ReturnType (F::*)(ArgTypes...)> {
  static constexpr int num_arguments = sizeof...(ArgTypes);
};

template<typename ReturnType, typename... ArgTypes, typename F>
struct num_arguments_helper<ReturnType (F::*)(ArgTypes...) const> {
  static constexpr int num_arguments = sizeof...(ArgTypes);
};

template<std::size_t... Indexes, typename... Ts>
auto truncate_tuple_impl(std::index_sequence<Indexes...>, std::tuple<Ts...> t) {
  return std::make_tuple(std::get<Indexes>(t)...);
}

/// Returns the first MaxCount elements of the given tuple
template<std::size_t MaxCount, typename... Ts>
auto truncate_tuple(std::tuple<Ts...>&& t) {
  static_assert(MaxCount <= sizeof...(Ts), "cannot take that many elements from the given tuple");
  return truncate_tuple_impl(std::make_index_sequence<MaxCount>(), t);
}

/// Similar to std::apply but tries to apply as many arguments as the receiver takes. It also
/// supports a single non-tuple value.
/// Partial application was measured at one point to cost about ~4% more in test_compile_duration.
/// This specialization is for calling an n-ary lambda using an m-ary tuple where m >= n
template<typename LambdaType, typename... TupleArguments>
auto partial_call(LambdaType&& lambda, std::tuple<TupleArguments...>&& t) {
  auto truncated_tuple = truncate_tuple<detail::lambda_helper<LambdaType>::num_arguments>(std::move(t));
  return std::apply(std::forward<LambdaType>(lambda), std::move(truncated_tuple));
}

/// Specialization for calling a 1-ary lambda using a single value.
template<typename LambdaType, typename T>
auto partial_call(LambdaType&& lambda, T&& t) -> decltype(lambda(std::move(t))) {
  return std::invoke(std::forward<LambdaType>(lambda), std::move(t));
}

/// Specialization for calling a 0-ary lambda using a single value.
template<typename LambdaType, typename T>
auto partial_call(LambdaType&& lambda, T&&) -> decltype(lambda()) {
  return lambda();
}

template<typename LambdaType, typename... TupleArguments>
void partial_call_no_return(LambdaType&& lambda, std::tuple<TupleArguments...>&& t) {
  auto truncated_tuple = truncate_tuple<detail::lambda_helper<LambdaType>::num_arguments>(std::move(t));
  std::apply(std::forward<LambdaType>(lambda), std::move(truncated_tuple));
}

/// Specialization for calling a 1-ary lambda using a single value.
template<typename LambdaType, typename T>
auto partial_call_no_return(LambdaType&& lambda, T&& t) -> decltype((void)lambda(std::move(t))) {
  std::invoke(std::forward<LambdaType>(lambda), std::move(t));
}

/// Specialization for calling a 0-ary lambda using a single value.
template<typename LambdaType, typename T>
auto partial_call_no_return(LambdaType&& lambda, T&& t) -> decltype((void)lambda()) {
  (void)t;
  lambda();
}

}

/// Tag for failures.
struct failure {
  explicit failure(MINICOROS_ERROR_TYPE&& error) : error(std::move(error)) {}
  MINICOROS_ERROR_TYPE error;
};

/// Holds the actual resulting value of a callback (or the failure).
template<typename T>
class concrete_result {
public:
  using type = std::decay_t<T>;

  concrete_result() : value_(type{}) {}
  concrete_result(T&& value) : value_(type{std::move(value)}) {}
  concrete_result(const concrete_result& other) : value_(other.value_) {}
  concrete_result(concrete_result&& other) : value_(std::move(other.value_)) {}
  concrete_result(failure&& f) : value_(std::move(f)) {}

  /// Invokes the callback with this result and resolves the promise using the return value
  /// from the callback.
  template<typename CallbackType, typename PromiseType>
  auto resolve_promise_with_callback(CallbackType&& callback, PromiseType&& promise) -> std::enable_if_t<!std::is_void<typename detail::lambda_helper<CallbackType>::return_type>::value> {
    // General case for handling callbacks that return mc::result<T>
    detail::partial_call(std::forward<CallbackType>(callback), std::move(*std::get_if<type>(&value_)))
      .resolve_promise(std::move(promise));
  }

  template<typename CallbackType, typename PromiseType>
  auto resolve_promise_with_callback(CallbackType&& callback, PromiseType&& promise) -> std::enable_if_t<std::is_void<typename detail::lambda_helper<CallbackType>::return_type>::value> {
    // Callbacks are allowed to return void, and for those we need some special handling
    detail::partial_call_no_return(std::forward<CallbackType>(callback), std::move(*std::get_if<type>(&value_)));
    std::move(promise)({}); // Only going to be used for future<void> since we infer the type from the lambda
  }

  bool success() const {
    return !std::get_if<failure>(&value_);
  }

  type* get_value() {
    return std::get_if<type>(&value_);
  }

  failure* get_failure() {
    return std::get_if<failure>(&value_);
  }

private:
  std::variant<type, failure> value_;
};

/// Specialization for `void` since we don't want to pass any argument to the callback. We also allow the callback to return
/// `void` directly without wrapping it using `result<void>`.
template<>
class concrete_result<void> {
public:
  using type = void;

  concrete_result() = default;
  concrete_result(failure&& f) : failure_(std::move(f)) {}

  template<typename CallbackType, typename PromiseType>
  auto resolve_promise_with_callback(CallbackType&& callback, PromiseType&& promise) -> std::enable_if_t<!std::is_void<decltype(callback())>::value> {
    // General case for handling callbacks that return mc::result<T>
    callback()
      .resolve_promise(std::move(promise));
  }

  template<typename CallbackType, typename PromiseType>
  auto resolve_promise_with_callback(CallbackType&& callback, PromiseType&& promise) -> std::enable_if_t<std::is_void<decltype(callback())>::value> {
    // Callbacks are allowed to return void, and for those we need some special handling
    callback();
    std::move(promise)({});
  }

  bool success() const {
    return !failure_;
  }

  std::optional<failure>&& get_failure() {
    return std::move(failure_);
  }

private:
  std::optional<failure> failure_;
};

template<typename T>
struct is_concrete_result : public std::false_type {};

template<typename T>
struct is_concrete_result<mc::concrete_result<T>> : public std::true_type {};

template<typename T>
constexpr bool is_concrete_result_v = is_concrete_result<T>::value;

template<typename ResultType>
using promise = continuation<concrete_result<ResultType>>;

} // mc

#endif // MINICOROS_TYPES_H_
