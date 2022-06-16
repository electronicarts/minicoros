/// Copyright (C) 2022 Electronic Arts Inc.  All rights reserved.

#ifndef MINICOROS_OPERATIONS_H_
#define MINICOROS_OPERATIONS_H_

#ifdef MINICOROS_CUSTOM_INCLUDE
  #include MINICOROS_CUSTOM_INCLUDE
#endif

#include <minicoros/future.h>
#include <minicoros/detail/operation_helpers.h>

#ifdef MINICOROS_USE_EASTL
  #include <eastl/vector.h>
  #include <eastl/tuple.h>
  #include <eastl/memory.h>
  #include <eastl/shared_ptr.h>

  #ifndef MINICOROS_STD
    #define MINICOROS_STD eastl
  #endif
#else
  #include <vector>
  #include <tuple>
  #include <memory>

  #ifndef MINICOROS_STD
    #define MINICOROS_STD std
  #endif
#endif

namespace mc {

namespace detail {

/// Unwrap the chains from their future overcoats. Futures aren't copy-constructible, but the chains are. Remove
/// this when we have move-only std::function.
template<typename T>
MINICOROS_STD::vector<continuation_chain<concrete_result<T>>> unwrap_chains(MINICOROS_STD::vector<future<T>>&& futures) {
  MINICOROS_STD::vector<continuation_chain<concrete_result<T>>> chains;
  chains.reserve(futures.size());

  for (future<T>& fut : futures)
    chains.push_back(MINICOROS_STD::move(fut).chain());

  return chains;
}

} // detail

template<typename T>
auto when_all(MINICOROS_STD::vector<future<T>>&& futures) {
  using ResultType = typename detail::vector_result<T>::value_type;
  auto chains = detail::unwrap_chains(MINICOROS_STD::move(futures));

  return future<ResultType>([chains = MINICOROS_STD::move(chains)](promise<ResultType>&& p) mutable {
    if (chains.empty()) {
      p(concrete_result<ResultType>{});
      return;
    }

    auto result_builder = MINICOROS_STD::make_shared<detail::vector_result<T>>(MINICOROS_STD::move(p));
    result_builder->resize(static_cast<int>(chains.size()));

    for (MINICOROS_STD::size_t i = 0; i < chains.size(); ++i) {
      MINICOROS_STD::move(chains[i]).evaluate_into([i, result_builder] (concrete_result<T>&& result) {
        result_builder->assign(i, MINICOROS_STD::move(result));
      });
    }
  });
}

/// Returns the first result from any of the futures. If the first result is a failure,
/// `when_any` will return that failure.
template<typename T>
auto when_any(MINICOROS_STD::vector<future<T>>&& futures) {
  auto chains = detail::unwrap_chains(MINICOROS_STD::move(futures));

  return future<T>([chains = MINICOROS_STD::move(chains)](promise<T>&& p) mutable {
    if (chains.empty()) {
      p(concrete_result<T>{});
      return;
    }

    auto result_builder = MINICOROS_STD::make_shared<detail::any_result<T>>(MINICOROS_STD::move(p));

    for (MINICOROS_STD::size_t i = 0; i < chains.size(); ++i) {
      MINICOROS_STD::move(chains[i]).evaluate_into([result_builder] (concrete_result<T>&& result) {
        result_builder->assign(MINICOROS_STD::move(result));
      });
    }
  });
}

/// Evaluates the given futures in sequential order and returns all the results.
template<typename T>
auto when_seq(MINICOROS_STD::vector<future<T>>&& futures) {
  using ResultType = typename detail::vector_result<T>::value_type;
  auto chains = detail::unwrap_chains(MINICOROS_STD::move(futures));

  return future<ResultType>([chains = MINICOROS_STD::move(chains)](promise<ResultType>&& p) mutable {
    if (chains.empty()) {
      p(concrete_result<ResultType>{});
      return;
    }

    MINICOROS_STD::make_shared<detail::seq_submitter<T>>(MINICOROS_STD::move(p), MINICOROS_STD::move(chains))->evaluate();
  });
}

} // mc

#endif // MINICOROS_OPERATIONS_H_
