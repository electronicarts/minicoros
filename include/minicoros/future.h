/// Copyright (C) 2022 Electronic Arts Inc.  All rights reserved.

#ifndef MINICOROS_FUTURE_H_
#define MINICOROS_FUTURE_H_

#ifdef MINICOROS_CUSTOM_INCLUDE
  #include MINICOROS_CUSTOM_INCLUDE
#endif

#include <minicoros/continuation_chain.h>
#include <minicoros/types.h>
#include <minicoros/detail/operation_helpers.h>

#ifdef MINICOROS_USE_EASTL
  #include <eastl/type_traits.h>
  #include <eastl/variant.h>
  #include <eastl/memory.h>
  #include <eastl/shared_ptr.h>

  #ifndef MINICOROS_STD
    #define MINICOROS_STD eastl
  #endif
#else
  #include <type_traits>
  #include <variant>
  #include <memory>

  #ifndef MINICOROS_STD
    #define MINICOROS_STD std
  #endif
#endif

namespace mc {

template<typename T>
class result;

namespace detail {

template<typename T>
struct is_result : public MINICOROS_STD::false_type {};

template<typename T>
struct is_result<mc::result<T>> : public MINICOROS_STD::true_type {};

template<typename T>
constexpr bool is_result_v = is_result<T>::value;

template<typename ReturnedType>
struct resulting_successful_type {
  static constexpr bool CallbackHasCorrectReturnType = is_result_v<ReturnedType> || MINICOROS_STD::is_void_v<ReturnedType>;
  static_assert(CallbackHasCorrectReturnType, "Invalid return type in .then handler. Please explicitly set the return type to `mc::result<...>`.");
};

template<typename ResultType>
struct resulting_successful_type<mc::result<ResultType>> {
  using type = ResultType;
};

/// To simplify common usage, returning `void` directly from a then handler is OK.
/// This decision causes an exception to the general rule that handlers should always return `mc::result`.
template<>
struct resulting_successful_type<void> {
  using type = void;
};

template<typename LambdaType>
auto resulting_type_from_successful_callback(LambdaType&& lambda) -> typename resulting_successful_type<decltype(return_type(MINICOROS_STD::forward<LambdaType>(lambda)))>::type;


/// Struct used to map the return type of fail handlers to a naked type that can be used in a future.
/// Ie;
///   mc::result<T> -> T
///   mc::failure -> FallbackType
///   ... -> error
template<typename ReturnedType, typename FallbackType>
struct resulting_failure_type {
  static constexpr bool CallbackHasCorrectReturnType = is_result_v<ReturnedType> || MINICOROS_STD::is_same_v<ReturnedType, mc::failure>;
  static_assert(CallbackHasCorrectReturnType, "Invalid return type in .fail handler. Please `return mc::failure(...)` to rethrow the error or explicitly set the return type to `mc::result<...>` to recover with a new successful value.");
};

template<typename ResultType, typename FallbackType>
struct resulting_failure_type<mc::result<ResultType>, FallbackType> {
  using type = ResultType;
};

/// Specialization for `failure` so that fail handlers can be created without knowing
/// the return type of the future. Useful for reusable failure handlers.
template<typename FallbackType>
struct resulting_failure_type<mc::failure, FallbackType> {
  using type = FallbackType;
};

template<typename FallbackType, typename CallbackType>
auto resulting_type_from_failure_callback(CallbackType&& callback) -> typename resulting_failure_type<decltype(callback(MINICOROS_STD::declval<MINICOROS_ERROR_TYPE>())), FallbackType>::type;

} // detail

/// Represents a lazily evaluated process which can be composed of multiple sub-processes ("callbacks") and that
/// eventually results in a value of type `T`.
/// Each callback can decide to return immediately/synchronously or later/asynchronously.
/// Callbacks are arranged in a chain/pipeline and interact using arguments and return values -- callback 1
/// returns value X which is given to callback 2 as an argument.
/// Callbacks can also return failures which are propagated through the chain in a similar fashion as the return values.
///
/// ```cpp
/// future<int>([] (promise<int> p) {
///   p(6581);
/// })
/// .then([](int value) -> result<std::string> {
///   return "text";
/// })
/// .then([](std::string value) -> result<void> {
///   return failure(ENOENT);
/// })
/// .fail([](int error_code) {
///   log("error!");
///   return failure(std::move(error_code));
/// });
/// ```
///
/// The `future` class is a monad that wraps a `continuation_chain`. Basically it's syntactic sugar
/// on top of the continuation chain to make it easier to use. It also has support for exception-like
/// error handling.
template<typename T>
class future {
public:
  static_assert(MINICOROS_STD::is_void_v<T> || MINICOROS_STD::is_copy_constructible_v<T>, "Type must be copy-constructible"); // TODO: unfortunately we need copy-constructors. Get rid of that (might need move-only std::function)
  static_assert(MINICOROS_STD::is_void_v<T> || MINICOROS_STD::is_move_constructible_v<T>, "Type must be move-constructible");

  future(continuation<promise<T>>&& callback) : chain_(MINICOROS_STD::move(callback)) {}
  future(continuation_chain<concrete_result<T>>&& chain) : chain_(MINICOROS_STD::move(chain)) {}

  future(const future&) = delete;
  future& operator =(const future&) = delete;

  future(future&& other) : chain_(MINICOROS_STD::move(other.chain_)) {}
  future& operator =(future&& other) {chain_ = MINICOROS_STD::move(other.chain_); return *this; }

  ~future() {
    if (!chain_.evaluated())
      MINICOROS_STD::move(*this).ignore_result();
  }

  using type = T;

  /// Creates a new future by transforming this future through the given callback.
  /// The callback will be invoked with the resulting value of this future iff it's successful, otherwise
  /// execution will propagate through to the next callback.
  /// The callback must either return `mc::result<A>` (which transforms this future to a `future<A>`), or
  /// `void`, ie, no return statement.
  /// The callback must accept as an argument the resulting value from this future.
  ///
  /// ```cpp
  /// future<int>(...)
  ///   .then([](int&& resulting_value) -> mc::result<std::string> {
  ///     return "hello";
  ///   })
  ///   .then([](std::string&& resulting_value) {
  ///     ...
  ///   })
  ///   .then([] {
  ///     ...
  ///   });
  /// ```
  template<typename CallbackType>
  [[nodiscard]] auto then(CallbackType&& callback) && {
    using ReturnType = decltype(detail::resulting_type_from_successful_callback(MINICOROS_STD::forward<CallbackType>(callback)));

    // Transform the continuation chain...
    auto new_chain = MINICOROS_STD::move(chain_).template transform<concrete_result<ReturnType>>([callback = MINICOROS_STD::forward<CallbackType>(callback)](concrete_result<T>&& result, promise<ReturnType>&& promise) mutable {
      if (result.success()) {
        result.resolve_promise_with_callback(MINICOROS_STD::forward<CallbackType>(callback), MINICOROS_STD::move(promise));
      }
      else {
        promise(MINICOROS_STD::move(*result.get_failure()));
      }
    });

    // ... and return it wrapped in a future
    return future<ReturnType>{MINICOROS_STD::move(new_chain)};
  }

  /// "Embraids" the given future with this future. Will evaluate the given future
  /// when this future has successfully evaluated.
  template<typename ReturnType>
  [[nodiscard]] auto then(future<ReturnType>&& coro) && {
    // We only need the continuation_chain inside the lambda, so peel off the future. This makes it possible
    // for future to be a move-only type -- continuation_chain has a stub copy ctor to work with std::function.
    auto new_chain = MINICOROS_STD::move(chain_).template transform<concrete_result<ReturnType>>([coro_chain = MINICOROS_STD::move(coro).chain()](concrete_result<T>&& result, promise<ReturnType>&& promise) mutable {
      if (result.success()) {
        MINICOROS_STD::move(coro_chain).evaluate_into(MINICOROS_STD::move(promise));
      }
      else {
        promise(MINICOROS_STD::move(*result.get_failure())); // Forward just the failure, not the successful return type
      }
    });

    return future<ReturnType>{MINICOROS_STD::move(new_chain)};
  }

  /// Creates a new future by transforming this future through the given callback.
  /// The callback is executed iff this future has resulted in a failure, otherwise execution will be
  /// propagated to the next callback.
  /// A fail callback can return either `mc::result<T>` to transform the value or raise a new error, or
  /// `failure` to propagate the existing or a transformed error.
  ///
  /// ```cpp
  ///   future<std::string>(...)
  ///     .then([] (std::string&&) -> mc::result<std::string> {
  ///       return failure(ENOENT);
  ///     })
  ///     .fail([] (int&& error_code) -> mc::result<std::string> {
  ///       return failure(EBUSY); // Remap the error
  ///     })
  ///     .fail([] (int&& error_code) {
  ///       log() << "received error: " << error_code;
  ///       return failure(error_code);
  ///     })
  ///     .fail([] (int&& error_code) -> mc::result<std::string> {
  ///       return "success"; // Recover from the error
  ///     });
  /// ```
  template<typename CallbackType>
  [[nodiscard]] auto fail(CallbackType&& callback) && {
    using ReturnType = decltype(detail::resulting_type_from_failure_callback<T>(MINICOROS_STD::forward<CallbackType>(callback)));

    // Transform the continuation chain...
    auto new_chain = MINICOROS_STD::move(chain_).template transform<concrete_result<ReturnType>>([callback = MINICOROS_STD::forward<CallbackType>(callback)] (concrete_result<T>&& result, promise<ReturnType>&& promise) mutable {
      if (result.success()) {
        promise(MINICOROS_STD::move(result));
      }
      else {
        // We received a failure, so invoke the callback
        mc::result<ReturnType> res{callback(MINICOROS_STD::move(result.get_failure()->error))};
        res.resolve_promise(MINICOROS_STD::move(promise));
      }
    });

    // ... and return it wrapped in a future
    return future<ReturnType>{MINICOROS_STD::move(new_chain)};
  }

  /// Called regardless of success or failure. A `concrete_result<T>` will be passed to
  /// the callback, and the callback is expected to return a `concrete_result<A>`.
  template<typename CallbackType>
  [[nodiscard]] auto map(CallbackType&& callback) && {
    using ReturnType = decltype(callback(MINICOROS_STD::declval<concrete_result<T>>()));
    static_assert(is_concrete_result_v<ReturnType>, "Callback must return concrete_result<...>");

    using WrappedType = typename ReturnType::type;

    auto new_chain = MINICOROS_STD::move(chain_).template transform<ReturnType>([callback = MINICOROS_STD::forward<CallbackType>(callback)] (concrete_result<T>&& result, promise<WrappedType>&& promise) mutable {
      promise(callback(MINICOROS_STD::move(result)));
    });

    return future<WrappedType>{MINICOROS_STD::move(new_chain)};
  }

  template<typename CallbackType>
  [[nodiscard]] auto finally(CallbackType&& callback) && {
    return MINICOROS_STD::move(*this).map(MINICOROS_STD::forward<CallbackType>(callback));
  }

  template<typename CallbackType>
  void done(CallbackType&& callback) && {
    MINICOROS_STD::move(chain_).evaluate_into(MINICOROS_STD::forward<CallbackType>(callback));
  }

  /// Explicitly terminate this chain; we've handled everything we need.
  void ignore_result() && {
    MINICOROS_STD::move(chain_).evaluate_into([] (auto) {});
  }

  /// Transforms this future by executing the downstream callbacks through the given "executor".
  /// An executor is something that has an `operator ()(WorkType&&)` where WorkType is a move-only object
  /// that has an `operator ()()`.
  /// Typically used for enqueuing evaluation on a work queue.
  template<typename ExecutorType>
  [[nodiscard]] future<T> enqueue(ExecutorType&& executor) && {
    // Take the executor by copy
    return MINICOROS_STD::move(chain_).template transform<concrete_result<T>>([executor](concrete_result<T>&& value, promise<T>&& promise) mutable {
      executor([value = MINICOROS_STD::move(value), promise = MINICOROS_STD::move(promise)] () mutable {
        MINICOROS_STD::move(promise)(MINICOROS_STD::move(value));
      });
    });
  }

  template<typename RhsResultType>
  [[nodiscard]] auto operator &&(future<RhsResultType>&& rhs) && {
    using ResultingTupleType = typename detail::tuple_result<T, RhsResultType>::value_type;

    return future<ResultingTupleType>([lhs_chain = MINICOROS_STD::move(*this).chain(), rhs_chain = MINICOROS_STD::move(rhs).chain()](promise<ResultingTupleType>&& p) mutable {
      auto result_builder = MINICOROS_STD::make_shared<detail::tuple_result<T, RhsResultType>>(MINICOROS_STD::move(p));

      MINICOROS_STD::move(lhs_chain).evaluate_into([result_builder] (concrete_result<T>&& result) {
        result_builder->assign_lhs(MINICOROS_STD::move(result));
      });

      MINICOROS_STD::move(rhs_chain).evaluate_into([result_builder] (concrete_result<RhsResultType>&& result) {
        result_builder->assign_rhs(MINICOROS_STD::move(result));
      });
    });
  }

  /// Returns the first result from any of the futures. If the first result is a failure,
  /// `||` will return that failure.
  [[nodiscard]] future<T> operator ||(future<T>&& rhs) && {
    // Unwrap the chains from their future overcoats. Futures aren't copy-constructible, but the chains are. Remove
    // this when we have move-only std::function.
    return future<T>([lhs_chain = MINICOROS_STD::move(*this).chain(), rhs_chain = MINICOROS_STD::move(rhs).chain()](promise<T>&& p) mutable {
      auto result_builder = MINICOROS_STD::make_shared<detail::any_result<T>>(MINICOROS_STD::move(p));

      MINICOROS_STD::move(lhs_chain).evaluate_into([result_builder] (concrete_result<T>&& result) {
        result_builder->assign(MINICOROS_STD::move(result));
      });

      MINICOROS_STD::move(rhs_chain).evaluate_into([result_builder] (concrete_result<T>&& result) {
        result_builder->assign(MINICOROS_STD::move(result));
      });
    });
  }

  /// Evaluates this future and the given future in sequential order. Returns the results. If any future returns a
  /// tuple, the result will be merged.
  template<typename RhsResultType>
  [[nodiscard]] auto operator >>(future<RhsResultType>&& rhs) && {
    using ResultingTupleType = typename detail::tuple_result<T, RhsResultType>::value_type;

    return future<ResultingTupleType>([lhs_chain = MINICOROS_STD::move(*this).chain(), rhs_chain = MINICOROS_STD::move(rhs).chain()](promise<ResultingTupleType>&& p) mutable {
      auto result_builder = MINICOROS_STD::make_shared<detail::tuple_result<T, RhsResultType>>(MINICOROS_STD::move(p));

      MINICOROS_STD::move(lhs_chain).evaluate_into([result_builder, rhs_chain = MINICOROS_STD::move(rhs_chain)] (concrete_result<T>&& result) mutable {
        result_builder->assign_lhs(MINICOROS_STD::move(result));

        MINICOROS_STD::move(rhs_chain).evaluate_into([result_builder] (concrete_result<RhsResultType>&& result) {
          result_builder->assign_rhs(MINICOROS_STD::move(result));
        });
      });
    });
  }

  continuation_chain<concrete_result<T>>&& chain() && {
    return MINICOROS_STD::move(chain_);
  }

  /// Stops the chain from getting evaluated on future destruction.
  void freeze() {
    chain_.reset();
  }

private:
  continuation_chain<concrete_result<T>> chain_;
};

template<typename T>
future<T> make_successful_future(T&& value) {
  return future<T>([value = MINICOROS_STD::forward<T>(value)](promise<T>&& p) mutable {p(MINICOROS_STD::forward<T>(value)); });
}

template<typename T>
future<T> make_successful_future(const T& value) {
  return future<T>([value](promise<T>&& p) {p(T{MINICOROS_STD::move(value)}); });
}

template<typename T>
future<T> make_successful_future(mc::future<T>&& value) {
  return MINICOROS_STD::move(value);
}

template<typename T>
future<void> make_successful_future() {
  return future<void>([](promise<void>&& p) {p({}); });
}

template<typename T>
future<T> make_failed_future(MINICOROS_ERROR_TYPE&& error) {
  return future<T>([error = MINICOROS_STD::move(error)](promise<T>&& p) mutable {p(failure{MINICOROS_STD::move(error)}); });
}

/// Deals with the various types a callback can return:
///
/// ```cpp
/// .then([](int&& value) -> mc::result<float> {
///   // Return a value:
///   return 3.141f;
///
///   // Return a failure:
///   return failure(1234);
///
///   // Return a future:
///   return mc::make_successful_future<float>(1.23f);
/// });
/// ```
template<typename T>
class result {
  using StoredType = MINICOROS_STD::decay_t<T>;

public:
  using type = T;

  result(future<T>&& coro) : value_(MINICOROS_STD::move(coro)) {}

  template<typename OtherType>
  result(OtherType&& value) : value_(StoredType{MINICOROS_STD::move(value)}) {}

  result(failure&& f) : value_(MINICOROS_STD::move(f)) {}

  void resolve_promise(promise<T>&& promise) {
    if (StoredType* value = MINICOROS_STD::get_if<StoredType>(&value_))
      MINICOROS_STD::move(promise)(MINICOROS_STD::move(*value));
    else if (future<T>* coro = MINICOROS_STD::get_if<future<T>>(&value_))
      MINICOROS_STD::move(*coro).chain().evaluate_into(MINICOROS_STD::move(promise));
    else if (failure* f = MINICOROS_STD::get_if<failure>(&value_))
      MINICOROS_STD::move(promise)(MINICOROS_STD::move(*f));
    else
      assert("invalid result state" && 0);
  }

private:
  MINICOROS_STD::variant<StoredType, future<T>, failure> value_;
};

/// Specalization for callbacks that return `result<void>`.
template<>
class result<void> {
  struct success_t {};

public:
  using type = void;

  result() : value_(success_t{}) {}
  result(future<void>&& coro) : value_(MINICOROS_STD::move(coro)) {}
  result(failure&& f) : value_(MINICOROS_STD::move(f)) {}

  void resolve_promise(promise<void>&& promise) {
    if (MINICOROS_STD::get_if<success_t>(&value_))
      MINICOROS_STD::move(promise)({});
    else if (future<void>* coro = MINICOROS_STD::get_if<future<void>>(&value_))
      MINICOROS_STD::move(*coro).chain().evaluate_into(MINICOROS_STD::move(promise));
    else if (failure* f = MINICOROS_STD::get_if<failure>(&value_))
      MINICOROS_STD::move(promise)(MINICOROS_STD::move(*f));
    else
      assert("invalid result state" && 0);
  }

private:
  MINICOROS_STD::variant<success_t, future<void>, failure> value_;
};

} // mc

#endif // MINICOROS_FUTURE_H_
