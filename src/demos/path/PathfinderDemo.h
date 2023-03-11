#pragma once

#include <VFramework/VEXBase.h>
#include <application/Application.h>
#include <application/Platfrom.h>

namespace vp
{
    struct PathfinderDemo : public IDemoImpl
    {
        void setupImGuiForDrawPass(vp::Application& owner);
        void runloop(Application& owner) override;
        void drawUI(Application& owner) override;
        virtual ~PathfinderDemo() {}

    private:
        bool docking_not_configured = true;
    };
} // namespace vp