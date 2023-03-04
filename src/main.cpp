#include <SDL.h>
#include <application/Application.h>

#include <memory>
#include <vector>

#define TINYOBJLOADER_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#include <tinyobj/tiny_obj_loader.h>

#include "VCore/Containers/Dict.h"
#include "VCore/Memory/Memory.h"
#include "VCore/Utils/CoreTemplates.h"
#include "VFramework/Misc/RunSample.h"
#include "VFramework/VEXBase.h"
#include "application/Platfrom.h" 


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

	vp::StartupConfig config;

	config.WindowArgs.w = 1600;

	auto& app = vp::Application::init(config);
	i32 result_code = app.runLoop(); 

	spdlog::info("returning from main with code:{}", result_code);
	return result_code;
}