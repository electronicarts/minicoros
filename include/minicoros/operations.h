/// Copyright (C) 2022 Electronic Arts Inc.  All rights reserved.

#ifndef MINICOROS_OPERATIONS_H_
#define MINICOROS_OPERATIONS_H_

#ifdef MINICOROS_CUSTOM_INCLUDE
  #include MINICOROS_CUSTOM_INCLUDE
#endif

#include <minicoros/future.h>
#include <minicoros/detail/operation_helpers.h>

#include <vector>
#include <tuple>
#include <memory>

namespace mc {

namespace detail {

/// Unwrap the chains from their future overcoats. Futures aren't copy-constructible, but the chains are. Remove
/// this when we have move-only std::function.
template<typename T>
std::vector<continuation_chain<concrete_result<T>>> unwrap_chains(std::vector<future<T>>&& futures) {
  std::vector<continuation_chain<concrete_result<T>>> chains;
  chains.reserve(futures.size());

  for (future<T>& fut : futures)
    chains.push_back(std::move(fut).chain());

  return chains;
}

} // detail

template<typename T>
auto when_all(std::vector<future<T>>&& futures) {
  using ResultType = typename detail::vector_result<T>::value_type;
  auto chains = detail::unwrap_chains(std::move(futures));

  return future<ResultType>([chains = std::move(chains)](promise<ResultType>&& p) mutable {
    if (chains.empty()) {
      p(concrete_result<ResultType>{});
      return;
    }

    auto result_builder = std::make_shared<detail::vector_result<T>>(std::move(p));
    result_builder->resize(static_cast<int>(chains.size()));

    for (std::size_t i = 0; i < chains.size(); ++i) {
      std::move(chains[i]).evaluate_into([i, result_builder] (concrete_result<T>&& result) {
        result_builder->assign(i, std::move(result));
      });
    }
  });
}

/// Returns the first result from any of the futures. If the first result is a failure,
/// `when_any` will return that failure.
template<typename T>
auto when_any(std::vector<future<T>>&& futures) {
  auto chains = detail::unwrap_chains(std::move(futures));

  return future<T>([chains = std::move(chains)](promise<T>&& p) mutable {
    if (chains.empty()) {
      p(concrete_result<T>{});
      return;
    }

    auto result_builder = std::make_shared<detail::any_result<T>>(std::move(p));

    for (std::size_t i = 0; i < chains.size(); ++i) {
      std::move(chains[i]).evaluate_into([result_builder] (concrete_result<T>&& result) {
        result_builder->assign(std::move(result));
      });
    }
  });
}

/// Evaluates the given futures in sequential order and returns all the results.
template<typename T>
auto when_seq(std::vector<future<T>>&& futures) {
  using ResultType = typename detail::vector_result<T>::value_type;
  auto chains = detail::unwrap_chains(std::move(futures));

  return future<ResultType>([chains = std::move(chains)](promise<ResultType>&& p) mutable {
    if (chains.empty()) {
      p(concrete_result<ResultType>{});
      return;
    }

    std::make_shared<detail::seq_submitter<T>>(std::move(p), std::move(chains))->evaluate();
  });
}

} // mc

#endif // MINICOROS_OPERATIONS_H_
