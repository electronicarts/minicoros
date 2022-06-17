/// Copyright (C) 2022 Electronic Arts Inc.  All rights reserved.

#ifndef MINICOROS_DETAIL_OPERATION_HELPERS_H_
#define MINICOROS_DETAIL_OPERATION_HELPERS_H_

#ifdef MINICOROS_CUSTOM_INCLUDE
  #include MINICOROS_CUSTOM_INCLUDE
#endif

#include <minicoros/types.h>
#include <minicoros/continuation_chain.h>

#ifdef MINICOROS_USE_EASTL
  #include <eastl/tuple.h>
  #include <eastl/utility.h>
  #include <eastl/vector.h>
  #include <eastl/optional.h>
  #include <eastl/shared_ptr.h>

  #ifndef MINICOROS_STD
    #define MINICOROS_STD eastl
  #endif
#else
  #include <tuple>
  #include <utility>
  #include <vector>
  #include <optional>
  #include <memory>

  #ifndef MINICOROS_STD
    #define MINICOROS_STD std
  #endif
#endif

namespace mc::detail {

template<typename T>
MINICOROS_STD::tuple<MINICOROS_STD::remove_reference_t<T>> convert_to_tuple(T&& value) {
  return MINICOROS_STD::tuple<T>(MINICOROS_STD::move(value));
}

template<typename... Args>
MINICOROS_STD::tuple<MINICOROS_STD::remove_reference_t<Args>...> convert_to_tuple(MINICOROS_STD::tuple<Args...>&& tup) {
  return MINICOROS_STD::move(tup);
}

template<typename A, typename B>
auto make_flat_tuple(A&& a, B&& b) {
  auto tup1 = convert_to_tuple(MINICOROS_STD::move(a));
  auto tup2 = convert_to_tuple(MINICOROS_STD::move(b));
  return MINICOROS_STD::tuple_cat(MINICOROS_STD::move(tup1), MINICOROS_STD::move(tup2));
}

template<typename T>
class vector_result {
public:
  using value_type = MINICOROS_STD::vector<T>;

  vector_result(promise<value_type>&& p) : promise_(MINICOROS_STD::move(p)) {}

  void resize(int new_size) {
    values_.resize(new_size); // T has to have a default constructor. TODO: make it work without a default ctor
  }

  void assign(int index, concrete_result<T>&& result) {
    if (auto fail = result.get_failure()) {
      resolve(MINICOROS_STD::move(*fail));
      return;
    }

    values_[index] = MINICOROS_STD::move(*result.get_value());

    if (++num_finished_futures_ == values_.size())
      resolve(MINICOROS_STD::move(values_));
  }

private:
  void resolve(concrete_result<value_type>&& value) {
    if (!promise_)
      return;

    auto promise = MINICOROS_STD::move(promise_);
    promise_ = {};
    promise(MINICOROS_STD::move(value));
  }

  MINICOROS_STD::vector<T> values_;
  size_t num_finished_futures_ = 0;
  promise<value_type> promise_;
};

template<>
class vector_result<void> {
public:
  using value_type = void;

  vector_result(promise<value_type>&& p) : promise_(MINICOROS_STD::move(p)) {}

  void resize(size_t new_size) {
    num_expected_futures_ = new_size;
  }

  void assign(size_t index, concrete_result<void>&& result) {
    (void)index;

    if (auto fail = result.get_failure()) {
      resolve(MINICOROS_STD::move(*fail));
      return;
    }

    if (++num_finished_futures_ == num_expected_futures_)
      resolve({});
  }

  static concrete_result<value_type> empty_value() {
    return {};
  }

private:
  void resolve(concrete_result<value_type>&& value) {
    if (!promise_)
      return;

    auto promise = MINICOROS_STD::move(promise_);
    promise_ = {};
    promise(MINICOROS_STD::move(value));
  }

  size_t num_finished_futures_ = 0;
  size_t num_expected_futures_ = 0;
  promise<void> promise_;
};

template<typename LHS, typename RHS>
class tuple_result {
public:
  using value_type = decltype(make_flat_tuple(MINICOROS_STD::declval<LHS>(), MINICOROS_STD::declval<RHS>()));

  tuple_result(promise<value_type>&& p) : promise_(MINICOROS_STD::move(p)) {}

  void assign_lhs(concrete_result<LHS>&& result) {
    if (auto fail = result.get_failure()) {
      resolve(MINICOROS_STD::move(*fail));
      return;
    }

    lhs_ = MINICOROS_STD::move(*result.get_value());
    check_and_resolve();
  }

  void assign_rhs(concrete_result<RHS>&& result) {
    if (auto fail = result.get_failure()) {
      resolve(MINICOROS_STD::move(*fail));
      return;
    }

    rhs_ = MINICOROS_STD::move(*result.get_value());
    check_and_resolve();
  }

private:
  void check_and_resolve() {
    if (lhs_ && rhs_)
      resolve(make_flat_tuple(MINICOROS_STD::move(*lhs_), MINICOROS_STD::move(*rhs_)));
  }

  void resolve(concrete_result<value_type>&& value) {
    if (!promise_)
      return;

    auto promise = MINICOROS_STD::move(promise_);
    promise_ = {};
    promise(MINICOROS_STD::move(value));
  }

  MINICOROS_STD::optional<LHS> lhs_;
  MINICOROS_STD::optional<RHS> rhs_;
  promise<value_type> promise_;
};

template<typename LHS>
class tuple_result<LHS, void> {
public:
  using value_type = LHS;

  tuple_result(promise<value_type>&& p) : promise_(MINICOROS_STD::move(p)) {}

  void assign_lhs(concrete_result<LHS>&& result) {
    if (auto fail = result.get_failure()) {
      resolve(MINICOROS_STD::move(*fail));
      return;
    }

    lhs_ = MINICOROS_STD::move(*result.get_value());
    check_and_resolve();
  }

  void assign_rhs(concrete_result<void>&& result) {
    if (auto fail = result.get_failure()) {
      resolve(MINICOROS_STD::move(*fail));
      return;
    }

    received_rhs_ = true;
    check_and_resolve();
  }

private:
  void check_and_resolve() {
    if (lhs_ && received_rhs_)
      resolve(MINICOROS_STD::move(*lhs_));
  }

  void resolve(concrete_result<value_type>&& value) {
    if (!promise_)
      return;

    auto promise = MINICOROS_STD::move(promise_);
    promise_ = {};
    promise(MINICOROS_STD::move(value));
  }

  MINICOROS_STD::optional<LHS> lhs_;
  bool received_rhs_ = false;
  promise<value_type> promise_;
};

template<typename RHS>
class tuple_result<void, RHS> {
public:
  using value_type = RHS;

  tuple_result(promise<value_type>&& p) : promise_(MINICOROS_STD::move(p)) {}

  void assign_lhs(concrete_result<void>&& result) {
    if (auto fail = result.get_failure()) {
      resolve(MINICOROS_STD::move(*fail));
      return;
    }

    received_lhs_ = true;
    check_and_resolve();
  }

  void assign_rhs(concrete_result<RHS>&& result) {
    if (auto fail = result.get_failure()) {
      resolve(MINICOROS_STD::move(*fail));
      return;
    }

    rhs_ = MINICOROS_STD::move(*result.get_value());
    check_and_resolve();
  }

private:
  void check_and_resolve() {
    if (received_lhs_ && rhs_)
      resolve(MINICOROS_STD::move(*rhs_));
  }

  void resolve(concrete_result<value_type>&& value) {
    if (!promise_)
      return;

    auto promise = MINICOROS_STD::move(promise_);
    promise_ = {};
    promise(MINICOROS_STD::move(value));
  }

  bool received_lhs_ = false;
  MINICOROS_STD::optional<RHS> rhs_;
  promise<value_type> promise_;
};

template<>
class tuple_result<void, void> {
public:
  using value_type = void;

  tuple_result(promise<value_type>&& p) : promise_(MINICOROS_STD::move(p)) {}

  void assign_lhs(concrete_result<void>&& result) {
    if (auto fail = result.get_failure()) {
      resolve(MINICOROS_STD::move(*fail));
      return;
    }

    received_lhs_ = true;
    check_and_resolve();
  }

  void assign_rhs(concrete_result<void>&& result) {
    if (auto fail = result.get_failure()) {
      resolve(MINICOROS_STD::move(*fail));
      return;
    }

    received_rhs_ = true;
    check_and_resolve();
  }

private:
  void check_and_resolve() {
    if (received_lhs_ && received_rhs_)
      resolve({});
  }

  void resolve(concrete_result<value_type>&& value) {
    if (!promise_)
      return;

    auto promise = MINICOROS_STD::move(promise_);
    promise_ = {};
    promise(MINICOROS_STD::move(value));
  }

  bool received_lhs_ = false;
  bool received_rhs_ = false;
  promise<value_type> promise_;
};

template<typename T>
class any_result {
public:
  using value_type = T;

  any_result(promise<T>&& promise) : promise_(MINICOROS_STD::move(promise)) {}

  /// First invocation resolves the promise
  void assign(concrete_result<T>&& result) {
    if (!promise_)
      return;

    auto promise = MINICOROS_STD::move(promise_);
    promise_ = {};
    promise(MINICOROS_STD::move(result));
  }

private:
  promise<T> promise_;
};

template<typename T>
class seq_submitter : public MINICOROS_STD::enable_shared_from_this<seq_submitter<T>> {
  using ResultingType = typename vector_result<T>::value_type;
  using ChainType = continuation_chain<concrete_result<T>>;

public:
  seq_submitter(promise<ResultingType>&& p, MINICOROS_STD::vector<ChainType>&& chains) : storage_(MINICOROS_STD::move(p)), chains_(MINICOROS_STD::move(chains)) {}

  void evaluate() {
    storage_.resize(chains_.size());
    evaluate_next_chain();
  }

private:
  using MINICOROS_STD::enable_shared_from_this<seq_submitter<T>>::shared_from_this;

  void evaluate_next_chain() {
    if (next_chain_idx_ >= chains_.size()) {
      // Not much to do -- should only happen for the last chain
      return;
    }

    const size_t chain_idx = next_chain_idx_++;
    auto& chain = chains_[chain_idx];

    MINICOROS_STD::move(chain).evaluate_into([shared_this = shared_from_this()] (concrete_result<T>&& result) {
      shared_this->storage_.assign(shared_this->next_chain_idx_ - 1, MINICOROS_STD::move(result));
      shared_this->evaluate_next_chain();
    });
  }

  vector_result<T> storage_;
  MINICOROS_STD::vector<ChainType> chains_;
  size_t next_chain_idx_ = 0u;
};

} // mc::detail

#endif // MINICOROS_DETAIL_OPERATION_HELPERS_H_
