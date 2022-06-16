/// Copyright (C) 2022 Electronic Arts Inc.  All rights reserved.

#ifndef MINICOROS_CONTINUATION_CHAIN_H_
#define MINICOROS_CONTINUATION_CHAIN_H_

#ifdef MINICOROS_CUSTOM_INCLUDE
  #include MINICOROS_CUSTOM_INCLUDE
#endif

#ifdef MINICOROS_USE_EASTL
  #include <eastl/utility.h>
  #include <eastl/functional.h>
  #include <cassert>

  #ifndef MINICOROS_STD
    #define MINICOROS_STD eastl
  #endif

  #ifndef MINICOROS_FUNCTION_TYPE
    #define MINICOROS_FUNCTION_TYPE eastl::function
  #endif
#else
  #include <utility>
  #include <functional>
  #include <cassert>

  #ifndef MINICOROS_STD
    #define MINICOROS_STD std
  #endif

  #ifndef MINICOROS_FUNCTION_TYPE
    #define MINICOROS_FUNCTION_TYPE std::function
  #endif
#endif

namespace mc {

template<typename ResultType>
using continuation = MINICOROS_FUNCTION_TYPE<void(ResultType&&)>;

template<typename InputType, typename OutputType>
using functor = MINICOROS_FUNCTION_TYPE<void(InputType&&, continuation<OutputType>&&)>;

/// The continuation chain monad, implements a lazy/async (based on promises) evaluation model and
/// is the core component that this library is built around.
/// Works by creating a chain of "activators" (promise of promises) that gets evaluated bottom-up.
///
/// ```cpp
/// continuation_chain<int>([count](continuation<int>&& c) {
///   c(12345);
/// })
/// .transform<std::string>([count](int&& value, continuation<std::string>&& c) {
///   c("hello");
/// })
/// .evaluate_into([count](std::string&& value) {
///   // ...
/// });
/// ```
template<typename T>
class continuation_chain
{
public:
  continuation_chain(continuation<continuation<T>>&& fun);
  continuation_chain(continuation_chain<T>&& other);
  ~continuation_chain();

  // NOTE: this copy-ctor is needed because std::function requires the lambda to be copy-constructible.
  //       We don't really copy any functions containing continuation_chains, so when we have support for
  //       move-only std::function, remove this copy-ctor.
  continuation_chain(const continuation_chain<T>&) {
    // TODO pbackman: configurable assert
    assert("copying continuation chains isn't supported" && 0);
  }

  /// Appends a functor to the chain, leading to a new chain tail
  template<typename ResultType, typename TransformType /* functor<T, ResultType> */>
  continuation_chain<ResultType> transform(TransformType&& transformation) &&;

  void evaluate_into(continuation<T>&& sink) &&;
  void cancel() &&;

private:
  continuation<continuation<T>> activator_;
};

template<typename T>
continuation_chain<T>::continuation_chain(continuation<continuation<T>>&& fun) : activator_(MINICOROS_STD::move(fun)) {}

template<typename T>
continuation_chain<T>::continuation_chain(continuation_chain<T>&& other) { activator_.swap(other.activator_); }

template<typename T>
continuation_chain<T>::~continuation_chain() { MINICOROS_STD::move(*this).evaluate_into([](T&&){}); }

template<typename T>
template<typename ResultType, typename TransformType>
continuation_chain<ResultType> continuation_chain<T>::transform(TransformType&& transformation) && {
  return continuation_chain<ResultType>{
    [
      transformation = MINICOROS_STD::forward<TransformType>(transformation),
      parent_activator = MINICOROS_STD::move(activator_)
    ]
    (continuation<ResultType>&& next_continuation) mutable {
      parent_activator(
        [
          next_continuation = MINICOROS_STD::move(next_continuation),
          transformation = MINICOROS_STD::forward<TransformType>(transformation)
        ]
        (T&& input) mutable {
          // This gets invoked through the continuation; it's the part of the evaluation flow that actually calls the code and binds it with a continuation
          // that evaluates the next functor of the chain.
          transformation(MINICOROS_STD::move(input), MINICOROS_STD::move(next_continuation));
        }
      );
    }
  };
}

template<typename T>
void continuation_chain<T>::evaluate_into(continuation<T>&& sink) && {
  if (!activator_)
    return;

  activator_(MINICOROS_STD::move(sink));
  activator_ = {};
}

template<typename T>
void continuation_chain<T>::cancel() && {
  activator_ = {};
}

} // mc

#endif // MINICOROS_CONTINUATION_CHAIN_H_
