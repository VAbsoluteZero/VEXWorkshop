#pragma once

#include <VFramework/VEXBase.h>

#include "Platfrom.h"

namespace vp
{
	class Application;
	struct SdlDx11Impl;

	struct SdlDx11Application : public IGraphicsImpl
	{
	public:
		SdlDx11Application() = default;
		~SdlDx11Application() {   }

		i32 init(vp::Application& owner) override;

		void preFrame(vp::Application& owner) override;
		void frame(vp::Application& owner) override;
		void postFrame(vp::Application& owner) override;
		 
		void teardown(vp::Application& owner) override;

	private:
		struct SdlDx11Impl* impl = nullptr;
		bool valid = false;
	};
} // namespace vp
