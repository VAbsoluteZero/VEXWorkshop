#pragma once

namespace env
{
	static constexpr int PixelPerUnit = 64;
	constexpr float PixelSize() { return 1.0f / PixelPerUnit; }
	 
} // namespace env
