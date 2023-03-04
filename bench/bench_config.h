
#include <nanobench/nanobench.h>

#include <snitch/snitch.hpp>

namespace bench = ankerl::nanobench;

#define BENCH(NAME, ...) TEST_CASE(NAME, __VA_ARGS__)