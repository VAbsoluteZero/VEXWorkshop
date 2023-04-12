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
#include <spdlog/stopwatch.h>
#include <stb/stb_image.h>
#include <stb/stb_image_write.h>
#include <tinyobj/tiny_obj_loader.h>

//
#include <webgpu/demos/basic_textured_quad/TexturedQuad.h>
#include <webgpu/demos/model_loader/ModelLoader.h>
#include <webgpu/demos/path/PathfinderDemo.h>

using namespace vex;

// #fixme - possibly register samples similarly to the way test frameworks register tests
vex::DemoSamples registerAvailableDemoCtors(); 

int main(int argc, char** argv)
{
    vex::StartupConfig config;
    // experimental feature, originally demo would be selected only once
    config.allow_demo_changes =
#ifndef VEX_EMSCRIPTEN
        true;
#else
        false;
#endif
    config.target_framerate = 300; 

    spdlog::stopwatch sw;
    auto& app = vex::Application::init(config, registerAvailableDemoCtors());

    SPDLOG_INFO(" ---------------- application initialization finished ---------------- ");
    app.activateDemo("pf_main");
    SPDLOG_WARN("Initializing application took {} seconds", sw);

    i32 result_code = app.runLoop();

    spdlog::info("returning from main with code:{}", result_code);
    return result_code;
}

vex::DemoSamples registerAvailableDemoCtors()
{
    vex::DemoSamples out;
#ifdef VEX_GFX_WEBGPU
    out.add("quad", "Basic textured quads", "Basic WGPU API usage",
        [](vex::Application& owner) -> vex::IDemoImpl*
        {
            spdlog::stopwatch sw;
            defer_ { SPDLOG_WARN("Initializing demo took {} seconds", sw); };

            auto new_demo = new vex::quad_demo::TexturedQuadDemo();
            new_demo->init(owner, {});
            return new_demo;
        });
    out.add("pf_main", "Pathfinding demo", "Flow fields pathfinding",
        [](vex::Application& owner) -> vex::IDemoImpl*
        {
            spdlog::stopwatch sw;
            defer_ { SPDLOG_WARN("Initializing demo took {} seconds", sw); }; 
            SPDLOG_INFO(" ctor ");
            auto new_demo = new vex::flow::FlowfieldPF();
            SPDLOG_INFO(" before init ");
            new_demo->init(owner, {});
            return new_demo;
        }); 
#endif

    return out;
}