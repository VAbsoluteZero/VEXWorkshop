#include <application/Application.h>

//
#include "VCore/Containers/Dict.h"
#include "VCore/Memory/Memory.h"
#include "VCore/Utils/CoreTemplates.h"
#include "VFramework/Misc/RunSample.h"
#include "VFramework/VEXBase.h"
#include "application/Platfrom.h"

#define TINYOBJLOADER_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#define WEBGPU_CPP_IMPLEMENTATION
#include <stb/stb_image.h>
#include <tinyobj/tiny_obj_loader.h>
#ifdef VEX_WGPU_DAWN
#include <dawn/webgpu.h>
#else
#include <wgpu/webgpu.h>
#endif

#undef WEBGPU_CPP_IMPLEMENTATION

using namespace vex;

int main(int argc, char** argv)
{
    vp::StartupConfig config;
    config.WindowArgs = {.w = 1600, .h = 1200};

    auto& app = vp::Application::init(config);
    i32 result_code = app.runLoop();

    spdlog::info("returning from main with code:{}", result_code);
    return result_code;
}