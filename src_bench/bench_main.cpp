#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench/nanobench.h>

#include <VCore/Utils/VUtilsBase.h>

#include <atomic>

#include "bench_config.h"

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
struct A1
{
    int a = 0;
};
struct BA1
{
    const A1* ptr = nullptr;
    int b = 1;
};
struct Def
{
    static inline A1 a1{.a = 42};
    const A1* ptr = &a1;
    int b = 2;

    constexpr operator BA1() const
    {
        return BA1{
            .ptr = ptr,
            .b = b,
        };
    };
};

BENCH("test api with operator", "[wgpu]")
{
    int y = 0;
    bench::Bench().run("separate",
        [&]
        {
            bench::doNotOptimizeAway(y);
            A1 a;
            BA1 b{.ptr = &a, y};
            bench::doNotOptimizeAway(b); 
        });
    bench::Bench().run("combined",
        [&]
        {
            bench::doNotOptimizeAway(y);
            BA1 b = Def{.b = y};
            bench::doNotOptimizeAway(b); 
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
    args->arguments.push_back({{}, {"test regex"}, "[wgpu]"});
    snitch::tests.configure(*args);

    bool r = snitch::tests.run_tests(*args) ? 0 : 1;
    return 0;
}