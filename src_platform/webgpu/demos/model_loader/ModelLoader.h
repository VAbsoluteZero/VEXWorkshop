#pragma once

#include <VFramework/VEXBase.h>
#include <application/Platfrom.h>
#include <application/Application.h>

namespace vex
{
    struct ModelLoader : public IDemoImpl
    {
        static ModelLoader* createDemo();
        void update(Application& owner) override;
        void drawUI(Application& owner) override;
        virtual ~ModelLoader() {}
    };
}