#pragma once
#include <VPBase.h>

namespace vp
{
	enum class ECameraSizeMode : u8
	{
		// absolute size will depend on the viewport size & specified relative size
		RelativeToViewport,
		// absolute size will be used as is
		AbsoluteSizeOverriden,
	};

	struct BasicOrthoCamera : public IComp<BasicOrthoCamera>
	{
		v2f SizeRelative = v2f_one;
		v2f SizeAbsolute = v2f_zero;
		ECameraSizeMode Mode = ECameraSizeMode::RelativeToViewport;

		static inline auto MakeWithRelativeSize(v2f relSize)
		{
			return BasicOrthoCamera{relSize, v2f_zero, ECameraSizeMode::RelativeToViewport};
		}
		
		static inline auto MakeWithAbsoluteSize(v2f absSize)
		{
			return BasicOrthoCamera {v2f_one, absSize, ECameraSizeMode::AbsoluteSizeOverriden};
		}
	};
} // namespace vp