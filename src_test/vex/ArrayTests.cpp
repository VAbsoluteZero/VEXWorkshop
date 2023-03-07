#include <VCore/Containers/Ring.h>
#include <VCore/Containers/Stack.h>

#include <vector>

#include "../config.h"
//
using namespace vex;

// =======================================================================================
// Buffer
TEST_CASE("Buffer must support reserve", "[container][buffer]")
{
	Buffer<i32> buf;

	REQUIRE(buf.size() == 0);
	REQUIRE(buf.capacity() == 0);
	buf.reserve(12);
	REQUIRE(buf.capacity() == 12);
}
TEST_CASE("Buffer must support initializer_list init", "[container][buffer]")
{
	Buffer<i32> buf{1, 2, 3, 4, 5};
	std::vector<i32> vec{1, 2, 3, 4, 5};

	SECTION("size must be equal to initializer_list size") { REQUIRE(buf.size() == vec.size()); }

	SECTION("elements must be in order")
	{
		REQUIRE_THAT(buf, snitch::matchers::ordered_equals_al<i32>{vec});
	}
}
TEST_CASE("Buffer must support adding elements", "[container][buffer]")
{
	Buffer<i32> buf;

	REQUIRE(buf.size() == 0);

	SECTION("must have size of 1 after adding element")
	{
		buf.add(42);
		REQUIRE(buf.size() == 1);
		REQUIRE(buf[0] == 42);
	}
	SECTION("must have size of 3 after adding 3 elements")
	{
		buf.add(1000);
		buf.add(1000);
		buf.add(1000);
		REQUIRE(buf.size() == 3);
	}
	SECTION("must have capacity >= size after adding elements")
	{
		buf.add(1000);
		buf.add(1000);
		buf.add(1000);
		REQUIRE(buf.size() < buf.capacity());
	}
	SECTION("must support adding range of elements")
	{
		i32 arr[3] = {1, 2, 3};
		buf.addRange(arr, 3);
		REQUIRE(buf.size() == 3);
		REQUIRE_THAT(buf, snitch::matchers::ordered_equals_al<i32>{{1, 2, 3}});
	}
}
TEST_CASE("Buffer must support removal of elements", "[container][buffer]")
{
	Buffer<i32> buf{1, 2, 3, 4, 5};
	REQUIRE(buf.size() == 5);
	REQUIRE(buf.capacity() >= 5);

	SECTION(" single element removal swap-removal")
	{
		buf.removeSwapAt(2);
		REQUIRE(buf.size() == 4);
		REQUIRE(buf.capacity() >= 5);
		REQUIRE_THAT(buf, snitch::matchers::unordered_equals_al<i32>{{1, 2, 5, 4}});
	}
	SECTION(" range-shift removal")
	{
		buf.removeShift(1, 2);
		REQUIRE(buf.size() == 3);
		REQUIRE(buf.capacity() >= 5);
		REQUIRE_THAT(buf, snitch::matchers::ordered_equals_al<i32>{{1, 4, 5}});
	}
	SECTION(" range-shift removal (edge case, at end)")
	{
		buf.removeShift(4, 1);
		REQUIRE(buf.size() == 4);
		REQUIRE(buf.capacity() >= 5);
		REQUIRE_THAT(buf, snitch::matchers::ordered_equals_al<i32>{{1, 2, 3, 4}});
	}
	SECTION(" range-shift removal (edge case, past end)")
	{
		buf.removeShift(4, 5);
		REQUIRE(buf.size() == 4);
		REQUIRE(buf.capacity() >= 5);
		REQUIRE_THAT(buf, snitch::matchers::ordered_equals_al<i32>{{1, 2, 3, 4}});
	}
	SECTION(" range-shift removal (edge case, full array)")
	{
		buf.removeShift(0, 5);
		REQUIRE(buf.size() == 0);
		REQUIRE(buf.capacity() >= 5);
	}
}

// =======================================================================================
// ring
using RignTypes = snitch::type_list<vex::StaticRing<i32, 4, true>, vex::StaticRing<i32, 4, false>>;

TEMPLATE_LIST_TEST_CASE("Ring basic operations tests", "[container][ring]", RignTypes)
{
	TestType ring_default;
	TestType ring{1, 2, 3, 4};

	SECTION("should not contain any elements")
	{
		EXPECT_EQ(ring_default.capacity(), 4);
		EXPECT_EQ(ring_default.size(), 0)
		EXPECT_EQ(ring_default.peek(), nullptr);
	}


	SECTION("should properly add element")
	{
		ring_default.push(1);
		EXPECT_EQ(ring_default.size(), 1);
		auto el = ring_default.peekUnchecked();
		EXPECT_EQ(el, 1);
	}

	SECTION("should properly add elements past capacity")
	{
		ring_default.push(1);
		ring_default.push(2);
		ring_default.push(3);

		EXPECT_EQ(ring_default.size(), 3);
		EXPECT_EQ(ring_default.peekUnchecked(), 3);
		EXPECT_EQ(ring_default.peekUnchecked(), *(ring_default.peek()));

		ring_default.push(4);
		EXPECT_EQ(ring_default.size(), 4);
		EXPECT_EQ(ring_default.peekUnchecked(), 4);

		ring_default.push(5);
		ring_default.push(6);
		EXPECT_EQ(ring_default.size(), 4);
		EXPECT_EQ(ring_default.peekUnchecked(), 6);
	}
}

TEMPLATE_LIST_TEST_CASE("Ring initializer list ctor test", "[container][ring]", RignTypes)
{
	TestType ring{1, 2, 3, 4};
	using VT = TestType::ValueType;
	// ring, as stack, is LIFO, so test range is reversed
	std::vector<VT> vec_expected{4, 3, 2, 1};
	std::vector<VT> vec_raw{1, 2, 3, 4};

	EXPECT_EQ(ring.capacity(), 4);
	EXPECT_EQ(ring.size(), 4);
	EXPECT_EQ(ring.peekUnchecked(), 4);

	SECTION("'at' function should use LIFO indexing")
	{
		for (int i = 0; i < 4; ++i)
		{
			auto a = ring.at(i);
			auto b = vec_expected.at(i);
			EXPECT_EQ(a, b);
		}
	}
	// =================================================================
	SECTION("range for should use LIFO undexing")
	{
		auto ri = ring.begin();
		auto vi = vec_expected.begin();
		for (;											   //
			 ri != ring.end() && vi != vec_expected.end(); //
			 ++ri, ++vi)
		{
			auto v1 = *ri;
			auto v2 = *vi;
			EXPECT_EQ(v1, v2);
		}
	}

	SECTION("raw memory layout should correspond ring mode (template arg)")
	{
		if constexpr (ring.k_fill_forward)
		{
			for (int i = 0; i < 4; ++i)
			{
				EXPECT_EQ(*(ring.rawDataUnsafe() + i), vec_raw.at(i));
			}
		}
		else
		{
			// fill_forward==FALSE means elements were added starting from the end of ring
			// buffer and going backwards. vec_expected, which has reversed order of elements
			// can be used to check raw layout
			for (int i = 0; i < 4; ++i)
			{
				EXPECT_EQ(*(ring.rawDataUnsafe() + i), vec_expected.at(i));
			}
		}
	}
}

TEMPLATE_LIST_TEST_CASE("Ring copy ctor and reverse iterator test", "[container][ring]", RignTypes)
{
	using VT = TestType::ValueType;
	TestType ring_orig{1, 2, 3};
	TestType ring_other{11, 22, 33, 44};
	TestType ring = ring_orig;

	EXPECT_EQ(ring.capacity(), 4);
	EXPECT_EQ(ring.size(), 3);

	// ring, as stack, is LIFO, so test range is reversed
	std::vector<VT> vec_expected{3, 2, 1};
	std::vector<VT> vec_expected_other{44, 33, 22, 11};

	// =================================================================
	SECTION("reverse iterator should use FIFO indexing")
	{
		auto ri = ring.rbegin();
		auto vi = vec_expected.rbegin();
		for (;												 //
			 ri != ring.rend() && vi != vec_expected.rend(); //
			 ++ri, ++vi)
		{
			auto v1 = *ri;
			auto v2 = *vi;
			EXPECT_EQ(v1, v2);
		}
	}

	SECTION(" copy ctor test")
	{
		ring = ring_other; // testing operator=
		EXPECT_EQ(ring_other.size(), 4);
		EXPECT_EQ(ring.size(), 4);

		for (int i = 0; i < ring.size(); ++i)
		{
			EXPECT_EQ(ring.at(i), vec_expected_other.at(i));
		}
	}
}

TEMPLATE_LIST_TEST_CASE("Ring Put/Pop/Clear test", "[container][ring]", RignTypes)
{
	TestType ring_orig{1, 2, 3};
	using VT = TestType::ValueType;
	using namespace vex;
	// test single push (& destroy)
	{
		TestType ring;
		EXPECT_EQ(ring.capacity(), 4);

		ring.push(1);
		ring.popDiscard();

		EXPECT_EQ(ring.size(), 0);		 //<< "size should be zero";
		EXPECT_EQ(ring.peek(), nullptr); //<< "should not contain any elements";

		ring.push(1);
		ring.push(2);
		auto popped_el = ring.pop();

		EXPECT_EQ(ring.size(), 1); //<< "size should be set to 1 after 2x pushes 1x pop";
		auto el = ring.peekUnchecked();
		EXPECT_EQ(el, 1);				   //<< "should contain proper element";
		EXPECT_EQ(popped_el.get<VT>(), 2); //<< "should pop proper element";

		popped_el = ring.pop();
		EXPECT_EQ(ring.size(), 0); //<< "size should be zero";

		popped_el = ring.pop();
		EXPECT_EQ(ring.size(), 0);		 //<< "size should be zero";
		REQUIRE(popped_el.has<Error>()); //<< "should return error if 0 elements";

		ring.push(1);
		ring.clear();
		EXPECT_EQ(ring.size(), 0); //<< "size should be zero";

		for (int i = 0; i <= 8; ++i) // 8, 7, 2, 1
		{
			if (i == 6 || i == 5)
			{
				[[maybe_unused]] auto disc = ring.pop(); // pops 4, 3
			}
			else
			{
				ring.push(i);
			}
		}

		std::vector<VT> vec_expected{8, 7, 2, 1};
		for (int i = 0; i < ring.size(); ++i)
		{
			EXPECT_EQ(ring.at(i), vec_expected.at(i));
		}
	}
}
// Stack
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

	AllocWrapper wrapper;
	auto cur_alloc = Allocator{&wrapper.al};

	// scope
	{
		vex::Stack<ValueType> stack(cur_alloc);

		stack.push(1);
		REQUIRE(stack.capacity() > 0);	  //<< "capacity should be set after first push";
		REQUIRE(stack.peek() != nullptr); //<< "should contain element";
		auto el = stack.peekUnchecked();
		EXPECT_EQ(el, 1); //<< "should contain proper element";

		stack.push(2);
		stack.push(3);

		EXPECT_EQ(stack.size(), 3); //<< "should contain proper number of elements after pushes";
		EXPECT_EQ(stack.peekUnchecked(), 3); //<< "should contain last added element";

		stack.pop();
		EXPECT_EQ(stack.size(), 2); //<< "should contain proper number of elements after pop";
		EXPECT_EQ(stack.peekUnchecked(), 2); //<< "should contain previously added element";

		stack.push(4);
		stack.push(5);
		EXPECT_EQ(stack.peekUnchecked(), 5); //<< "should contain last added element";
		EXPECT_EQ(stack.size(), 4); //<< "should contain proper number of elements after pushes";
	}
}

TEMPLATE_LIST_TEST_CASE(
	"Stack growth test (with varying allocators)", "[container][stack]", StackTestTypesWithAlloc)
{
	using namespace vex;

	using AllocWrapper = typename TestType::First;
	using ValueType = typename TestType::Second;

	AllocWrapper wrapper;
	auto cur_alloc = Allocator{&wrapper.al};

	// scope
	{
		vex::Stack<ValueType> stack(cur_alloc);
		std::vector<ValueType> reverse_vals;
		for (int i = 0; i < 32; ++i)
		{
			stack.push((ValueType)i);
			reverse_vals.push_back((ValueType)i);
		}
		std::reverse(reverse_vals.begin(), reverse_vals.end());

		EXPECT_EQ(stack.peekUnchecked(), 31); //<< "should contain last added element";
		EXPECT_EQ(stack.size(), 32); //<< "should contain proper number of elements after pushes"; 

		int num_popped = 0;

		for (; stack.size() > 0; num_popped++)
		{
			Union<ValueType, vex::Error> opt_elem = stack.pop();
			if (!opt_elem.has<Error>())
			{
				auto el = opt_elem.get<ValueType>();
				EXPECT_EQ(el, reverse_vals[num_popped]);
			}
			else
			{
				FAIL("expected element but found none");
			}
		}
		auto err_elem = stack.pop();
		REQUIRE(err_elem.has<Error>());
	}
}