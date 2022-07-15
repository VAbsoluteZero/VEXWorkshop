#include <SDL.h>
#include <application/Application.h>

#include <memory>

#include "VCore/Containers/Dict.h"
#include "VCore/Utils/CoreTemplates.h"
#include "VFramework/Misc/RunSample.h"
#include "VFramework/VEXBase.h"
#include "application/Platfrom.h"
#include "demos/pathfinding/FlowFieldsDemo.h"

using namespace vex;

int main(int argc, char** argv)
{
	//#ifdef _MSC_VER
	//	IsMSVC = true;
	//#endif // DEBUG
	//
	//#ifdef __clang__
	//	IsClang = true;
	//#endif

	// SampleRunner::global().runSamples("test_defer");

	std::unordered_map<int, const char*> stdmp{{10, "add"}, {3, "tst"}};

	vex::Dict<int, const char*> dt{{10, "add"}, {3, "tst"}};
	dt[0] = "test";
	dt[1] = "test2";
	dt[2] = "test3";
	dt[4] = "test4";
	dt.remove(1);

	vp::StartupConfig config;

	auto& app = vp::Application::init(config); 
	i32 result_code = app.runLoop();

	spdlog::info("returning from main with code:{}", result_code);
	return result_code;
}