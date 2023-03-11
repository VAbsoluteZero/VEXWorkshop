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
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define WEBGPU_CPP_IMPLEMENTATION
#include <stb/stb_image.h>
#include <stb/stb_image_write.h>
#include <tinyobj/tiny_obj_loader.h>

//
#include <demos/model_loader/ModelLoader.h>
#include <demos/path/PathfinderDemo.h>

using namespace vex;

// #fixme - possibly register samples similarly to the way test frameworks register tests
vp::DemoSamples registerAvailableDemoCtors();

int main(int argc, char** argv)
{
    vp::StartupConfig config;
    config.WindowArgs = {.w = 1600, .h = 1200};

    auto& app = vp::Application::init(config, registerAvailableDemoCtors());
    // for now force pf demo
    app.activateDemo("pf_main");
    i32 result_code = app.runLoop();

    spdlog::info("returning from main with code:{}", result_code);
    return result_code;
}

vp::DemoSamples registerAvailableDemoCtors()
{
    vp::DemoSamples out;
    out.add("pf_main", "Particle 'pathfinding' demo", "Flow fields pathfinding",
        [](vp::Application& owner) -> vp::IDemoImpl*
        {
            return new vp::PathfinderDemo();
        });
    out.add("model_loader", "Model loader", "Load and render simple model",
        [](vp::Application& owner) -> vp::IDemoImpl*
        {
            return new vp::ModelLoader();
        });

    return out;
}