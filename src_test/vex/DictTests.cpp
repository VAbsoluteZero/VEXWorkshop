
#include <VCore/Containers/Dict.h>

#include <unordered_set>
#include <set>
#include <unordered_map>
#include <vector>

#include "../config.h"
//
using namespace vex;

// ============================================================
static constexpr auto def_buf_size = 2048u;
struct FixedBuffAlloc_Wrapper
{
	vex::InlineBufferAllocator<def_buf_size> al;
};
struct Mallocator_Wrapper
{
	vex::Mallocator al;
};
template <u32 buf_size = def_buf_size>
struct BumpAlloc_Wrapper
{
	u8 fixed_buff[buf_size];
	vex::BumpAllocator al{fixed_buff, sizeof fixed_buff};
};
template <typename T, typename M>
struct TypePair
{
	using First = T;
	using Second = M;
};

using StackTestTypesWithAlloc =
	snitch::type_list<TypePair<FixedBuffAlloc_Wrapper, u8>, TypePair<FixedBuffAlloc_Wrapper, i64>,
		TypePair<Mallocator_Wrapper, u8>, TypePair<Mallocator_Wrapper, i64>>;

TEMPLATE_LIST_TEST_CASE(
	"Stack push/pop test (with varying allocators)", "[container][stack]", StackTestTypesWithAlloc)
{
	using namespace vex;

	using AllocWrapper = typename TestType::First;
	using ValueType = typename TestType::Second;
}


struct custom_hasher
{
	static uint32_t hash(const int& val) { return (uint32_t)val; }
};

struct XY
{
	int x = 1;
	int y = 2;
	static int hash(const XY& v) { return v.x; }
};

struct ComplexStruct
{
	bool r = false;
	char padding[64];
	std::string data = "default";
	ComplexStruct() = delete;
	ComplexStruct(const std::string& otherS) { data = otherS; }
};
bool static operator==(const XY& a, const XY& b) { return (a.x == b.x && a.y == b.y); }
bool static operator!=(const XY& a, const XY& b) { return !(a == b); }
bool static operator==(const ComplexStruct& a, const ComplexStruct& b)
{
	return (a.data == b.data && a.r == b.r);
}
bool static operator!=(const ComplexStruct& a, const ComplexStruct& b) { return b != a; }

namespace std
{
	template <>
	struct hash<XY>
	{
		// intentionally returning only X to force predictable collisions
		auto operator()(const XY& v) const { return v.x; }
	};
	template <>
	struct hash<ComplexStruct>
	{
		auto operator()(const ComplexStruct& v) const { return std::hash<std::string>{}(v.data); }
	};
} // namespace std

using namespace vex;
using SimpleTypes = snitch::type_list<Dict<int, long long, custom_hasher>>;
using CollisionTypes = snitch::type_list<Dict<XY, long long, XY>>;
using ComplexTypes = snitch::type_list<Dict<ComplexStruct, ComplexStruct>>;


TEMPLATE_LIST_TEST_CASE(
	"(legacy,gtest)Dictionary - basic operations test (struct)", "[container][dict]", ComplexTypes)
{
	TestType d{3};
	ComplexStruct str_1("test_string");

	d.emplace(str_1, str_1);

	for (int i = 1; i < 17; ++i)
	{
		auto complex = ComplexStruct(std::to_string(i * 10));
		d.emplace(complex, complex);
	}

	REQUIRE(d.contains(str_1));
	REQUIRE(d.contains(ComplexStruct("test_string")));

	REQUIRE_FALSE(d.tryGet(ComplexStruct("none")));
	REQUIRE(d.tryGet(ComplexStruct("test_string")));

	auto opt = d.tryGet(ComplexStruct("test_string"));
	if (opt)
	{
		EXPECT_EQ(opt->data, str_1.data); // empty
	}
}

TEMPLATE_LIST_TEST_CASE(
	"(legacy,gtest)Dictionary - basic operations test (int)", "[container][dict]", SimpleTypes)
{
	using namespace vex;
	TestType d{8};

	EXPECT_NE(d.capacity(), 8);							  // should be prime
	EXPECT_EQ(d.capacity(), util::closestPrimeSearch(8)); // should be prime

	EXPECT_EQ(d.size(), 0); // empty
	d.emplace(1, 22);
	REQUIRE(d.contains(1));
	EXPECT_EQ(d.size(), 1); // exactly one
	EXPECT_EQ(d[1], 22);
	d.emplace(1, 422);
	EXPECT_EQ(d[1], 422);

	d.remove(1);
	EXPECT_EQ(d.size(), 0);		  // empty
	REQUIRE_FALSE(d.contains(1)); // empty

	d[2] = 10;
	d[1] = 101;
	REQUIRE(d.contains(2));
	EXPECT_EQ(d[1], 101);
	EXPECT_EQ(d.size(), 2);
	EXPECT_EQ(d.capacity(), util::closestPrimeSearch(8));

	d.emplace(3, 101);
	EXPECT_EQ(d[1], d[3]);
}

TEMPLATE_LIST_TEST_CASE(
	"(legacy,gtest)Dictionary - collision test", "[container][dict]", CollisionTypes)
{
	TestType d{8};
	EXPECT_NE(d.capacity(), 8);							  // should be prime
	EXPECT_EQ(d.capacity(), util::closestPrimeSearch(8)); // should be prime

	EXPECT_EQ(d.size(), 0); // empty
	XY first = {10, 45};
	XY second = {10, 90};
	XY third = {10, 300};
	XY fourth = {10, 900};

	EXPECT_EQ(XY::hash(first), XY::hash(second));
	EXPECT_NE(first, second);

	d.emplace(first, 22);
	d[second] = 43;

	REQUIRE(d.contains(first));
	REQUIRE(d.contains(second));
	// collision properly handled
	EXPECT_EQ(d[first], 22);
	EXPECT_EQ(d[second], 43);

	d.remove(first);
	REQUIRE_FALSE(d.contains(first));
	REQUIRE(d.contains(second));

	d.remove(second);
	REQUIRE_FALSE(d.contains(second));


	TestType long_d{8};
	std::array<XY, 4> one_list = {first, second, third, fourth};

	for (auto it : one_list)
	{
		long_d[it] = 8;
	}
	for (auto it : one_list)
	{
		REQUIRE(long_d.contains(it));
	}

	long_d.remove(third);
	REQUIRE_FALSE(d.contains(third));
	for (auto it : one_list)
	{
		if (it != third)
		{
			REQUIRE(long_d.contains(it));
		}
	}
}

TEMPLATE_LIST_TEST_CASE(
	"(legacy,gtest)Dictionary - growth test", "[container][dict]", SimpleTypes)
{
	constexpr auto initCap = util::closestPrimeSearch(17);
	constexpr auto prime2 = util::closestPrimeSearch(initCap);
	TestType d(initCap);
	EXPECT_EQ(d.capacity(), initCap); // should be prime

	for (int i = 0; i < initCap; ++i)
	{
		d[i] = i * 10;
	}
	EXPECT_EQ(d.capacity(), initCap);
	REQUIRE(d.contains(0));
	REQUIRE(d.contains(initCap - 1));

	d.emplace(9999, 5656);

	REQUIRE(d.capacity() > initCap);
	for (int i = 0; i < initCap; ++i)
	{
		REQUIRE(d.contains(i));
		EXPECT_EQ(d[i], i * 10.0);
	}
}

TEMPLATE_LIST_TEST_CASE("(legacy,gtest)Dictionary - copy ctor test", "[container][dict]", SimpleTypes)
{
	constexpr auto initCap = util::closestPrimeSearch(17);
	TestType d(initCap);
	TestType dcpy = d;

	for (int i = 0; i < initCap; ++i)
	{
		d[i] = (int)(i * 10);
	}
	EXPECT_EQ(d.capacity(), initCap);
	REQUIRE(d.contains(0));
	REQUIRE(d.contains(initCap - 1));

	d.emplace(9999, 5656);

	EXPECT_EQ(dcpy.size(), 0); // is not affected by original changes

	REQUIRE(d.capacity() > initCap);
	for (int i = 0; i < initCap; ++i)
	{
		REQUIRE(d.contains(i));
		EXPECT_EQ(d[i], i * 10);
	}

	TestType dcpy2 = d;
	for (int i = 0; i < initCap; ++i)
	{
		REQUIRE(d.contains(i) && dcpy2.contains(i));
		EXPECT_EQ(d[i], dcpy2[i]);
	}
	dcpy = dcpy2;
	for (int i = 0; i < initCap; ++i)
	{
		REQUIRE(d.contains(i) && dcpy.contains(i));
		EXPECT_EQ(d[i], dcpy[i]);
	}
}

TEMPLATE_LIST_TEST_CASE(
	"(legacy,gtest)Dictionary - growth and memory corruption test", "[container][dict]", SimpleTypes)
{
	using namespace util;
	const int n = 2000;
	TestType d(30);

	std::unordered_map<int, int> stdDict(n);

	std::set<int> uniqueKeys;
	std::vector<int> uniqueKeysVec;
	uniqueKeysVec.reserve(n);
	for (int i = 0; i < n; ++i)
	{
		int r = randomRange(0, 2000);
		int v = randomRange(8, 16);

		d[r] = v;
		stdDict[r] = v;
		if (uniqueKeys.find(r) == uniqueKeys.end())
			uniqueKeysVec.push_back(r);
		uniqueKeys.insert(r);
	}
	EXPECT_EQ(uniqueKeys.size(), d.size());

	// first pass
	for (auto kv : stdDict)
	{
		REQUIRE(d.contains(kv.first));
		EXPECT_EQ(d[kv.first], kv.second);
	}

	std::unordered_map<int, int> nm = stdDict;

	// remove few
	std::vector<int> removed;
	removed.reserve(n);
	for (int i = 0; i < (n / 4); i++)
	{
		int randKeyIndex = randomRange(0, (int)uniqueKeysVec.size());

		int rkey = uniqueKeysVec[randKeyIndex];

		d.remove(rkey);
		stdDict.erase(rkey);

		removed.push_back(rkey);
		uniqueKeysVec.erase(uniqueKeysVec.begin() + randKeyIndex);
	}

	for (auto it : removed)
	{
		REQUIRE_FALSE(d.contains(it));
	}

	for (auto kv : stdDict)
	{
		REQUIRE(d.contains(kv.first));
		EXPECT_EQ(d[kv.first], kv.second);
	}
}

TEMPLATE_LIST_TEST_CASE(
	"(legacy,gtest)Dictionary - collision growth and mem corruption test", "[container][dict]", CollisionTypes)
{
	using namespace util;
	const int n = 2000;
	TestType d(30);

	std::unordered_map<XY, int> stdDict(n);

	std::unordered_set<XY> uniqueKeys;
	std::vector<XY> uniqueKeysVec;
	uniqueKeysVec.reserve(n);
	for (int i = 0; i < n; ++i)
	{
		int r = randomRange(0, 2000);
		int v = randomRange(8, 16);

		XY r_xy = {r, i};
		d[r_xy] = v;
		stdDict[r_xy] = v;

		if (uniqueKeys.find(r_xy) == uniqueKeys.end())
			uniqueKeysVec.push_back(r_xy);
		uniqueKeys.insert(r_xy);
	}
	EXPECT_EQ(uniqueKeys.size(), d.size());
	EXPECT_EQ(n, d.size());

	// first pass
	for (auto kv : stdDict)
	{
		REQUIRE(d.contains(kv.first));
		EXPECT_EQ(d[kv.first], kv.second);
	}

	// remove few
	decltype(uniqueKeysVec) removed;
	removed.reserve(n);
	for (int i = 0; i < (n / 4); i++)
	{
		auto randKeyIndex = randomRange(0, (int)uniqueKeysVec.size());

		auto rkey = uniqueKeysVec[randKeyIndex];

		d.remove(rkey);
		stdDict.erase(rkey);

		removed.push_back(rkey);
		uniqueKeysVec.erase(uniqueKeysVec.begin() + randKeyIndex);
	}

	for (auto it : removed)
	{
		REQUIRE_FALSE(d.contains(it));
	}

	for (auto kv : stdDict)
	{
		REQUIRE(d.contains(kv.first));
		EXPECT_EQ(d[kv.first], kv.second);
	}
}

TEMPLATE_LIST_TEST_CASE(
	"(legacy,gtest)Dictionary - iterators test", "[container][dict]", SimpleTypes)
{
	constexpr auto initCap = util::closestPrimeSearch(17);
	Dict<int, int> d(initCap);
	std::unordered_map<int, int> umap;
	// #todo WEIRD ZERO
	for (int i = 0; i < initCap; ++i)
	{
		d[i] = i * 10;
		umap[i] = i * 10;
	}
	REQUIRE(d.size() == umap.size());

	auto ustart = umap.begin();
	auto uend = umap.end();
	for (auto st = d.begin(); st != d.end(); ++st)
	{
		auto it = umap.find((*st).key);
		if (it != uend)
			umap.erase(it);
		else
			FAIL("vex::Dict did not match element-to-element unordered_amp");
	}
} 