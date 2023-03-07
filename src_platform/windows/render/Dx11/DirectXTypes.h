#pragma once

#include <VFramework/VEXBase.h>
//
#include <d3d11.h>
#include <d3dcompiler.h> // shader compiler
#include <dxgi.h>		 // direct X driver interface

template <typename ComPtr>
auto releaseAndNullCOMPtr(ComPtr& ptr) -> u32
{
	if (nullptr != ptr)
	{
		auto rv = ptr->Release();
		ptr = nullptr;
		return rv;
	}
	return 0u;
}

struct Dx11Globals
{
	ID3D11Device* d3d_device = nullptr;
	ID3D11DeviceContext* im_ctx = nullptr;
	IDXGISwapChain* swap_chain = nullptr;

	auto isValid() const -> bool
	{
		return d3d_device != nullptr && im_ctx != nullptr && swap_chain != nullptr;
	}

	void release()
	{
		releaseAndNullCOMPtr(swap_chain);
		releaseAndNullCOMPtr(im_ctx);
		releaseAndNullCOMPtr(d3d_device);
	}
};

struct Dx11Window
{
	ID3D11RenderTargetView* target_view = nullptr;
	ID3D11RasterizerState* raster_state = nullptr;

	ID3D11Texture2D* stencil_tex = nullptr;
	ID3D11DepthStencilView* stencil_view = nullptr;
	ID3D11DepthStencilState* stencil_state = nullptr;

	auto isValid() const -> bool
	{
		return stencil_state != nullptr && stencil_view != nullptr && target_view != nullptr &&
			   raster_state != nullptr && stencil_tex != nullptr;
	}

	void release()
	{
		releaseAndNullCOMPtr(target_view);
		releaseAndNullCOMPtr(raster_state);

		releaseAndNullCOMPtr(stencil_tex);
		releaseAndNullCOMPtr(stencil_view);
		releaseAndNullCOMPtr(stencil_state);
	}
};

static constexpr v2i32 vp_default_sz{800, 600};
struct ViewportInitArgs
{
	v2u32 size = vp_default_sz;
	bool wireframe = false;
};

struct Dx11Viewport
{
	static inline u32 g_generation = 0;

	const u32 uid = ++g_generation;

	ViewportInitArgs createdWithArgs;
	ID3D11Texture2D* tex = nullptr;
	ID3D11RenderTargetView* view = nullptr;
	ID3D11ShaderResourceView* srv = nullptr;

	ID3D11Texture2D* stencil_tex = nullptr;
	ID3D11DepthStencilView* stencil_view = nullptr;
	ID3D11DepthStencilState* stencil_state = nullptr;

	ID3D11RasterizerState* rasterizer_state = nullptr;

	ID3D11SamplerState* sampler_state = nullptr;

	auto isValid() const -> bool
	{
		bool v = tex != nullptr && sampler_state != nullptr;
		v &= view != nullptr && srv != nullptr;
		v &= stencil_tex != nullptr && stencil_view != nullptr;
		v &= stencil_state != nullptr && rasterizer_state != nullptr;
		return v;
	}

	void initialize(ID3D11Device* d3d_device, const ViewportInitArgs& args);

	void release()
	{ 
		[[maybe_unused]] auto rc1 = releaseAndNullCOMPtr(tex);
		[[maybe_unused]] auto rc2 = releaseAndNullCOMPtr(view);
		[[maybe_unused]] auto rc3 = releaseAndNullCOMPtr(srv);

		releaseAndNullCOMPtr(stencil_tex);
		releaseAndNullCOMPtr(stencil_view);
		releaseAndNullCOMPtr(stencil_state);
		releaseAndNullCOMPtr(rasterizer_state);
		releaseAndNullCOMPtr(sampler_state);
	}
};