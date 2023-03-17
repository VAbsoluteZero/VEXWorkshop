#include "ModelLoader.h"
#include <imgui.h>

vex::ModelLoader* vex::ModelLoader::createDemo()
{ //
    return new ModelLoader();
}

void vex::ModelLoader::update(Application& owner)
{ //
}

void vex::ModelLoader::drawUI(Application& owner)
{ //
    bool visible = ImGui::Begin("ModelLoader");
    ImGui::Text("testing testing");
    defer_ { ImGui::End(); };
}
