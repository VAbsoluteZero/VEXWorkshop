#include "ModelLoader.h"
#include <imgui.h>

vp::ModelLoader* vp::ModelLoader::createDemo()
{ //
    return new ModelLoader();
}

void vp::ModelLoader::runloop(Application& owner)
{ //
}

void vp::ModelLoader::drawUI(Application& owner)
{ //
    bool visible = ImGui::Begin("ModelLoader");
    ImGui::Text("testing testing");
    defer_ { ImGui::End(); };
}
