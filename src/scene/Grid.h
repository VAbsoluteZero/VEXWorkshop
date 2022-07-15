#pragma once
#include <application/Environment.h>
#include <VCore/World/ComponentBase.h>
#include <VCore/Utils/CoreTemplates.h>
#include "VCore/Utils/VMath.h"

namespace vp
{
	struct Grid : vex::IComp<Grid>
	{
		f32 ResUnits = 1;
		i32 LineWidthPX = 2;

		inline i32 ResPixels() const { return (i32)glm::round(env::PixelPerUnit * ResUnits); }
		inline f32 LineWidthUnits() const { return env::PixelSize() * LineWidthPX; }
	};
} // namespace vp
