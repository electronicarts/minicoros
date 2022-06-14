CXX = clang++
CXXFLAGS = -std=c++17 -fno-exceptions -I../include/ -I../tools/ -O3 -Werror -Wall -Wextra -Wpedantic

obj_files = ../tools/testing.o test_continuation_chain.o test_future.o test_operations.o
compile_duration_files = test_compile_duration.o
comparison_files = test_comparison.o

%.o: %.cc ../include/coro.h
	$(CXX) -c $(CXXFLAGS) $< -o $@

test: $(obj_files)
	$(CXX) $(obj_files)

test_compile_duration: $(compile_duration_files)
	$(CXX) $(compile_duration_files)

comparison: $(comparison_files)
	$(CXX) $(comparison_files)

clean:
	rm *.o
