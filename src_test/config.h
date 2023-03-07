#include <snitch/snitch.hpp>
////
#include <VCore/Utils/CoreTemplates.h>
#include <VCore/Utils/TTraits.h>
#include <VCore/Utils/VMath.h>
#include <VCore/Utils/VUtilsBase.h>
#include <VFramework/Misc/VUtils.h>
//
#include <VCore/Containers/Array.h>
//
#include <memory>
#include <string>
#include <vector>

#define EXPECT_EQ(x, y) REQUIRE(x == y);
#define EXPECT_NE(x, y) REQUIRE(x != y);

// _al postfix means matcher allocates on heap
namespace snitch::matchers
{
	template <typename T>
	struct ordered_equals_al
	{
		std::vector<T> data;

		template <typename Container>
		bool match(const Container& arr) const noexcept
		{
			if (arr.size() != data.size())
				return false;
			for (i32 i = 0; i < data.size(); ++i)
				if (data[i] != arr[i])
					return false;
			return true;
		}

		template <typename Container>
		small_string<max_message_length> describe_match(
			const Container& arr, match_status status) const noexcept
		{
			small_string<max_message_length> message;
			append_or_truncate(message,
				status == match_status::matched ? "ranges are equal" : "ranges are not equal");

			if (status == match_status::failed)
			{
				if (arr.size() != data.size())
				{
					append_or_truncate(message, " size mismatch ");
					return message;
				}
				for (i32 i = 0; i < data.size(); ++i)
					if (data[i] != arr[i])
						append_or_truncate(
							message, " at index [", i, "] expected: ", data[i], ", got: ", arr[i]);
			}

			return message;
		}
	};

	template <typename T>
	struct unordered_equals_al
	{
		std::vector<T> data;
		std::vector<T> not_found;

		bool findAndRemove(const T& v)
		{
			for (i32 i = 0; i < data.size(); ++i)
			{
				if (data[i] == v)
				{
					data.erase(data.begin() + i);
					return true;
				}
			}
			return false;
		}

		template <typename Container>
		bool match(const Container& arr) 
		{
			if (arr.size() != data.size())
			{
				return false;
			}
			bool success = true;
			for (i32 i = 0; i < arr.size(); ++i)
				if (!findAndRemove(arr[i]))
				{
					success = false;
					not_found.push_back(arr[i]);
				}
			return success;
		}

		template <typename Container>
		small_string<max_message_length> describe_match(
			const Container& arr, match_status status) const noexcept
		{
			small_string<max_message_length> message;
			append_or_truncate(message, status == match_status::matched
											? "sets of elements are equal"
											: "sets of elements are not equal");

			if (status == match_status::failed)
			{
				if (arr.size() != data.size())
				{
					append_or_truncate(message, " size mismatch ");
					return message;
				}
				for (i32 i = 0; i < data.size(); ++i)
					append_or_truncate(message, " Expected element not found: ", data[i]);
				for (i32 i = 0; i < not_found.size(); ++i)
					append_or_truncate(
						message, " element was not expected but provided: ", not_found[i]);
			}

			return message;
		}
	};

	// template <typename TKey, typename TVal>
	// struct set_equals
	//{

	//};
} // namespace snitch::matchers