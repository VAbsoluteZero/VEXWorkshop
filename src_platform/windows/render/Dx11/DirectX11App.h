#pragma once

#include <VFramework/VEXBase.h>
#include <application/Platfrom.h>

struct Dx11RenderInterface;

struct ImViewport
{
	bool visible = false;
	bool paused = false;
	u32 native_vp_id = 0;
	v2i32 last_seen_size{0, 0};
};

namespace vp
{
	class Application;

	struct DirectX11App : public IGraphicsImpl
	{
	public:
        DirectX11App() { id = GfxBackendID::Dx11; }
		~DirectX11App();

		i32 init(vp::Application& owner) override;

		void preFrame(vp::Application& owner) override;
		void frame(vp::Application& owner) override;
		void postFrame(vp::Application& owner) override;

		void teardown(vp::Application& owner) override;

		void handleWindowResize(vp::Application& owner, v2u32 size) override;                      

	private:
		// this exists in order to limit header pollution with Dx/Windows specific stuff
		Dx11RenderInterface* impl = nullptr;
		std::vector<ImViewport> imgui_views;
	};

} // namespace vp