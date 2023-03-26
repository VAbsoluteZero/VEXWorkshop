#pragma once

#include <VCore/Utils/CoreTemplates.h>

namespace vex::env
{
    static constexpr u32 pixel_per_unit = 128;
    constexpr float pxSize() { return 1.0f / pixel_per_unit; }

    constexpr u32 unitToPixel(float u) { return u * pixel_per_unit; }
    constexpr float pixelToUnit(u32 px) { return px / (float)pixel_per_unit; } 
} // namespace vex::env
constexpr u32 operator""_unit_to_px(long double un) { return vex::env::unitToPixel((float)un); }
constexpr float operator""_px_to_unit(u64 px) { return vex::env::pixelToUnit((u32)px); }
