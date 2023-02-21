#include <SDL.h>
#include <application/Application.h>

#include <memory>
#include <vector>

#include "VCore/Containers/Dict.h"
#include "VCore/Memory/Memory.h"
#include "VCore/Utils/CoreTemplates.h"
#include "VFramework/Misc/RunSample.h"
#include "VFramework/VEXBase.h"
#include "application/Platfrom.h"
#include "demos/pathfinding/FlowFieldsDemo.h"

using namespace vex;

int main(int argc, char** argv)
{
	// #ifdef _MSC_VER
	//	IsMSVC = true;
	// #endif // DEBUG
	//
	// #ifdef __clang__
	//	IsClang = true;
	// #endif

	// SampleRunner::global().runSamples("test_defer");

	std::unordered_map<int, const char*> stdmp{{10, "add"}, {3, "tst"}};

	vex::Dict<int, const char*> dt{{10, "add"}, {3, "tst"}};
	dt[0] = "test";
	dt[1] = "test2";
	dt[2] = "test3";
	dt[4] = "test4";
	dt.remove(1);


	vp::StartupConfig config;

	config.WindowArgs.w = 1600;

	//
	{
		InlineBufferAllocator<1024 * 100> outer;
		u32 start_size = 100;
		ExpandableBufferAllocator exp_alloc(start_size, //
			ExpandableBufferAllocator::State{
				.outer_allocator = Allocator{&outer},
				.grow_mult = 1.1f,
			});

		std::string_view dbg_str((const char*)outer.buffer, outer.bump.state.capacity);
		Allocator test_al = {&exp_alloc};

		std::vector<const char*> dbg;
		std::vector<u32> total;
		// 1a
		{
			for (i32 i = 8; i > 0; --i)
			{
				char* dbgs = vexAllocTyped<char>(test_al, 77);
				dbg.push_back(dbgs);

				memcpy(dbgs,
					"A test string data sample- 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 -1111111111-\0",
					77);
				u32* code = vexAllocTyped<u32>(test_al, 1);
				*code = 0xABCD'1000 + (8 - i);
			}

			total.push_back(outer.bump.state.top);
			total.push_back(exp_alloc.state.total_reserved);
			 
			exp_alloc.release();
			dbg.clear();
		}
		// 1b
		{
			for (i32 i = 8; i > 0; --i)
			{
				char* dbgs = vexAllocTyped<char>(test_al, 77);
				dbg.push_back(dbgs);

				memcpy(dbgs,
					"2 test string data sample- 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 -1111111112-\0",
					77);
				u32* code = vexAllocTyped<u32>(test_al, 1);
				*code = 0xABCD'1000 + (8 - i);
			}

			total.push_back(outer.bump.state.top);
			total.push_back(exp_alloc.state.total_reserved);

			exp_alloc.release();
			dbg.clear();
		}
		// 2
		{
			for (i32 i = 8; i > 0; --i)
			{
				char* dbgs = vexAllocTyped<char>(test_al, 78);
				dbg.push_back(dbgs);

				memcpy(dbgs,
					"3 test string data sample. 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 .1111111113. "
					"\0",
					78);
				u32* code = vexAllocTyped<u32>(test_al, 1);
				*code = 0xABCD'1000 + (8 - i);
			}

			total.push_back(outer.bump.state.top);
			total.push_back(exp_alloc.state.total_reserved);
			 
			exp_alloc.releaseAndReserveUsedSize();
			dbg.clear();
		}
		// 3
		{
			for (i32 i = 8; i > 0; --i)
			{
				char* dbgs = vexAllocTyped<char>(test_al, 78);
				dbg.push_back(dbgs);

				memcpy(dbgs,
					"3 test string data sample. 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 .1111111113. "
					"\0",
					78);
				u32* code = vexAllocTyped<u32>(test_al, 1);
				*code = 0xABCD'1000 + (8 - i);
			}

			total.push_back(outer.bump.state.top);
			total.push_back(exp_alloc.state.total_reserved);

			exp_alloc.releaseAndReserveUsedSize();
			dbg.clear();
		}
	}

	auto& app = vp::Application::init(config);
	i32 result_code = app.runLoop(); 

	spdlog::info("returning from main with code:{}", result_code);
	return result_code;
}