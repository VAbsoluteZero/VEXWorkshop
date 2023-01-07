#include <algorithm>
#include <functional>
//
#define TINYOBJLOADER_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION

#include <SDL.h>
#include <SDL_syswm.h>
#include <SDL_video.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_sdl.h>
#include <d3d11.h>
#include <d3dcompiler.h> // shader compiler
#include <dxgi.h>		 // direct X driver interface
#include <imgui.h>
#include <spdlog/stopwatch.h>
#include <stb/stb_image.h>
#include <tinyobj/tiny_obj_loader.h>
#include <utils/ImGuiUtils.h>

#include <glm/gtx/hash.hpp>

#include "Application.h"
#include "SDLDx11Application.h"

namespace vp
{
	struct SdlDx11Impl
	{
		SDL_SysWMinfo sdl_wm_info;
		SDL_Window* sdl_window = nullptr;
		HWND window_handle;

		ID3D11Device* d3d_device = nullptr;
		ID3D11DeviceContext* d3d_device_ctx = nullptr;
		IDXGISwapChain* swap_chain = nullptr;

		ID3DBlob* error_blob = nullptr;

		// plain shader stuff
		ID3DBlob* vertshad_plain = nullptr;
		ID3DBlob* pixelshad_plain = nullptr;
		ID3D11InputLayout* sil_plain = nullptr; 

		ID3D11VertexShader* vshad_plain = nullptr;
		ID3D11PixelShader* pshad_plain = nullptr;

		// plain2 shader stuff
		ID3DBlob* vertshad_plain2 = nullptr;                  
		ID3DBlob* pixelshad_plain2 = nullptr;
		ID3D11InputLayout* sil_plain2 = nullptr;

		ID3D11VertexShader* vshad_plain2 = nullptr;
		ID3D11PixelShader* pshad_plain2 = nullptr;

		// diffuse shader stuff
		ID3DBlob* vertshad_diffuse = nullptr;
		ID3DBlob* pixelshad_diffuse = nullptr;
		ID3D11InputLayout* sil_diffuse = nullptr;

		// main window viewport
		ID3D11RenderTargetView* window_target_view = nullptr;
		ID3D11RasterizerState* rasterizer_state = nullptr;

		ID3D11Texture2D* stencil_buffer_tex = nullptr;
		ID3D11DepthStencilView* stencil_view = nullptr;
		ID3D11DepthStencilState* stencil_state = nullptr;

		// game viewport
		ID3D11Texture2D* viewport_tex2d = nullptr;
		ID3D11RenderTargetView* viewport_target_view = nullptr;
		ID3D11ShaderResourceView* viewport_srv = nullptr;

		ID3D11Texture2D* viewport_stencil_tex = nullptr;
		ID3D11DepthStencilView* viewport_stencil_view = nullptr;
		ID3D11DepthStencilState* viewport_stencil_state = nullptr;
		ID3D11RasterizerState* viewport_rasterizer_state = nullptr;

		void initHWND(SDL_Window* raw_window)
		{
			sdl_window = raw_window;
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
			sd.BufferDesc.RefreshRate.Numerator = 0;
			sd.BufferDesc.RefreshRate.Denominator = 1;
			sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
			sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			sd.OutputWindow = window_handle;
			sd.SampleDesc.Count = 1;
			sd.SampleDesc.Quality = 0;
			sd.Windowed = true;
			sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // DXGI_SWAP_EFFECT_DISCARD;
													  // DXGI_SWAP_EFFECT_FLIP_DISCARD

			u32 createDeviceFlags = 0;

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
				std::abort();
				return false;
			}
			initialieMainViewRenderTarget();
			return true;
		}
		bool initialieMainViewRenderTarget()
		{
			if (nullptr == d3d_device || nullptr == swap_chain)
				return false;

			ID3D11Texture2D* back_buffer = nullptr;
			swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
			d3d_device->CreateRenderTargetView(back_buffer, nullptr, &window_target_view);
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
				d3d_device->CreateDepthStencilState(&desc, &viewport_stencil_state);
			} 
			// Create the rasterizer state
			{
				D3D11_RASTERIZER_DESC desc;
				ZeroMemory(&desc, sizeof(desc));

				desc.FillMode = D3D11_FILL_SOLID;
				desc.CullMode = D3D11_CULL_NONE;
				desc.ScissorEnable = true;
				desc.DepthClipEnable = true;
				d3d_device->CreateRasterizerState(&desc, &rasterizer_state);
			}
			return true;
			// Create Depth/Stencil buffer
			{
				D3D11_TEXTURE2D_DESC ds_desc;
				ZeroMemory(&ds_desc, sizeof(D3D11_TEXTURE2D_DESC));
				ds_desc.ArraySize = 1;
				ds_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
				ds_desc.CPUAccessFlags = 0; // No CPU access required.
				ds_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
				int wnd_x = 128;
				int wnd_y = 128;
				SDL_GetWindowSize(sdl_window, &wnd_x, &wnd_y);
				ds_desc.Width = wnd_x;
				ds_desc.Height = wnd_y;

				ds_desc.MipLevels = 1;
				ds_desc.SampleDesc.Count = 1;
				ds_desc.SampleDesc.Quality = 0;
				ds_desc.Usage = D3D11_USAGE_DEFAULT;
				auto hr = d3d_device->CreateTexture2D(&ds_desc, nullptr, &stencil_buffer_tex);
				if (FAILED(hr))
				{
					return false;
				}
				hr = d3d_device->CreateDepthStencilView(stencil_buffer_tex, nullptr, &stencil_view);
				if (FAILED(hr))
				{
					return false;
				}
			}

			return true;
		}

		void initViewportStuff()
		{
			u32 flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(DEBUG) || defined(_DEBUG)
			flags |= D3DCOMPILE_DEBUG; // add more debug output
#endif
			constexpr auto vp_size_w = 1800;
			constexpr auto vp_size_h = 1800;

			// plain shader IL
			{
				// COMPILE VERTEX SHADER
				HRESULT hr = D3DCompileFromFile(L"content/shaders/plain_dbg.hlsl", nullptr,
					D3D_COMPILE_STANDARD_FILE_INCLUDE, "vs_main", "vs_5_0", flags, 0,
					&vertshad_plain, &error_blob);
				if (FAILED(hr))
				{
					if (error_blob)
					{
						OutputDebugStringA((char*)error_blob->GetBufferPointer());
						error_blob->Release();
					}
					if (vertshad_plain)
					{
						vertshad_plain->Release();
					}
					VEX_DBGBREAK();
				}

				// COMPILE PIXEL SHADER
				hr = D3DCompileFromFile(L"content/shaders/plain_dbg.hlsl", nullptr,
					D3D_COMPILE_STANDARD_FILE_INCLUDE, "ps_main", "ps_5_0", flags, 0,
					&pixelshad_plain, &error_blob);
				if (FAILED(hr))
				{
					if (error_blob)
					{
						OutputDebugStringA((char*)error_blob->GetBufferPointer());
						error_blob->Release();
					}
					if (pixelshad_plain)
					{
						pixelshad_plain->Release();
					}
					VEX_DBGBREAK();
				}

				hr = d3d_device->CreateVertexShader(vertshad_plain->GetBufferPointer(),
					vertshad_plain->GetBufferSize(), NULL, &vshad_plain);
				check_(SUCCEEDED(hr));

				hr = d3d_device->CreatePixelShader(pixelshad_plain->GetBufferPointer(),
					pixelshad_plain->GetBufferSize(), NULL, &pshad_plain);
				check_(SUCCEEDED(hr));

				D3D11_INPUT_ELEMENT_DESC input_desc[] = {
					{"POS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0}};

				hr = d3d_device->CreateInputLayout(input_desc, ARRAYSIZE(input_desc),
					vertshad_plain->GetBufferPointer(), vertshad_plain->GetBufferSize(),
					&sil_plain);

				check(SUCCEEDED(hr), "could not create input layout for shader");
			}

#if 0
			// main shader IL
			{
				// COMPILE VERTEX SHADER
				HRESULT hr = D3DCompileFromFile(L"content/shaders/simple.hlsl", nullptr,
					D3D_COMPILE_STANDARD_FILE_INCLUDE, "vs_main", "vs_5_0", flags, 0,
					&vertshad_diffuse, &error_blob);
				if (FAILED(hr))
				{
					if (error_blob)
					{
						OutputDebugStringA((char*)error_blob->GetBufferPointer());
						error_blob->Release();
					}
					if (vertshad_diffuse)
					{
						vertshad_diffuse->Release();
					}
					VEX_DBGBREAK();
				}

				// COMPILE PIXEL SHADER
				hr = D3DCompileFromFile(L"content/shaders/simple.hlsl", nullptr,
					D3D_COMPILE_STANDARD_FILE_INCLUDE, "ps_main", "ps_5_0", flags, 0,
					&pixelshad_diffuse, &error_blob);
				if (FAILED(hr))
				{
					if (error_blob)
					{
						OutputDebugStringA((char*)error_blob->GetBufferPointer());
						error_blob->Release();
					}
					if (pixelshad_diffuse)
					{
						pixelshad_diffuse->Release();
					}
					VEX_DBGBREAK();
				}


				D3D11_INPUT_ELEMENT_DESC input_desc[] = {
					{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA,
						0},
					{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA,
						0},
					{"TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA,
						0}};

				hr = d3d_device->CreateInputLayout(input_desc, ARRAYSIZE(input_desc),
					vertshad_diffuse->GetBufferPointer(), vertshad_diffuse->GetBufferSize(),
					&sil_diffuse);

				check(SUCCEEDED(hr), "could not create input layout for shader");
			}
#endif

			// test viewport stuff
			{
				D3D11_TEXTURE2D_DESC viewport_tex_desc;
				D3D11_RENDER_TARGET_VIEW_DESC viewport_tv_desc;
				D3D11_SHADER_RESOURCE_VIEW_DESC viewport_shader_res_desc;

				// Initialize the  texture description
				ZeroMemory(&viewport_tex_desc, sizeof(viewport_tex_desc));
				ZeroMemory(&viewport_tv_desc, sizeof(viewport_tv_desc));
				ZeroMemory(&viewport_shader_res_desc, sizeof(viewport_shader_res_desc));

				viewport_tex_desc.Width = vp_size_w;
				viewport_tex_desc.Height = vp_size_h;
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

				// Setup the description of the render target view.
				viewport_tv_desc.Format = viewport_tex_desc.Format;
				viewport_tv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
				viewport_tv_desc.Texture2D.MipSlice = 0;

				// Create the render target view.
				d3d_device->CreateRenderTargetView(
					viewport_tex2d, &viewport_tv_desc, &viewport_target_view);

				// Setup the description of the shader resource view.
				viewport_shader_res_desc.Format = viewport_tex_desc.Format;
				viewport_shader_res_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
				viewport_shader_res_desc.Texture2D.MostDetailedMip = 0;
				viewport_shader_res_desc.Texture2D.MipLevels = 1;

				// Create the shader resource view.
				auto result_code = d3d_device->CreateShaderResourceView(
					viewport_tex2d, &viewport_shader_res_desc, &viewport_srv);
				check(SUCCEEDED(result_code), "could not CreateShaderResourceView");
			}

			return;
			// create depth bufer for viewport
			{
				D3D11_TEXTURE2D_DESC ds_desc;
				ZeroMemory(&ds_desc, sizeof(D3D11_TEXTURE2D_DESC));

				ds_desc.ArraySize = 1;
				ds_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
				ds_desc.CPUAccessFlags = 0; // No CPU access required.
				ds_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

				ds_desc.Width = vp_size_w;
				ds_desc.Height = vp_size_h;

				ds_desc.MipLevels = 1;
				ds_desc.SampleDesc.Count = 1;
				ds_desc.SampleDesc.Quality = 0;
				ds_desc.Usage = D3D11_USAGE_DEFAULT;

				auto hr = d3d_device->CreateTexture2D(&ds_desc, nullptr, &viewport_stencil_tex);
				if (FAILED(hr))
				{
					return;
				}
				hr = d3d_device->CreateDepthStencilView(
					viewport_stencil_tex, nullptr, &viewport_stencil_view);
				if (FAILED(hr))
				{
					return;
				}
			}
			// Create depth-stencil State
			{
				// Setup depth/stencil state.
				D3D11_DEPTH_STENCIL_DESC stencil_state_desc;
				ZeroMemory(&stencil_state_desc, sizeof(D3D11_DEPTH_STENCIL_DESC));

				stencil_state_desc.DepthEnable = true;
				stencil_state_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
				stencil_state_desc.DepthFunc = D3D11_COMPARISON_LESS;
				stencil_state_desc.StencilEnable = false;

				auto hr = d3d_device->CreateDepthStencilState(
					&stencil_state_desc, &viewport_stencil_state);
			}

			// Create the rasterizer state for view
			{
				D3D11_RASTERIZER_DESC rasterized_desc;
				ZeroMemory(&rasterized_desc, sizeof(D3D11_RASTERIZER_DESC));

				rasterized_desc.AntialiasedLineEnable = false;
				rasterized_desc.CullMode = D3D11_CULL_BACK;
				rasterized_desc.DepthBias = 0;
				rasterized_desc.DepthBiasClamp = 0.0f;
				rasterized_desc.DepthClipEnable = true;
				rasterized_desc.FillMode = D3D11_FILL_SOLID;
				rasterized_desc.FrontCounterClockwise = false;
				rasterized_desc.MultisampleEnable = false;
				rasterized_desc.ScissorEnable = false;
				rasterized_desc.SlopeScaledDepthBias = 0.0f;

				// Create the rasterizer state object.
				auto hr = d3d_device->CreateRasterizerState(&rasterized_desc, //
					&viewport_rasterizer_state);
				if (FAILED(hr))
				{
					return;
				}
			}
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
			if (window_target_view)
			{
				window_target_view->Release();
				window_target_view = nullptr;
			}
		}
	};
} // namespace vp

// =================================================================================
// Data
static vp::SdlDx11Impl sdl_impl{};

constexpr auto g_view_name = "Viewport";

ID3D11Buffer* vertex_buffer_ptr = nullptr;
// static float vertex_data_array[] = {
//	0.0f, 0.5f, 0.0f,	// point at top
//	1.0f, -0.5f, 0.0f,	// point at bottom-right
//	-0.1f, -0.5f, 0.0f, // point at bottom-left
// };
static float vertex_data_array[] = {
	0.0f, 0.5f, .0f,   // point at top
	0.5f, -0.5f, .0f,  // point at bottom-right
	-0.5f, -0.5f, .0f, // point at bottom-left
};
struct MatricesMVP
{
	mtx4 model{1.0f};
	mtx4 view{1.0f};
	mtx4 proj{1.0f};
};

struct Vertex
{
	v3f pos = v3f_zero;
	v3f norm = v3f_zero;
	v2f tex_coord = v2f_zero;

	bool operator==(const Vertex&) const = default;
	bool operator!=(const Vertex&) const = default;
};

namespace std
{
	template <>
	struct hash<Vertex>
	{
		// #todo : benchmark
		size_t operator()(Vertex const& vertex) const { return vex::util::fnv1a_obj(vertex); }
	};
} // namespace std

struct Triangle
{
	v3i32 i_pos;
	v3i32 i_norm;
	v2i32 i_uv;
};

struct TmpModel
{
	std::vector<Vertex> vertices;
	std::vector<u32> indices;

	ID3D11Buffer* vertex_buffer = nullptr;
	ID3D11Buffer* index_buffer = nullptr;

	void release()
	{
		if (vertex_buffer)
			vertex_buffer->Release();
		if (index_buffer)
			index_buffer->Release();
		vertex_buffer = nullptr;
		index_buffer = nullptr;
		vertices.clear();
		indices.clear();
	}

	void createGPUBuffers(const vp::SdlDx11Impl* dximpl)
	{
		check_(indices.size() > 0 && indices.size() > 0);
		// vertex buffer
		{
			auto vert_desc = vex::makeZeroed<D3D11_BUFFER_DESC>();

			vert_desc.ByteWidth = vertices.size() * sizeof(Vertex);
			vert_desc.Usage = D3D11_USAGE_DEFAULT;
			vert_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

			auto vert_subres = vex::makeZeroed<D3D11_SUBRESOURCE_DATA>();
			vert_subres.pSysMem = vertices.data();

			HRESULT hr = dximpl->d3d_device->CreateBuffer(&vert_desc, &vert_subres, &vertex_buffer);
			check_(SUCCEEDED(hr));
		}
		// index buffer
		{
			auto vert_desc = vex::makeZeroed<D3D11_BUFFER_DESC>();

			vert_desc.ByteWidth = indices.size() * sizeof(Vertex);
			vert_desc.Usage = D3D11_USAGE_DEFAULT;
			vert_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

			auto vert_subres = vex::makeZeroed<D3D11_SUBRESOURCE_DATA>();
			vert_subres.pSysMem = indices.data();

			HRESULT hr = dximpl->d3d_device->CreateBuffer(&vert_desc, &vert_subres, &index_buffer);
			check_(SUCCEEDED(hr));
		}
	}
};

ID3D11ShaderResourceView* createTextureImageView(ID3D11Device* d3d_device, const char* path)
{
	spdlog::stopwatch sw;
	//
	//
	int width, height, chanels;
	stbi_uc* pixels = stbi_load(path, &width, &height, &chanels, STBI_rgb_alpha);
	auto sz = width * height * 4;

	SPDLOG_DEBUG("Texture loader: loading PNG, elapsed: {} seconds", sw);
	if (!pixels)
	{
		check_(false);
		std::abort();
	}

	ID3D11ShaderResourceView* texture_view = nullptr;
	// Upload texture to graphics system
	{
		D3D11_TEXTURE2D_DESC desc;
		ZeroMemory(&desc, sizeof(desc));
		desc.Width = width;
		desc.Height = height;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;

		ID3D11Texture2D* tmp_texture = NULL;
		D3D11_SUBRESOURCE_DATA sub_resource;
		sub_resource.pSysMem = pixels;
		sub_resource.SysMemPitch = desc.Width * 4;
		sub_resource.SysMemSlicePitch = 0;
		auto tex_hr = d3d_device->CreateTexture2D(&desc, &sub_resource, &tmp_texture);
		IM_ASSERT(tmp_texture != NULL);

		// Create texture view
		D3D11_SHADER_RESOURCE_VIEW_DESC view_desc;
		ZeroMemory(&view_desc, sizeof(view_desc));
		view_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		view_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		view_desc.Texture2D.MipLevels = desc.MipLevels;
		view_desc.Texture2D.MostDetailedMip = 0;

		auto srv_hr = d3d_device->CreateShaderResourceView(tmp_texture, &view_desc, &texture_view);
		auto new_cnt = tmp_texture->Release();

		check_(false);
	}
	return texture_view;
}

// tmp, tinyobjloader allocates a ton.
TmpModel loadModel(const vp::SdlDx11Impl* dximpl, const char* path)
{
	spdlog::stopwatch debug_sw;

	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path))
	{
		SPDLOG_CRITICAL(warn + err);

		std::abort();
		return {};
	}

	TmpModel mod;
	SPDLOG_DEBUG("Object loader: parsing OBJ, elapsed: {} seconds", debug_sw);

	std::unordered_map<Vertex, uint32_t> uniqueVertices{};
	for (const auto& shape : shapes)
	{
		for (const auto& index : shape.mesh.indices)
		{
			Vertex vertex{};

			vertex.pos = {attrib.vertices[3 * index.vertex_index + 0],
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 2]};

			vertex.norm = {attrib.normals[3 * index.normal_index + 0],
				attrib.normals[3 * index.normal_index + 1],
				attrib.normals[3 * index.normal_index + 2]};

			vertex.tex_coord = {attrib.texcoords[2 * index.texcoord_index + 0],
				/*1.0f -*/ attrib.texcoords[2 * index.texcoord_index + 1]};

			if (uniqueVertices.count(vertex) == 0)
			{
				uniqueVertices[vertex] = static_cast<uint32_t>(mod.vertices.size());
				mod.vertices.push_back(vertex);
			}

			mod.indices.push_back(uniqueVertices[vertex]);
		}
	}

	// Create vertex buffer:
	mod.createGPUBuffers(dximpl);
	return mod;
}

ID3D11ShaderResourceView* g_demo_texview = nullptr;
static TmpModel g_demo;

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
		impl->initViewportStuff();
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
	// g_demo = loadModel(this->impl, "content/models/cc_demo/viking_room.obj");

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

static v2f im_viewport_size = {-1, -1};
static v2f last_drawn_viewport_sz = {-1, -1};

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
		view->delayed_gui_drawcalls.push_back([&](ArgsImViewUpd args)
			{ //
				bool visible = ImGui::Begin("Viewport");
				defer_ { ImGui::End(); };
				ImVec2 vmin = ImGui::GetWindowContentRegionMin();
				ImVec2 vmax = ImGui::GetWindowContentRegionMax();

				auto deltas = ImVec2{vmax.x - vmin.x, vmax.y - vmin.y};
				// auto sz_wnd = ImGui::GetWindowSize();

				im_viewport_size = {deltas.x, deltas.y};

				if (visible)
				{
					ImGui::Image((void*)(uintptr_t)impl->viewport_srv, deltas, ImVec2(0.0f, 1.0f),
						ImVec2(1.0f, 0.0f));
				}
			});
	}
}

void vp::SdlDx11Application::frame(vp::Application& owner)
{
	if (!valid)
		return;

	if (im_viewport_size.x <= 0 || im_viewport_size.y <= 0)
		return;


	if (last_drawn_viewport_sz != im_viewport_size)
	{
		// #todo resize buffers
		last_drawn_viewport_sz = im_viewport_size;
	}

	// prepare frame 
	{
		ImVec4 clear_color = ImVec4(0.17f, 0.11f, 0.25f, 1.00f);
		const float clear_color_with_alpha[4] = {clear_color.x * clear_color.w,
			clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w};

		impl->d3d_device_ctx->OMSetRenderTargets(1, &impl->viewport_target_view, nullptr);
		impl->d3d_device_ctx->ClearRenderTargetView(
			impl->viewport_target_view, clear_color_with_alpha);

		// Setup viewport
		auto main_window = owner.getMainWindow();
		D3D11_VIEWPORT vp;
		memset(&vp, 0, sizeof(D3D11_VIEWPORT));
		vp.Width = im_viewport_size.x;
		vp.Height = im_viewport_size.y;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		vp.TopLeftX = vp.TopLeftY = 0;
		impl->d3d_device_ctx->RSSetViewports(1, &vp);

		impl->d3d_device_ctx->OMSetDepthStencilState(impl->viewport_stencil_state, 0);
		impl->d3d_device_ctx->RSSetState(impl->viewport_rasterizer_state);

		u32 vertex_stride = 3 * sizeof(float);
		u32 vertex_offset = 0;
		u32 vertex_count = 3;

		impl->d3d_device_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		impl->d3d_device_ctx->IASetInputLayout(impl->sil_plain);
		impl->d3d_device_ctx->IASetVertexBuffers(
			0, 1, &vertex_buffer_ptr, &vertex_stride, &vertex_offset);

		impl->d3d_device_ctx->VSSetShader(impl->vshad_plain, NULL, 0);
		impl->d3d_device_ctx->PSSetShader(impl->pshad_plain, NULL, 0);

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
		impl->d3d_device_ctx->OMSetRenderTargets(1, &impl->window_target_view, nullptr);
		impl->d3d_device_ctx->ClearRenderTargetView(
			impl->window_target_view, clear_color_with_alpha);


		ImGui_ImplDX11_NewFrame(); // compiles shaders when called first time
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	}
	/////////////////////////////////////////////////////////////////////////////////
	impl->swap_chain->Present(0, 0); // Present without vsync
	/////////////////////////////////////////////////////////////////////////////////
}


void vp::SdlDx11Application::teardown(vp::Application& owner)
{
	if (!valid)
		return;

	g_demo.release();

	impl->cleanupDeviceD3D();
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	valid = false;
}
