
#include <VCore/Utils/CoreTemplates.h>
#include <VFramework/VEXBase.h>
#include <nanobench/nanobench.h>

#include <bitset>

#include "../bench_config.h"

struct Entity
{
    u64 ComponentMask = 0;
    template <class... TTypes>
    inline bool Has() const noexcept
    {
        u64 buf = (... | TTypes::Id);
        return (ComponentMask & buf) == buf;
    }
    template <class... TTypes>
    inline void SetComponentFlag() noexcept
    {
        ComponentMask |= (... | TTypes::Id);
    }
    FORCE_INLINE constexpr void set_rt(u16* flag_bit, i16 len) noexcept
    {
        for (; len > 0; len--, flag_bit++)
        {
            u16 v = *flag_bit;
            ComponentMask |= (1ull << v);
        }
    }
    FORCE_INLINE constexpr bool test_rt(u16* flag_bit, i16 len) const noexcept
    {
        bool out = true;
        for (; len > 0; len--, flag_bit++)
        {
            u16 v = *flag_bit;
            out &= 0ull != (1ull << v);
        }
        return true;
    }
};

struct CT1
{
    static constexpr u16 Id = 1;
    static constexpr u64 MaskClamped = (1ull << (Id % 64));
};
struct CT2
{
    static constexpr u16 Id = 4;
    static constexpr u64 MaskClamped = (1ull << (Id % 64));
};
struct CT3
{
    static constexpr u16 Id = 34;
    static constexpr u64 MaskClamped = (1ull << (Id % 64));
};
struct CT4
{
    static constexpr u16 Id = 63;
    static constexpr u64 MaskClamped = (1ull << (Id % 64));
};
struct CT1_1
{
    static constexpr u16 Id = 1;
    static constexpr u64 MaskClamped = (1ull << (Id % 64));
};
struct CT2_1
{
    static constexpr u16 Id = 78;
    static constexpr u64 MaskClamped = (1ull << (Id % 64));
};
struct CT3_1
{
    static constexpr u16 Id = 111;
    static constexpr u64 MaskClamped = (1ull << (Id % 64));
};
struct CT4_1
{
    static constexpr u16 Id = 63;
    static constexpr u64 MaskClamped = (1ull << (Id % 64));
};
// constexpr u64 kk_1 = 1ull << 63;
// constexpr u64 kk_2 = 1ull << 64;
template <u8 size>
struct F1
{
    static constexpr auto size_chunks = size;
    static_assert(size_chunks >= 1);
    u64 chunks[size_chunks] = {};
    std::bitset<size * 64> bset;

    template <class... TTypes>
    FORCE_INLINE constexpr void set_v1() noexcept
    {
        (bset.set(std::size_t(TTypes::Id), true), ...);
    }
    template <class... TTypes>
    FORCE_INLINE constexpr bool test_v1() const noexcept
    {
        return (... && bset.test(std::size_t(TTypes::Id)));
    }
};
template <u8 size>
struct F2
{
    static constexpr auto size_chunks = size;
    static_assert(size_chunks >= 1);
    union
    {
        u8 bytes[size_chunks * 8];
        u64 chunks[size_chunks];
    };

    template <class... TTypes>
    FORCE_INLINE constexpr void set_v1() noexcept
    {
        ((chunks[(TTypes::Id / 64)] |= TTypes::MaskClamped), ...);
    }
    template <class... TTypes>
    FORCE_INLINE constexpr bool test_v1() const noexcept
    {
        return (... && (chunks[(TTypes::Id / 64)] & TTypes::MaskClamped));
    }

    FORCE_INLINE constexpr void set_rt(u16* flag_bit, i16 len) noexcept
    {
        for (; len > 0; len--, flag_bit++)
        {
            u16 v = *flag_bit;
            chunks[v / 64] |= (1ull << (v % 64));
        }
    }

    FORCE_INLINE constexpr bool test_rt(u16* flag_bit, i16 len) const noexcept
    {
        for (; len > 0; len--, flag_bit++)
        {
            u16 v = *flag_bit;
            if (0ull != (chunks[v] & (1ull << (v % 64))))
                return false;
        }
        return true;
    }
};

BENCH(" baseline", "[bits]")
{
    Entity aaa{(u64)rand()};

    // =======================================================
    bench::Bench().run("baseline test",
        [&]
        {
            bench::doNotOptimizeAway(aaa);
        });
    bench::Bench().run("baseline set visible",
        [&]
        {
            bench::doNotOptimizeAway(aaa);
            bench::doNotOptimizeAway(aaa);
        });
}
BENCH("Bit test", "[bits]")
{
    Entity aaa{(u64)rand()};

    // =======================================================
    bench::Bench().run("old64 test",
        [&]
        {
            bench::doNotOptimizeAway(aaa);
            bench::doNotOptimizeAway(aaa.Has<CT4, CT1, CT2, CT3>());
        });
    bench::Bench().run("old64 set visible",
        [&]
        {
            bench::doNotOptimizeAway(aaa);
            aaa.SetComponentFlag<CT1, CT2>();
            bench::doNotOptimizeAway(aaa);
        });
}
BENCH("Bit test new", "[bits]")
{
    F1<1> flags1;
    flags1.chunks[0] = rand();
    flags1.bset |= (u32)rand();

    bench::Bench().run("V1 test",
        [&]
        {
            bench::doNotOptimizeAway(flags1);
            bench::doNotOptimizeAway(flags1.test_v1<CT4, CT1, CT2, CT3>());
        });
    bench::Bench().run("V1 set visible",
        [&]
        {
            bench::doNotOptimizeAway(flags1);
            flags1.set_v1<CT1, CT2>();
            bench::doNotOptimizeAway(flags1);
        });
}
//BENCH("Bit test v2", "[bits]")
//{
//    F1<16> flags1;
//    constexpr u64 sz_flags1 = sizeof(flags1);
//    flags1.chunks[0] = rand();
//
//    bench::Bench().run("V1 test (big)",
//        [&]
//        {
//            bench::doNotOptimizeAway(flags1);
//            bench::doNotOptimizeAway(flags1.test_v1<CT4_1, CT1_1, CT2_1, CT3_1>());
//        });
//    bench::Bench().run("V1 set visible (big)",
//        [&]
//        {
//            bench::doNotOptimizeAway(flags1);
//            flags1.set_v1<CT1_1, CT2_1>();
//            bench::doNotOptimizeAway(flags1);
//        });
//
//    F1<16> flags2;
//    flags2.set_v1<CT1_1>();
//    REQUIRE(flags2.chunks[0] == (1ull << CT1_1::Id));
//
//    F1<16> flags3;
//    flags3.set_v1<CT3_1>();
//    REQUIRE(flags3.chunks[1] == (1ull << (CT3_1::Id % 64)));
//    REQUIRE(flags3.test_v1<CT3_1>());
//}

BENCH("Bit test v2", "[bits]")
{
    F2<1> flags1;
    flags1.chunks[0] = rand();

    bench::Bench().run("V2 test",
        [&]
        {
            bench::doNotOptimizeAway(flags1);
            bench::doNotOptimizeAway(flags1.test_v1<CT4, CT1, CT2, CT3>());
        });

    auto r = bench::Bench();
    r.run("V2 set visible",
        [&]
        {
            bench::doNotOptimizeAway(flags1);
            flags1.set_v1<CT1, CT2>();
            bench::doNotOptimizeAway(flags1);
        });
}

BENCH("Bit test RT", "[bits]")
{
    using namespace vex;
    Entity aaa{(u64)rand()};

    u16 bset[7];
    for (auto i : 7_times)
    {
        bset[i] = (u16)rand() % 64;
    }

    // =======================================================
    bench::Bench().run("RT old64 test",
        [&]
        {
            bench::doNotOptimizeAway(bset);
            bench::doNotOptimizeAway(aaa);
            bench::doNotOptimizeAway(aaa.test_rt(bset, 7));
        });
    bench::Bench().run("RT old64 set visible",
        [&]
        {
            bench::doNotOptimizeAway(bset);
            aaa.set_rt(bset, 7);
            bench::doNotOptimizeAway(aaa);
        });
    // =======================================================
    F2<1> flags1;
    flags1.chunks[0] = rand();

    bench::Bench().run("RT V2 test",
        [&]
        {
            bench::doNotOptimizeAway(bset);
            bench::doNotOptimizeAway(flags1);
            bench::doNotOptimizeAway(flags1.test_rt(bset, 7));
        });

    auto r = bench::Bench();
    r.run("RT V2 set visible",
        [&]
        {
            bench::doNotOptimizeAway(bset);
            flags1.set_rt(bset, 7);
            bench::doNotOptimizeAway(flags1);
        });
}