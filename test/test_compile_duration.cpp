/// Copyright (C) 2022 Electronic Arts Inc.  All rights reserved.

#include <minicoros/future.h>
#include <string>

using namespace mc;

future<int> doStuff()
{
  return future<int>([](promise<int> p)
    {
      std::move(p)(123);
    });
}

int main()
{
  future<int>([](promise<int> p)
    {
      p(123);
    })
    .then([](int value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int&& value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int&& value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int&& value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int&& value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int&& value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int&& value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int&& value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int&& value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int&& value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int&& value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int&& value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int&& value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int&& value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int&& value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int&& value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int&& value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int&& value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int&& value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int&& value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int&& value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int&& value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int&& value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int&& value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int&& value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int&& value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int&& value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int&& value) -> mc::result<int>
    {
      return doStuff();
    })
    .then([](int&& value) -> mc::result<int>
    {
      return 123;
    })
    .then([](int&& value) -> mc::result<int>
    {
      return 123;
    })
    .then([](int&& value) -> mc::result<int>
    {
      return 123;
    })
    .then([](int&& value) -> mc::result<int>
    {
      return 123;
    })
    .then([](int&& value) -> mc::result<std::string>
    {
      return failure(123);
    })
    .then([](std::string&& value) -> mc::result<std::string>
    {
      return "hello";
    })
    .fail([](int error_code) -> mc::result<std::string>
    {
      return "moofie";
    })
    .then([](std::string&& value) -> mc::result<std::string>
    {
      return failure(123);
    }).then([](std::string&& value) -> mc::result<std::string>
    {
      return "hello";
    })
    .fail([](int error_code) -> mc::result<std::string>
    {
      return "moofie";
    })
    .then([](std::string&& value) -> mc::result<std::string>
    {
      return failure(123);
    }).then([](std::string&& value) -> mc::result<std::string>
    {
      return "hello";
    })
    .fail([](int error_code) -> mc::result<std::string>
    {
      return "moofie";
    })
    .then([](std::string&& value) -> mc::result<std::string>
    {
      return failure(123);
    }).then([](std::string&& value) -> mc::result<std::string>
    {
      return "hello";
    })
    .fail([](int error_code) -> mc::result<std::string>
    {
      return "moofie";
    })
    .then([](std::string&& value) -> mc::result<std::string>
    {
      return failure(123);
    }).then([](std::string&& value) -> mc::result<std::string>
    {
      return "hello";
    })
    .fail([](int error_code) -> mc::result<std::string>
    {
      return "moofie";
    })
    .then([](std::string&& value) -> mc::result<std::string>
    {
      return failure(123);
    }).then([](std::string&& value) -> mc::result<std::string>
    {
      return "hello";
    })
    .fail([](int error_code) -> mc::result<std::string>
    {
      return "moofie";
    })
    .then([](std::string&& value) -> mc::result<std::string>
    {
      return failure(123);
    }).then([](std::string&& value) -> mc::result<std::string>
    {
      return "hello";
    })
    .fail([](int error_code) -> mc::result<std::string>
    {
      return "moofie";
    })
    .then([](std::string&& value) -> mc::result<std::string>
    {
      return failure(123);
    });

}