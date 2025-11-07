/// Copyright (C) 2024 Electronic Arts Inc.  All rights reserved.

#ifndef MINICOROS_ASYNC_FUTURE_H_
#define MINICOROS_ASYNC_FUTURE_H_

#include <minicoros/future.h>
#include <minicoros/operations.h>

namespace mc
{

template<typename T> class async_future;
template<typename T>
async_future<typename detail::vector_result<T>::value_type> when_all(MINICOROS_STD::vector<async_future<T>>&& asyncFutures);

/// A future that must be enqueued on an executor before it can be used for registering handlers (`then`, `fail`, etc).
/// Can be used to enforce execution boundaries.
template<typename T>
class [[nodiscard]] async_future
{
public:
  /// Create an async_future from a future.
  async_future(future<T>&& fut) : future_(MINICOROS_STD::move(fut)) {}

  /// Convert the async_future to an ordinary future by enqueueing it on an executor.
  template<typename ExecutorType>
  future<T> enqueue(ExecutorType&& exec) &&
  {
    return MINICOROS_STD::move(future_).enqueue(MINICOROS_STD::forward<ExecutorType>(exec));
  }

  using type = typename future<T>::type;

  void ignore_result() &&
  {
    MINICOROS_STD::move(future_).ignore_result();
  }

  template<typename LhsType, typename RhsType> friend auto operator &&(future<LhsType>&& lhs, async_future<RhsType>&& rhs);
  template<typename LhsType, typename RhsType> friend auto operator &&(async_future<LhsType>&& lhs, future<RhsType>&& rhs);
  template<typename LhsType, typename RhsType> friend auto operator &&(async_future<LhsType>&& lhs, async_future<RhsType>&& rhs);

  template<typename T2> friend async_future<T2> operator ||(future<T2>&& lhs, async_future<T2>&& rhs);
  template<typename T2> friend async_future<T2> operator ||(async_future<T2>&& lhs, future<T2>&& rhs);
  template<typename T2> friend async_future<T2> operator ||(async_future<T2>&& lhs, async_future<T2>&& rhs);

  friend async_future<typename detail::vector_result<T>::value_type>
    when_all<>(MINICOROS_STD::vector<async_future<T>>&& asyncFutures);

private:
  future<T> future_;
};

template<typename LhsType, typename RhsType>
auto operator &&(future<LhsType>&& lhs, async_future<RhsType>&& rhs)
{
  return async_future(MINICOROS_STD::move(lhs) && MINICOROS_STD::move(rhs.future_));
}

template<typename LhsType, typename RhsType>
auto operator &&(async_future<LhsType>&& lhs, future<RhsType>&& rhs)
{
  return async_future(MINICOROS_STD::move(lhs.future_) && MINICOROS_STD::move(rhs));
}

template<typename LhsType, typename RhsType>
auto operator &&(async_future<LhsType>&& lhs, async_future<RhsType>&& rhs)
{
  return async_future(MINICOROS_STD::move(lhs.future_) && MINICOROS_STD::move(rhs.future_));
}

template<typename T>
async_future<T> operator ||(future<T>&& lhs, async_future<T>&& rhs)
{
  return async_future(MINICOROS_STD::move(lhs) || MINICOROS_STD::move(rhs.future_));
}

template<typename T>
async_future<T> operator ||(async_future<T>&& lhs, future<T>&& rhs)
{
  return async_future(MINICOROS_STD::move(lhs.future_) || MINICOROS_STD::move(rhs));
}

template<typename T>
async_future<T> operator ||(async_future<T>&& lhs, async_future<T>&& rhs)
{
  return async_future(MINICOROS_STD::move(lhs.future_) || MINICOROS_STD::move(rhs.future_));
}

template<typename T>
async_future<typename detail::vector_result<T>::value_type> when_all(MINICOROS_STD::vector<async_future<T>>&& asyncFutures)
{
  MINICOROS_STD::vector<future<T>> futures;
  futures.reserve(asyncFutures.size());

  for (async_future<T>& asyncFuture : asyncFutures)
    futures.push_back(MINICOROS_STD::move(asyncFuture.future_));

  return when_all(eastl::move(futures));
}

}

#endif // MINICOROS_ASYNC_FUTURE_H_
