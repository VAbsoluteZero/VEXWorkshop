#pragma once
#include <VFramework/VEXBase.h>

#include "application/Platfrom.h"

namespace vp
{
	struct FlowFieldsDemo
	{
		void onPaint(DrawCtx& ctx);

	private:
		void init();
		bool initialized = false;
	};
} // namespace vp