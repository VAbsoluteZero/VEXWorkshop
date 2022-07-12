#include <algorithm>
#include <functional>
//

#include <SDL.h>
#include <SDL_syswm.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_sdl.h>
#include <d3d11.h>
#include <d3dcompiler.h> // shader compiler
#include <dxgi.h>		 // direct X driver interface
#include <imgui.h>
#include <utils/ImGuiUtils.h>

#include "Application.h"
#include "SDLDx11Application.h"

namespace vp
{
	struct SdlDx11Impl
	{
		SDL_SysWMinfo sdl_wm_info;
		HWND window_handle;

		ID3D11Device* d3d_device = nullptr;
		ID3D11DeviceContext* d3d_device_ctx = nullptr;
		IDXGISwapChain* swap_chain = nullptr;
		ID3D11RenderTargetView* main_target_view = nullptr;

		ID3DBlob* vs_blob_ptr = nullptr;
		ID3DBlob* ps_blob_ptr = nullptr;
		ID3DBlob* error_blob = nullptr;
		ID3D11InputLayout* input_layout_ptr = nullptr;

		ID3D11VertexShader* vertex_shader_ptr = nullptr;
		ID3D11PixelShader* pixel_shader_ptr = nullptr;

		ID3D11Texture2D* viewport_tex2d = nullptr;
		ID3D11RenderTargetView* viewport_target_view = nullptr;
		ID3D11ShaderResourceView* shaderResourceViewMap = nullptr;
		ID3D11DepthStencilState* depth_state_no_depth = nullptr;

		void initHWND(SDL_Window* raw_window)
		{
			SDL_VERSION(&sdl_wm_info.version);
			SDL_GetWindowWMInfo(raw_window, &sdl_wm_info);
			window_handle = (HWND)sdl_wm_info.info.win.window;
		}
		bool createDeviceD3D()
		{
			// Setup swap chain
			DXGI_SWAP_CHAIN_DESC sd;
			ZeroMemory(&sd, sizeof(sd));
			sd.BufferCount = 2;
			sd.BufferDesc.Width = 0;
			sd.BufferDesc.Height = 0;
			sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			sd.BufferDesc.RefreshRate.Numerator = 60;
			sd.BufferDesc.RefreshRate.Denominator = 1;
			sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
			sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			sd.OutputWindow = window_handle;
			sd.SampleDesc.Count = 1;
			sd.SampleDesc.Quality = 0;
			sd.Windowed = TRUE;
			sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

			UINT createDeviceFlags = 0;

#if defined(DEBUG) || defined(_DEBUG)
			createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

			D3D_FEATURE_LEVEL featureLevel;
			const D3D_FEATURE_LEVEL featureLevelArray[2] = {
				D3D_FEATURE_LEVEL_11_0,
				D3D_FEATURE_LEVEL_10_0,
			};

			if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
					createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &swap_chain,
					&d3d_device, &featureLevel, &d3d_device_ctx) != S_OK)
			{
				return false;
			}
			createRenderTarget();
			return true;
		}

		ID3D11RasterizerState* pRasterizerState;
		bool createRenderTarget()
		{
			if (nullptr == d3d_device || nullptr == swap_chain)
				return false;

			ID3D11Texture2D* back_buffer = nullptr;
			swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
			d3d_device->CreateRenderTargetView(back_buffer, nullptr, &main_target_view);
			back_buffer->Release(); // WHY RELEASE ? #todo findout
			 
			// Create depth-stencil State for NO DEPTH
			{
				D3D11_DEPTH_STENCIL_DESC desc;
				ZeroMemory(&desc, sizeof(desc));
				desc.DepthEnable = false;
				desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
				desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
				desc.StencilEnable = false;
				desc.FrontFace.StencilFailOp = desc.FrontFace.StencilDepthFailOp =
					desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
				desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
				desc.BackFace = desc.FrontFace;
				d3d_device->CreateDepthStencilState(&desc, &depth_state_no_depth);
			}
			// Create the rasterizer state
			{
				D3D11_RASTERIZER_DESC desc;
				ZeroMemory(&desc, sizeof(desc));
				desc.FillMode = D3D11_FILL_SOLID;
				desc.CullMode = D3D11_CULL_NONE;
				desc.ScissorEnable = true;
				desc.DepthClipEnable = true;
				d3d_device->CreateRasterizerState(&desc, &pRasterizerState);
			}

			return true;
		}

		void initDebugShaders()
		{
			UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(DEBUG) || defined(_DEBUG)
			flags |= D3DCOMPILE_DEBUG; // add more debug output
#endif

			// COMPILE VERTEX SHADER
			HRESULT hr = D3DCompileFromFile(L"content/shaders/debug/plain_dbg.hlsl", nullptr,
				D3D_COMPILE_STANDARD_FILE_INCLUDE, "vs_main", "vs_5_0", flags, 0, &vs_blob_ptr,
				&error_blob);
			if (FAILED(hr))
			{
				if (error_blob)
				{
					OutputDebugStringA((char*)error_blob->GetBufferPointer());
					error_blob->Release();
				}
				if (vs_blob_ptr)
				{
					vs_blob_ptr->Release();
				}
				VEX_DBGBREAK();
			}

			// COMPILE PIXEL SHADER
			hr = D3DCompileFromFile(L"content/shaders/debug/plain_dbg.hlsl", nullptr,
				D3D_COMPILE_STANDARD_FILE_INCLUDE, "ps_main", "ps_5_0", flags, 0, &ps_blob_ptr,
				&error_blob);
			if (FAILED(hr))
			{
				if (error_blob)
				{
					OutputDebugStringA((char*)error_blob->GetBufferPointer());
					error_blob->Release();
				}
				if (ps_blob_ptr)
				{
					ps_blob_ptr->Release();
				}
				VEX_DBGBREAK();
			}

			hr = d3d_device->CreateVertexShader(vs_blob_ptr->GetBufferPointer(),
				vs_blob_ptr->GetBufferSize(), NULL, &vertex_shader_ptr);
			check_(SUCCEEDED(hr));

			hr = d3d_device->CreatePixelShader(ps_blob_ptr->GetBufferPointer(),
				ps_blob_ptr->GetBufferSize(), NULL, &pixel_shader_ptr);
			check_(SUCCEEDED(hr));

			D3D11_INPUT_ELEMENT_DESC input_desc[] = {
				{"POS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0}};

			hr = d3d_device->CreateInputLayout(input_desc, ARRAYSIZE(input_desc),
				vs_blob_ptr->GetBufferPointer(), vs_blob_ptr->GetBufferSize(), &input_layout_ptr);

			check(SUCCEEDED(hr), "could not create input layout for shader");

			////////////////////////////////////////////////////////////
			// test viewport stuff
			D3D11_TEXTURE2D_DESC viewport_tex_desc;
			D3D11_RENDER_TARGET_VIEW_DESC viewport_tv_desc;
			D3D11_SHADER_RESOURCE_VIEW_DESC viewport_shader_res_desc;

			///////////////////////// Map's Texture
			// Initialize the  texture description.
			ZeroMemory(&viewport_tex_desc, sizeof(viewport_tex_desc));
			ZeroMemory(&viewport_tv_desc, sizeof(viewport_tv_desc));
			ZeroMemory(&viewport_shader_res_desc, sizeof(viewport_shader_res_desc));

			viewport_tex_desc.Width = 3800;
			viewport_tex_desc.Height = 3800;
			viewport_tex_desc.MipLevels = 1;
			viewport_tex_desc.ArraySize = 1;
			viewport_tex_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			viewport_tex_desc.SampleDesc.Count = 1;
			viewport_tex_desc.Usage = D3D11_USAGE_DEFAULT;
			viewport_tex_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			viewport_tex_desc.CPUAccessFlags = 0;
			viewport_tex_desc.MiscFlags = 0;

			// Create the texture
			d3d_device->CreateTexture2D(&viewport_tex_desc, NULL, &viewport_tex2d);

			/////////////////////// Map's Render Target
			// Setup the description of the render target view.
			viewport_tv_desc.Format = viewport_tex_desc.Format;
			viewport_tv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			viewport_tv_desc.Texture2D.MipSlice = 0;

			// Create the render target view.
			d3d_device->CreateRenderTargetView(
				viewport_tex2d, &viewport_tv_desc, &viewport_target_view);

			/////////////////////// Map's Shader Resource View
			// Setup the description of the shader resource view.
			viewport_shader_res_desc.Format = viewport_tex_desc.Format;
			viewport_shader_res_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			viewport_shader_res_desc.Texture2D.MostDetailedMip = 0;
			viewport_shader_res_desc.Texture2D.MipLevels = 1;

			// Create the shader resource view.
			d3d_device->CreateShaderResourceView(
				viewport_tex2d, &viewport_shader_res_desc, &shaderResourceViewMap);
		}

		void cleanupDeviceD3D()
		{
			cleanupRenderTarget();
			if (swap_chain)
			{
				swap_chain->Release();
				swap_chain = nullptr;
			}
			if (d3d_device_ctx)
			{
				d3d_device_ctx->Release();
				d3d_device_ctx = nullptr;
			}
			if (d3d_device)
			{
				d3d_device->Release();
				d3d_device = nullptr;
			}
		}

		void cleanupRenderTarget()
		{
			if (main_target_view)
			{
				main_target_view->Release();
				main_target_view = nullptr;
			}
		}
	};
} // namespace vp

// Data
static vp::SdlDx11Impl sdl_impl{};

constexpr auto g_view_name = "Viewport";


ID3D11Buffer* vertex_buffer_ptr = nullptr;
static float vertex_data_array[] = {
	0.0f, 0.5f, .0f,   // point at top
	0.5f, -0.5f, .0f,  // point at bottom-right
	-0.5f, -0.5f, .0f, // point at bottom-left
};

i32 vp::SdlDx11Application::init(vp::Application& owner)
{
	auto main_window = owner.getMainWindow();
	SDL_Window* window = main_window->getRawWindow();

	this->impl = &sdl_impl;

	// Initialize Direct3D
	impl->initHWND(window);
	if (!impl->createDeviceD3D())
	{
		impl->cleanupDeviceD3D();
		checkAlwaysRel(false, "could not create D3D device");
		return 2;
	}

	// debug shaders
	{
		impl->initDebugShaders();
	}

	// init imgui
	{
		auto gImguiContext = ImGui::CreateContext();
		ImGui_ImplDX11_Init(impl->d3d_device, impl->d3d_device_ctx);
		ImGui_ImplSDL2_InitForSDLRenderer(window);

		auto tst = SDL_GetBasePath();
		spdlog::info("path : {}", tst);
	}


	valid = true;

	vp::g_view_hub.views.emplace(g_view_name, vp::ImView{g_view_name});


	if (!vertex_buffer_ptr)
	{
		D3D11_BUFFER_DESC vertex_buff_descr = {};
		vertex_buff_descr.ByteWidth = sizeof(vertex_data_array);
		vertex_buff_descr.Usage = D3D11_USAGE_DEFAULT;
		vertex_buff_descr.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		D3D11_SUBRESOURCE_DATA sr_data = {0};
		sr_data.pSysMem = vertex_data_array;

		HRESULT hr =
			impl->d3d_device->CreateBuffer(&vertex_buff_descr, &sr_data, &vertex_buffer_ptr);

		check_(SUCCEEDED(hr));
	}

	return 0;
}

// void DrawViewportWindow(
//	const char* name, Vector2& viewportSize, Vector2& viewportPosition, bool* isOpen)
//{
//	ImGui::Begin(name, isOpen, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
//	auto viewport = Rendering::GetViewport();
//
//	if (viewport.IsValid() && viewport->GetRenderTexture().IsValid())
//	{
//		Vector2 newWindowSize = ImGui::GetWindowSize();
//		if (newWindowSize != viewportSize) // notify application that viewport size has been changed
//		{
//			Event::AddEvent(MakeUnique<WindowResizeEvent>(viewportSize, newWindowSize));
//			viewportSize = newWindowSize;
//		}
//		viewportPosition = (newWindowSize - viewportSize) * 0.5f;
//		ImGui::SetCursorPos(viewportPosition);
//		ImGui::Image((void*)(uintptr_t)viewport->GetRenderTexture()->GetNativeHandle(),
//			viewportSize, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
//	}
//	ImGui::End();
// }

void vp::SdlDx11Application::preFrame(vp::Application& owner)
{
	if (!valid)
		return;
	{
		ImGui_ImplSDL2_NewFrame(); // window & inputs related frame init specific for SDL
		ImGui::NewFrame();		   // cross-platform logic for a  new frame imgui init
	}

	// Setup viewport
	auto main_window = owner.getMainWindow();
	D3D11_VIEWPORT vp;
	memset(&vp, 0, sizeof(D3D11_VIEWPORT));
	vp.Width = main_window->windowSize().x;
	vp.Height = main_window->windowSize().y;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = vp.TopLeftY = 0;
	impl->d3d_device_ctx->RSSetViewports(1, &vp);

	// #fixme : temporary snippet, before proper viewport is done
	vp::ImView* view = vp::g_view_hub.views.tryGet(g_view_name);
	if (view)
	{
		view->delayed_gui_drawcalls.push_back([&](ArgsImViewUpd args) { //
			bool visible = ImGui::Begin("Viewport");
			defer_ { ImGui::End(); };

			auto sz_wnd = ImGui::GetWindowSize();

			if (visible) { //
				//ImGui::SetCursorPos(viewportPosition);
				ImGui::Image((void*)(uintptr_t)impl->shaderResourceViewMap, sz_wnd,
					ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
			
			}
		});
	}
}
void vp::SdlDx11Application::frame(vp::Application& owner)
{
	if (!valid)
		return;

	// prepare frame
	if (vertex_buffer_ptr)
	{
		static u32 ctst = 0;
		ctst += 1000000;
		ImVec4 clear_color = ImVec4(0.07f, ((float)ctst) / (float)UINT32_MAX, 0.12f, 1.00f);
		const float clear_color_with_alpha[4] = {clear_color.x * clear_color.w,
			clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w};
		impl->d3d_device_ctx->OMSetRenderTargets(1, &impl->viewport_target_view, nullptr);
		impl->d3d_device_ctx->ClearRenderTargetView( impl->viewport_target_view, clear_color_with_alpha); 

		//impl->d3d_device_ctx->OMSetDepthStencilState(impl->depth_state_no_depth, 0); 
		//impl->d3d_device_ctx->RSSetState(impl->pRasterizerState);

		UINT vertex_stride = 3 * sizeof(float);
		UINT vertex_offset = 0;
		UINT vertex_count = 3; 

		impl->d3d_device_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		impl->d3d_device_ctx->IASetInputLayout(impl->input_layout_ptr);
		impl->d3d_device_ctx->IASetVertexBuffers(
			0, 1, &vertex_buffer_ptr, &vertex_stride, &vertex_offset);

		impl->d3d_device_ctx->VSSetShader(impl->vertex_shader_ptr, NULL, 0);
		impl->d3d_device_ctx->PSSetShader(impl->pixel_shader_ptr, NULL, 0);
		 
		impl->d3d_device_ctx->Draw(vertex_count, 0);
	}
}

void vp::SdlDx11Application::postFrame(vp::Application& owner)
{
	if (!valid)
		return;

	/////////////////////////////////////////////////////////////////////////////////
	// prepare frame
	{
		ImVec4 clear_color = ImVec4(0.17f, 0.21f, 0.25f, 1.00f);
		const float clear_color_with_alpha[4] = {clear_color.x * clear_color.w,
			clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w};
		impl->d3d_device_ctx->OMSetRenderTargets(1, &impl->main_target_view, nullptr);
		impl->d3d_device_ctx->ClearRenderTargetView(impl->main_target_view, clear_color_with_alpha);


		ImGui_ImplDX11_NewFrame(); // compiles shaders when called first time
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	}
	// prepare frame
	if (vertex_buffer_ptr)
	{  
		UINT vertex_stride = 3 * sizeof(float);
		UINT vertex_offset = 0;
		UINT vertex_count = 3;

		impl->d3d_device_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		impl->d3d_device_ctx->IASetInputLayout(impl->input_layout_ptr);
		impl->d3d_device_ctx->IASetVertexBuffers(
			0, 1, &vertex_buffer_ptr, &vertex_stride, &vertex_offset);

		impl->d3d_device_ctx->VSSetShader(impl->vertex_shader_ptr, NULL, 0);
		impl->d3d_device_ctx->PSSetShader(impl->pixel_shader_ptr, NULL, 0);

		impl->d3d_device_ctx->Draw(vertex_count, 0);
	}
	/////////////////////////////////////////////////////////////////////////////////
	impl->swap_chain->Present(0, 0); // Present without vsync
	/////////////////////////////////////////////////////////////////////////////////
}


void vp::SdlDx11Application::teardown(vp::Application& owner)
{
	if (!valid)
		return;

	impl->cleanupDeviceD3D();
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	valid = false;
}
