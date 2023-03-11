#pragma once

#include <VFramework/VEXBase.h>
#include <application/Platfrom.h>
#include <application/Application.h>

namespace vp
{
    struct ModelLoader : public IDemoImpl
    {
        static ModelLoader* createDemo();
        void runloop(Application& owner) override;
        void drawUI(Application& owner) override;
        virtual ~ModelLoader() {}
    };
}