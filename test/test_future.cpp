/// Copyright (C) 2022 Electronic Arts Inc.  All rights reserved.

#include "testing.h"
#include <minicoros/future.h>
#include <minicoros/testing.h>
#include <memory>
#include <string>
#include <vector>

using namespace testing;

class work_queue {
public:
  void enqueue_work(std::function<void()> item) {
    work_items_.push_back(std::move(item));
  }

  void execute() {
    std::vector<std::function<void()>> items = std::move(work_items_);

    for (auto& item : items)
      item();
  }

private:
  std::vector<std::function<void()>> work_items_;
};

TEST(future, chaining_works) {
  auto count = std::make_shared<int>(0);

  {
    mc::future<int>([](mc::promise<int> p) {
      p(123);
    })
    .then([count](int value) -> mc::result<std::string> {
      ++*count;
      ASSERT_EQ(value, 123);
      return "hullo";
    })
    .then([count](std::string value) -> mc::result<int> {
      ++*count;
      ASSERT_EQ(value, "hullo");
      return 8086;
    })
    .done([](auto) {});
  }

  ASSERT_EQ(*count, 2);
}

TEST(future, can_return_nested_future) {
  auto count = std::make_shared<int>(0);

  {
    mc::future<int>([](mc::promise<int> p) {
      p(123);
    })
    .then([count](int) -> mc::result<std::string> {
      return mc::future<std::string>([count](mc::promise<std::string> p) {
          ++*count;
          p(std::string{"mo"});
        })
        .then([](std::string value) -> mc::result<std::string> {
          return value + "of";
        });
    })
    .then([count](std::string value) -> mc::result<int> {
      ++*count;
      ASSERT_EQ(value, "moof");
      return 8086;
    })
    .done([](auto) {});
  }

  ASSERT_EQ(*count, 2);
}

TEST(future, failures_jump_to_fail_handler) {
  using namespace mc;
  auto num_fail_invocations = std::make_shared<int>();

  future<int>([](promise<int> p) {
    p(123);
  })
  .then([](int) -> mc::result<std::string> {
    return failure(123);
  })
  .then([](std::string) -> mc::result<std::string> {
    TEST_FAIL("Reached a .then handler we shouldn't");
    return "moof";
  })
  .fail([num_fail_invocations](int error_code) -> mc::result<std::string> {
    ASSERT_EQ(error_code, 123);
    ++*num_fail_invocations;
    return failure(1234);
  })
  .fail([num_fail_invocations](int error_code) -> mc::result<std::string> {
    ASSERT_EQ(error_code, 1234);
    ++*num_fail_invocations;
    return failure(444);
  })
  .done([](auto) {});

  ASSERT_EQ(*num_fail_invocations, 2);
}

TEST(future, failures_can_be_recovered) {
  using namespace mc;
  auto num_invocations = std::make_shared<int>();

  future<std::string>([](promise<std::string> p) {
    p(failure(1235));
  })
  .fail([num_invocations](int error_code) -> mc::result<std::string> {
    ASSERT_EQ(error_code, 1235);
    ++*num_invocations;
    return "hullo";
  })
  .fail([](int error_code) -> mc::result<std::string> {
    (void)error_code;
    TEST_FAIL("Reached a .fail handler we shouldn't");
    return failure(444);
  })
  .then([num_invocations](std::string value) -> mc::result<std::string> {
    ASSERT_EQ(value, "hullo");
    ++*num_invocations;
    return "moof";
  })
  .done([](auto) {});

  ASSERT_EQ(*num_invocations, 2);
}

TEST(future, enqueue_executes_through_executor) {
  using namespace mc;

  auto executor = std::make_shared<work_queue>();
  auto put_on_executor = [executor] (std::function<void()> work) {executor->enqueue_work(std::move(work)); };
  auto num_invocations = std::make_shared<int>();

  future<int>([](promise<int> p) {
    p(123);
  })
  .then([num_invocations](int) -> mc::result<int> {
    ++*num_invocations;
    return 444;
  })
  .enqueue(put_on_executor)
  .then([num_invocations](int) -> mc::result<int> {
    ++*num_invocations;
    return failure(123);
  })
  .enqueue(put_on_executor)
  .fail([num_invocations](int) -> mc::result<int> {
    ++*num_invocations;
    return failure(123);
  })
  .done([](auto) {});

  ASSERT_EQ(*num_invocations, 1);
  executor->execute();
  ASSERT_EQ(*num_invocations, 2);
  executor->execute();
  ASSERT_EQ(*num_invocations, 3);
}

TEST(future, success_type_is_deduced) {
  using namespace mc;

  future<std::string>([](promise<std::string> p) {
    p(std::string{"hullo"});
  })
  .then([](std::string) -> result<std::string> {
    return "hey";
  })
  .then([](std::string) -> result<int> {
    return make_successful_future<int>(1234);
  })
  .then([](int) -> result<int> {
    if (1 == 1)
      return make_successful_future<int>(4444);
    else
      return failure(12345);

    return 123;
  })
  .then([](int) -> result<std::string> {
    return "huhu";
  })
  .then([](std::string) -> result<int> {
    return 444;
  })
  .done([](auto) {});
}

TEST(future, then_takes_futures) {
  using namespace mc;
  auto num_invocations = std::make_shared<int>();

  auto nested_coro = make_successful_future<int>(123)
    .then([num_invocations](int value) -> result<int> {
      ++*num_invocations;
      return value + 1;
    });

  make_successful_future<std::string>("hullo")
    .then(std::move(nested_coro))
    .then([num_invocations](int value) -> result<int> {
      ++*num_invocations;
      ASSERT_EQ(value, 124);
      return 8086;
    })
    .done([](auto) {});

  ASSERT_EQ(*num_invocations, 2);
}

TEST(future, then_propagates_future_failures) {
  using namespace mc;
  auto num_invocations = std::make_shared<int>();

  make_successful_future<void>()
    .then(make_failed_future<void>(123456))
    .then([num_invocations] {*num_invocations += 1; })
    .fail([num_invocations](int error_code) {
      ASSERT_EQ(error_code, 123456);
      *num_invocations += 2;
      return failure(123);
    })
    .done([](auto) {});

  ASSERT_EQ(*num_invocations, 2);
}

TEST(future, failure_is_not_propagated_to_future) {
  using namespace mc;
  auto num_invocations = std::make_shared<int>();

  make_failed_future<void>(12345)
    .then(future<void>([num_invocations](promise<void>) {*num_invocations += 1; }))
    .done([](auto) {});

  ASSERT_EQ(*num_invocations, 0);
}

TEST(future, futures_can_return_void) {
  using namespace mc;
  auto num_invocations = std::make_shared<int>();

  {
    future<void> c = make_successful_future<void>()
      .then([num_invocations] {
        ++*num_invocations;
      })
      .then([num_invocations]() -> mc::result<void> {
        ++*num_invocations;
        return make_successful_future<void>();
      });
  }

  ASSERT_EQ(*num_invocations, 2);
}

TEST(future, futures_returning_void_can_be_transformed_to_and_from) {
  using namespace mc;
  auto num_invocations = std::make_shared<int>();

  {
    future<void> c = make_successful_future<void>()
      .then([num_invocations]() -> result<int> {
        ++*num_invocations;
        return 123;
      })
      .then([num_invocations](int value) -> result<void> {
        ASSERT_EQ(value, 123);
        ++*num_invocations;
        return {};
      })
      .then([num_invocations]() -> result<int> {
        ++*num_invocations;
        return 124;
      })
      .then([num_invocations](int value) {
        ASSERT_EQ(value, 124);
        ++*num_invocations;
      });
  }

  ASSERT_EQ(*num_invocations, 4);
}

TEST(future, fail_handler_can_take_untyped_passthrough_callback) {
  auto num_invocations = std::make_shared<int>();

  // Verifies that `fail` can take callbacks that transform the error without knowing about
  // the successful future return type
  mc::make_failed_future<std::string>(12345)
    .fail([](int error) {
      return mc::failure(error + 1);
    })
    .fail([num_invocations](int error_code) {
      ASSERT_EQ(error_code, 12346);
      ++*num_invocations;
      return mc::failure(std::move(error_code));
    })
    .done([](auto) {});

  ASSERT_EQ(*num_invocations, 1);
}

TEST(future, fail_handler_can_recover_with_result_void) {
  auto num_invocations = std::make_shared<int>();

  mc::make_failed_future<std::string>(12345)
    .then([num_invocations](std::string) -> mc::result<void> {
      ++*num_invocations;
      return {};
    })
    .fail([num_invocations](int) -> mc::result<void> {
      *num_invocations += 2;
      return {};
    })
    .fail([num_invocations](int error) {
      *num_invocations += 4;
      return mc::failure(std::move(error));
    })
    .then([num_invocations] {
      *num_invocations += 8;
    })
    .done([](auto) {});

  ASSERT_EQ(*num_invocations, 2 + 8);
}

TEST(future, two_allocations_per_evaluated_then) {
  using namespace mc;
  alloc_counter allocs;

  {
    make_successful_future<int>(8086)
      .then([] (int) -> mc::result<int> {return 123;})
      .then([] (int) {})
      .then([] {})
      .done([](auto) {});
  }

  ASSERT_EQ(allocs.total_allocation_count(), 6);
}

TEST(future, one_allocation_per_unevaluated_then) {
  using namespace mc;
  alloc_counter allocs;

  {
    auto c = make_successful_future<int>(8086)
      .then([] (int) -> mc::result<int> {return 123;})
      .then([] (int) {})
      .then([] {});
    ASSERT_EQ(allocs.total_allocation_count(), 3);
  }
}

TEST(future, two_allocations_per_evaluated_fail) {
  using namespace mc;
  alloc_counter allocs;

  {
    make_failed_future<int>(8086)
      .fail([] (int) {return failure(123);})
      .fail([] (int) {return failure(444);})
      .done([](auto) {});
  }

  ASSERT_EQ(allocs.total_allocation_count(), 4);
}

TEST(future, one_allocation_per_unevaluated_fail) {
  using namespace mc;
  alloc_counter allocs;

  {
    auto c = make_failed_future<int>(8086)
      .fail([] (int) {return failure(123);})
      .fail([] (int) {return failure(444);});
    ASSERT_EQ(allocs.total_allocation_count(), 2);
  }
}

TEST(future, andand_with_two_successful_futures_returns_tuple_successfully) {
  using namespace mc;

  future<std::tuple<int, std::string>> coro = make_successful_future<int>(123) && make_successful_future<std::string>("hello");
  assert_successful_result_eq(std::move(coro), {123, std::string{"hello"}});
}

TEST(future, andand_with_value_and_value_get_concatenated) {
  using namespace mc;

  future<std::tuple<int, bool>> coro = make_successful_future<int>(123) && make_successful_future<bool>(true);
  assert_successful_result_eq(std::move(coro), {123, true});
}

TEST(future, andand_with_value_and_value_can_raise_error) {
  using namespace mc;

  future<std::tuple<int, bool>> fut = make_successful_future<int>(123) && make_successful_future<bool>(true);
  assert_successful_result_eq(std::move(fut), {123, true});

  future<std::tuple<int, bool>> fut2 = make_failed_future<int>(123) && make_successful_future<bool>(true);
  assert_fail_eq(std::move(fut2), 123);

  future<std::tuple<int, bool>> fut3 = make_successful_future<int>(123) && make_failed_future<bool>(444);
  assert_fail_eq(std::move(fut3), 444);
}

TEST(future, andand_with_tuple_and_value_get_concatenated) {
  using namespace mc;

  auto operand = make_successful_future<int>(123) && make_successful_future<std::string>("hello");
  future<std::tuple<int, std::string, bool>> fut = std::move(operand) && make_successful_future<bool>(true);
  assert_successful_result_eq(std::move(fut), {123, std::string{"hello"}, true});
}

TEST(future, andand_with_value_and_tuple_get_concatenated) {
  using namespace mc;

  auto operand = make_successful_future<int>(123) && make_successful_future<std::string>("hello");
  future<std::tuple<bool, int, std::string>> fut = make_successful_future<bool>(true) && std::move(operand);
  assert_successful_result_eq(std::move(fut), {true, 123, std::string{"hello"}});
}

TEST(future, andand_with_tuple_and_tuple_get_concatenated) {
  using namespace mc;

  auto operand1 = make_successful_future<bool>(true) && make_successful_future<bool>(false);
  auto operand2 = make_successful_future<int>(123) && make_successful_future<std::string>("hello");

  future<std::tuple<bool, bool, int, std::string>> fut = std::move(operand1) && std::move(operand2);
  assert_successful_result_eq(std::move(fut), {true, false, 123, std::string{"hello"}});
}

TEST(future, andand_supports_void) {
  using namespace mc;

  {
    future<bool> fut = make_successful_future<bool>(true) && make_successful_future<void>();
    assert_successful_result_eq(std::move(fut), {true});

    future<bool> fut2 = make_failed_future<bool>(333) && make_successful_future<void>();
    assert_fail_eq(std::move(fut2), 333);

    future<bool> fut3 = make_successful_future<bool>(true) && make_failed_future<void>(222);
    assert_fail_eq(std::move(fut3), 222);
  }

  {
    future<bool> coro = make_successful_future<void>() && make_successful_future<bool>(true);
    assert_successful_result_eq(std::move(coro), {true});

    future<bool> coro2 = make_failed_future<void>(555) && make_successful_future<bool>(true);
    assert_fail_eq(std::move(coro2), 555);

    future<bool> coro3 = make_successful_future<void>() && make_failed_future<bool>(555);
    assert_fail_eq(std::move(coro3), 555);
  }

  {
    future<void> coro = make_successful_future<void>() && make_successful_future<void>();
    assert_successful_result(std::move(coro));

    future<void> coro2 = make_failed_future<void>(444) && make_successful_future<void>();
    assert_fail_eq(std::move(coro2), 444);

    future<void> coro3 = make_successful_future<void>() && make_failed_future<void>(444);
    assert_fail_eq(std::move(coro3), 444);
  }
}

TEST(future, oror_resolves_to_first) {
  using namespace mc;

  future<int> coro = make_successful_future<int>(1234) || make_failed_future<int>(444);
  assert_successful_result_eq(std::move(coro), 1234);
}

TEST(future, oror_resolves_to_first_even_if_it_is_a_failure) {
  using namespace mc;

  future<int> coro = make_failed_future<int>(555) || make_successful_future<int>(123);
  assert_fail_eq(std::move(coro), 555);
}

TEST(future, oror_supports_void) {
  using namespace mc;

  future<void> coro = make_successful_future<void>() || make_failed_future<void>(444);
  assert_successful_result(std::move(coro));
}

TEST(future, oror_handles_delayed_results) {
  using namespace mc;

  promise<int> p1, p2;
  bool called = false;

  auto coro1 = future<int>([&](promise<int> p) {p1 = std::move(p); });
  auto coro2 = future<int>([&](promise<int> p) {p2 = std::move(p); });

  (std::move(coro1) || std::move(coro2))
    .fail([&](int error_code) {
      ASSERT_EQ(error_code, 445);
      called = true;
      return failure(std::move(error_code));
    })
    .done([](auto) {});

  ASSERT_FALSE(called);

  p1(failure{445});

  ASSERT_TRUE(called);

  p2(123); // Check that it doesn't crash
}


mc::future<void> make_future(mc::promise<void>& p) {
  return mc::future<void>([&](mc::promise<void> new_promise) {p = std::move(new_promise); });
}

TEST(future, oror_composed_evaluates_all_promises) {
  mc::promise<void> p1, p2, p3;

  (make_future(p1) || make_future(p2) || make_future(p3))
      .done([](auto) {});

  ASSERT_TRUE(bool{p1});
  ASSERT_TRUE(bool{p2});
  ASSERT_TRUE(bool{p3});
}

TEST(future, oror_composed_resolve_on_first_call) {
  mc::promise<void> p1, p2, p3;

  {
    bool called = false;
    (make_future(p1) || make_future(p2) || make_future(p3)).done([&] (auto) {called = true; });
    ASSERT_FALSE(called);

    p1({});
    ASSERT_TRUE(called);
  }

  {
    bool called = false;
    (make_future(p1) || make_future(p2) || make_future(p3)).done([&] (auto) {called = true; });
    ASSERT_FALSE(called);

    p2({});
    ASSERT_TRUE(called);
  }

  {
    bool called = false;
    (make_future(p1) || make_future(p2) || make_future(p3)).done([&] (auto) {called = true; });
    ASSERT_FALSE(called);

    p3({});
    ASSERT_TRUE(called);
  }
}

TEST(future, seq_evalutes_in_order) {
  using namespace mc;

  promise<int> p1;
  promise<bool> p2;
  bool called = false;

  auto coro1 = future<int>([&](promise<int> p) {p1 = std::move(p); });
  auto coro2 = future<bool>([&](promise<bool> p) {p2 = std::move(p); });

  ASSERT_FALSE(bool{p1});

  (std::move(coro1) >> std::move(coro2))
    .then([&](int val1, bool val2) {
      ASSERT_EQ(val1, 123);
      ASSERT_EQ(val2, true);
      called = true;
    })
    .done([] (auto) {});

  ASSERT_FALSE(called);
  ASSERT_TRUE(bool{p1});

  p1(123);

  ASSERT_FALSE(called);
  ASSERT_TRUE(bool{p2});

  p2(true);

  ASSERT_TRUE(called);
}

TEST(future, operations_compose) {
  mc::future<std::tuple<int, bool, std::string, int>> c = (
    (
      (mc::make_successful_future<int>(123) >> mc::make_successful_future<void>())
      &&
      (mc::make_successful_future<bool>(false) || mc::make_successful_future<bool>(true))
    )
    >>
    (
      mc::make_successful_future<std::string>("moof")
      >>
      (mc::make_successful_future<int>(444) || mc::make_successful_future<int>(555))
    )
  );

  assert_successful_result_eq(std::move(c), {123, false, std::string{"moof"}, 444});
}

TEST(future, partial_application_can_take_subsets) {
  auto call_count = std::make_shared<int>();

  (mc::make_successful_future<int>(123) && mc::make_successful_future<bool>(true) && mc::make_successful_future<void>())
    .then([call_count](int v1, bool v2) {
      ++*call_count;
      ASSERT_EQ(v1, 123);
      ASSERT_EQ(v2, true);
    })
    .done([] (auto) {});

  (mc::make_successful_future<int>(123) && mc::make_successful_future<bool>(true) && mc::make_successful_future<void>())
    .then([call_count](int v1) {
      ++*call_count;
      ASSERT_EQ(v1, 123);
    })
    .done([] (auto) {});

  (mc::make_successful_future<int>(123) && mc::make_successful_future<bool>(true) && mc::make_successful_future<void>())
    .then([call_count](int v1) mutable {
      ++*call_count;
      ASSERT_EQ(v1, 123);
    })
    .done([] (auto) {});


  (mc::make_successful_future<int>(123) && mc::make_successful_future<bool>(true) && mc::make_successful_future<void>())
    .then([call_count] {
      ++*call_count;
    })
    .done([] (auto) {});

  ASSERT_EQ(*call_count, 4);
}

TEST(future, can_return_composed_futures) {
  auto call_count = std::make_shared<int>();

  mc::make_successful_future<void>()
    .then([] () -> mc::result<std::tuple<int, int>> {
      return mc::make_successful_future<int>(123) && mc::make_successful_future<int>(444);
    })
    .then([call_count] (int i1, int i2) {
      ASSERT_EQ(i1, 123);
      ASSERT_EQ(i2, 444);
      ++*call_count;
    })
    .done([] (auto) {});

  ASSERT_EQ(*call_count, 1);
}

TEST(future, can_take_mutable_lambdas) {
  mc::make_successful_future<int>(123)
    .then([](int) mutable {

    })
    .fail([] (int) mutable {
      return mc::failure(123);
    })
    .done([] (auto) {});

  mc::make_successful_future<std::string>("hello")
    .then([](std::string) mutable {

    })
    .done([] (auto) {});
}

TEST(future, fails_can_take_auto_parameter) {
  mc::make_successful_future<int>(123)
    .fail([] (auto error) {
      return mc::failure(std::move(error));
    })
    .done([] (auto) {});
}

TEST(future, fails_can_return_void) {
  mc::make_successful_future<void>()
    .fail([] (auto) -> mc::result<void> {
      return {};
    })
    .done([] (auto) {});
}

TEST(future, make_successful_future_takes_untyped_value) {
  mc::make_successful_future<std::vector<int>>({1, 4})
    .then([] (std::vector<int> values) {
      ASSERT_EQ(values.size(), 2);
      ASSERT_EQ(values[0], 1);
      ASSERT_EQ(values[1], 4);
    })
    .done([] (auto) {});

  mc::make_successful_future<std::string>("hello")
    .then([] (std::string value) {
      ASSERT_EQ(value.size(), 5);
    })
    .done([] (auto) {});
}

TEST(future, make_successful_future_takes_copy) {
  std::string value = "hello";
  mc::make_successful_future<std::string>(value)
    .then([] (std::string value) {
      ASSERT_EQ(value.size(), 5);
    })
    .done([] (auto) {});
}

TEST(future, make_successful_future_takes_move) {
  std::string value = "hello";
  mc::make_successful_future<std::string>(std::move(value))
    .then([] (std::string value) {
      ASSERT_EQ(value.size(), 5);
    })
    .done([] (auto) {});
}

TEST(future, finally) {
  auto call_count = std::make_shared<int>();

  {
    mc::future<std::string> c = mc::make_successful_future<std::string>("hello")
      .finally([call_count] (mc::concrete_result<std::string> res) {
        ASSERT_TRUE(res.success());
        ASSERT_EQ(*res.get_value(), "hello");
        ++*call_count;
        return res;
      })
      .finally([call_count] (auto res) {
        ASSERT_TRUE(res.success());
        ASSERT_EQ(*res.get_value(), "hello");
        ++*call_count;
        return res;
      });
  }

  ASSERT_EQ(*call_count, 2);
}

TEST(future, accepts_type_without_default_constructor) {
  class type_without_default_ctor {
  public:
    type_without_default_ctor() = delete;
    type_without_default_ctor(const type_without_default_ctor&) = default;
    type_without_default_ctor(int) {}
  };

  mc::make_successful_future<type_without_default_ctor>(1234)
    .then([] (type_without_default_ctor) {

    })
    .done([] (mc::concrete_result<void>) {});
}

TEST(future, captured_promise_does_not_evaluate_rest_of_chain) {
  // The continuation_chain destructor used to call `evaluate_into` which would evaluate the chain on
  // destruction. That's unexpected and we don't want that.

  auto called = std::make_shared<bool>(false);
  std::unique_ptr<mc::promise<void>> captured_promise;

  {
    // A lazy future that saves its promise into `captured_promise` (which is outside current scope)
    auto fut = mc::future<void>([&captured_promise] (auto promise) {
      captured_promise = std::make_unique<mc::promise<void>>(std::move(promise));
    });

    std::move(fut)
      .then(mc::future<void>([called](auto) {
        *called = true;
      }))
      .done([] (auto) {}); // Evaluate the chain so that the promise is put in `captured_promise`

    ASSERT_FALSE(*called);
  }

  ASSERT_FALSE(*called);

  captured_promise = nullptr;

  ASSERT_FALSE(*called);
}

TEST(future, freeze_makes_future_not_evaluate) {
  bool called = false;

  {
    mc::future<void> fut([&called] (auto) {
      called = true;
    });

    fut.freeze();
  }

  ASSERT_FALSE(called);
}

mc::future<int> foo1() {
  return mc::make_successful_future<int>(1)
    .then([] (int val) -> mc::result<int> {return val + 1; });
}

mc::future<int> foo2() {
  return foo1()
    .then([] (int val) -> mc::result<int> {return val + 1; });
}

TEST(future, functions_compose) {
  int result = 0;

  foo2()
    .then([&] (int val) {result = val; })
    .done([] (auto) {});

  ASSERT_EQ(result, 3);
}
