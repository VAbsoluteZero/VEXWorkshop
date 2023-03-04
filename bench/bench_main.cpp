#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench/nanobench.h>
#include "bench_config.h"

#include <atomic>

BENCH("test", "[tag1]")
{
    int y = 0;

    std::atomic<int> x(0);

    bench::Bench().run("compare_exchange_strong",
        [&]
        {
            x.compare_exchange_strong(y, 0);
        });
}
BENCH("test2", "[tag2]")
{
    int y = 0;

    std::atomic<int> x(0);

    bench::Bench().run("compare_exchange_strong",
        [&]
        {
            bench::doNotOptimizeAway(x.compare_exchange_strong(y, 1));
            bench::doNotOptimizeAway(y);
            bench::doNotOptimizeAway(x.compare_exchange_strong(y, 2));
        });
}
// todo : create vex ECS v1 vs v2 comparison
int main(int argc, char** argv)
{ 
    std::optional<snitch::cli::input> args = snitch::cli::parse_arguments(argc, argv);
    if (!args)
    { 
        return 1;
    }

    args->arguments.push_back({"--verbosity", "quiet", "quiet"});
    args->arguments.push_back({{}, {"test regex"}, "[bits]"});
    snitch::tests.configure(*args); 

    bool r = snitch::tests.run_tests(*args) ? 0 : 1; 
    return 0;
}