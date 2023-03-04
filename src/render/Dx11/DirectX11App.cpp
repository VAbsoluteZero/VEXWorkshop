#include "DirectX11App.h"

// dx11
#include <d3d11.h>
#include <d3dcompiler.h> // shader compiler
#include <dxgi.h>        // direct X driver interface
// sdl / imgui / third party
#include <SDL.h>
#include <SDL_filesystem.h>
#include <SDL_syswm.h>
#include <SDL_video.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_sdl.h>
#include <imgui.h>
#include <imgui/gizmo/ImGuizmo.h>
// vex
#include <application/Application.h>
#include <utils/ImGuiUtils.h>

#include "DirectXTypes.h"

void Dx11Viewport::initialize(ID3D11Device* d3d_device, const ViewportInitArgs& args)
{
    createdWithArgs = args;
    // srv setup
    {
        D3D11_TEXTURE2D_DESC viewport_tex_desc{
            .Width = args.size.x,
            .Height = args.size.y,
            .MipLevels = 1,
            .ArraySize = 1,
            .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
            .SampleDesc = {.Count = 1},
            .Usage = D3D11_USAGE_DEFAULT,
            .BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
            .CPUAccessFlags = 0,
        };
        auto hr = d3d_device->CreateTexture2D(&viewport_tex_desc, NULL, &tex);
        check(SUCCEEDED(hr), "could not CreateTexture2D");

        D3D11_RENDER_TARGET_VIEW_DESC viewport_tv_desc{
            .Format = viewport_tex_desc.Format,
            .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
            .Texture2D = {.MipSlice = 0},
        };
        hr = d3d_device->CreateRenderTargetView(tex, &viewport_tv_desc, &view);
        check(SUCCEEDED(hr), "could not CreateRenderTargetView");

        D3D11_SHADER_RESOURCE_VIEW_DESC viewport_shader_res_desc{
            .Format = viewport_tex_desc.Format,
            .ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
            .Texture2D = {.MostDetailedMip = 0, .MipLevels = 1},
        };

        hr = d3d_device->CreateShaderResourceView(tex, &viewport_shader_res_desc, &srv);
        check(SUCCEEDED(hr), "could not CreateShaderResourceView");
    }
    // create depth buffer for viewport
    {
        D3D11_TEXTURE2D_DESC ds_desc{
            .Width = args.size.x,
            .Height = args.size.y,
            .MipLevels = 1,
            .ArraySize = 1,
            .Format = DXGI_FORMAT_D24_UNORM_S8_UINT,
            .SampleDesc = {.Count = 1, .Quality = 0},
            .Usage = D3D11_USAGE_DEFAULT,
            .BindFlags = D3D11_BIND_DEPTH_STENCIL,
            .CPUAccessFlags = 0,
        };

        auto hr = d3d_device->CreateTexture2D(&ds_desc, nullptr, &stencil_tex);
        check_(!FAILED(hr));
        hr = d3d_device->CreateDepthStencilView(stencil_tex, nullptr, &stencil_view);
        check_(!FAILED(hr));
    }
    // Create depth-stencil State
    {
        D3D11_DEPTH_STENCIL_DESC stencil_state_desc{
            .DepthEnable = true,
            .DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL,
            .DepthFunc = D3D11_COMPARISON_LESS,
            .StencilEnable = false,
        };
        auto hr = d3d_device->CreateDepthStencilState(&stencil_state_desc, &stencil_state);
        check_(!FAILED(hr));
    }
    // Create the rasterizer state for view
    {
        D3D11_RASTERIZER_DESC rasterized_desc{
            .FillMode = args.wireframe ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID, // ,
            .CullMode = D3D11_CULL_NONE,
            .FrontCounterClockwise = false,
            .DepthBias = 0,
            .DepthBiasClamp = 0.0f,
            .SlopeScaledDepthBias = 0.0f,
            .DepthClipEnable = true,
            .ScissorEnable = false,
            .MultisampleEnable = false,
            .AntialiasedLineEnable = false,
        };
        auto hr = d3d_device->CreateRasterizerState(&rasterized_desc, //
            &rasterizer_state);
        check_(!FAILED(hr));
    }
    // Create the sampler state for view
    {
        D3D11_SAMPLER_DESC samplerDesc{
            .Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR,
            .AddressU = D3D11_TEXTURE_ADDRESS_WRAP,
            .AddressV = D3D11_TEXTURE_ADDRESS_WRAP,
            .AddressW = D3D11_TEXTURE_ADDRESS_CLAMP,
            .ComparisonFunc = D3D11_COMPARISON_NEVER,
            .MinLOD = FLT_MIN,
            .MaxLOD = FLT_MAX,
        };
        auto hr = d3d_device->CreateSamplerState(&samplerDesc, &sampler_state);
        check_(!FAILED(hr));
    }
}

struct Dx11RenderInterface
{
    static constexpr i32 k_max_viewports = 8;
    Dx11Globals globals;
    Dx11Window dx_window;

    std::vector<Dx11Viewport> viewports;

    bool initialized = false;
    Dx11RenderInterface() = default;
    Dx11RenderInterface(const Dx11RenderInterface&) = delete;
    ~Dx11RenderInterface() { release(); }

    auto init(vp::Application& owner) -> i32
    {
        SDL_Window* sdl_window = owner.getMainWindow()->getRawWindow();
        viewports.reserve(k_max_viewports + 1);
        // create d3d device and swapcahin
        {
            SDL_SysWMinfo sdl_wm_info;
            SDL_VERSION(&sdl_wm_info.version);
            SDL_GetWindowWMInfo(sdl_window, &sdl_wm_info);
            HWND window_handle = (HWND)sdl_wm_info.info.win.window;

            DXGI_SWAP_CHAIN_DESC sd{
                .BufferDesc =
                    {
                        .Width = 0,
                        .Height = 0,
                        .RefreshRate =
                            {
                                .Numerator = 0,
                                .Denominator = 1,
                            },
                        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
                    },
                .SampleDesc = {.Count = 1},
                .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
                .BufferCount = 2,
                .OutputWindow = window_handle,
                .Windowed = true,
                .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
                .Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH,
            };

            // check Scaling.
            u32 createDeviceFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
            createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

            D3D_FEATURE_LEVEL featureLevel;
            const D3D_FEATURE_LEVEL featureLevelArray[2] = {
                D3D_FEATURE_LEVEL_11_1,
                D3D_FEATURE_LEVEL_11_0,
            };

            globals = {};
            if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                    createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd,
                    &globals.swap_chain, &globals.d3d_device, &featureLevel,
                    &globals.im_ctx) != S_OK)
            {
                checkLethal(false, "D3D11CreateDeviceAndSwapChain failed to create objects.");
                return 1;
            }
            auto lvl1 = globals.d3d_device->GetFeatureLevel();
            bool last = lvl1 == D3D_FEATURE_LEVEL_11_1;
        }

        int wnd_x = 128;
        int wnd_y = 128;
        SDL_GetWindowSize(sdl_window, &wnd_x, &wnd_y);
        createWindowResources((u32)wnd_x, (u32)wnd_y);

        return dx_window.isValid() ? 0 : 1;
    }

    auto createWindowResources(u32 wnd_x, u32 wnd_y) -> void
    {
        // create main window
        ID3D11Texture2D* back_buffer = nullptr;
        auto hr = globals.swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
        check_(!FAILED(hr));
        hr = globals.d3d_device->CreateRenderTargetView(
            back_buffer, nullptr, &dx_window.target_view);
        check_(!FAILED(hr));
        back_buffer->Release();

        // depth-stencil State  NO DEPTH
        {
            D3D11_DEPTH_STENCIL_DESC desc{
                .DepthEnable = false,
                .DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL,
                .DepthFunc = D3D11_COMPARISON_ALWAYS,
                .StencilEnable = true,
                .FrontFace = {.StencilFunc = D3D11_COMPARISON_ALWAYS},
            };
            desc.FrontFace.StencilFailOp = desc.FrontFace.StencilDepthFailOp =
                desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
            desc.BackFace = desc.FrontFace;

            hr = globals.d3d_device->CreateDepthStencilState(&desc, &dx_window.stencil_state);
            check_(!FAILED(hr));
        }
        // Create Depth/Stencil buffer
        {
            D3D11_TEXTURE2D_DESC ds_desc{
                .Width = (u32)wnd_x,
                .Height = (u32)wnd_y,
                .MipLevels = 1,
                .ArraySize = 1,
                .Format = DXGI_FORMAT_D24_UNORM_S8_UINT,
                .SampleDesc = {.Count = 1, .Quality = 0},
                .Usage = D3D11_USAGE_DEFAULT,
                .BindFlags = D3D11_BIND_DEPTH_STENCIL,
                .CPUAccessFlags = 0,
            };

            hr = globals.d3d_device->CreateTexture2D(&ds_desc, nullptr, &dx_window.stencil_tex);
            check_(!FAILED(hr));

            hr = globals.d3d_device->CreateDepthStencilView(
                dx_window.stencil_tex, nullptr, &dx_window.stencil_view);
            check_(!FAILED(hr));
        }
        // Create the rasterizer state
        {
            D3D11_RASTERIZER_DESC desc{
                .FillMode = D3D11_FILL_SOLID,
                .CullMode = D3D11_CULL_NONE,
                .DepthClipEnable = true,
                .ScissorEnable = true,
            };
            hr = globals.d3d_device->CreateRasterizerState(&desc, &dx_window.raster_state);
            check_(!FAILED(hr));
        }
        checkRel(dx_window.isValid(), "all fields must be set");
    }

    auto resizeWindow(vp::Application& owner, u32 w, u32 h) -> void
    {
        dx_window.release();
        // must be after release so RTV is released
        auto hr = globals.swap_chain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
        check_(!FAILED(hr));
        createWindowResources(w, h);

        D3D11_VIEWPORT vp{};
        vp.Width = w;
        vp.Height = h;
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        vp.TopLeftX = vp.TopLeftY = 0;
        globals.im_ctx->RSSetViewports(1, &vp);
    }

    auto preFrame(vp::Application& owner) -> void
    {
        auto main_window = owner.getMainWindow();
        D3D11_VIEWPORT vp{};
        vp.Width = main_window->windowSize().x;
        vp.Height = main_window->windowSize().y;
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        vp.TopLeftX = vp.TopLeftY = 0;
        globals.im_ctx->RSSetViewports(1, &vp);
    }
    auto frame(vp::Application& owner) -> void {}
    auto postFrame(vp::Application& owner) -> void
    {
        // clear frame and buffers, set targets
        ImVec4 clear_color = ImVec4(0.37f, 0.21f, 0.25f, 1.00f);
        const float color[4] = {clear_color.x * clear_color.w, clear_color.y * clear_color.w,
            clear_color.z * clear_color.w, clear_color.w};

        globals.im_ctx->OMSetRenderTargets(1, &dx_window.target_view, dx_window.stencil_view);
        globals.im_ctx->ClearRenderTargetView(dx_window.target_view, color);

        globals.im_ctx->OMSetDepthStencilState(dx_window.stencil_state, 0);
        globals.im_ctx->ClearDepthStencilView(
            dx_window.stencil_view, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

        globals.im_ctx->RSSetState(dx_window.raster_state);
    }
    auto endFrame() -> void
    {
        globals.swap_chain->Present(0, 0); // Present without vsync
    }
    auto release() -> void
    {
        initialized = false;
        dx_window.release();
        globals.release();
    }

    auto canAddViewport() const -> bool { return viewports.size() <= k_max_viewports; }
    auto addViewport(const ViewportInitArgs& args) -> u32
    {
        if (viewports.size() >= k_max_viewports)
            return 0;

        Dx11Viewport vp;
        vp.initialize(globals.d3d_device, args);
        viewports.emplace_back(std::move(vp));

        return vp.uid;
    }
    auto findViewport(u32 id) -> Dx11Viewport*
    {
        for (auto& vp : viewports)
        {
            if (vp.uid == id)
                return &vp;
        }
        return nullptr;
    }
    auto resetViewport(u32 id, const ViewportInitArgs& args) -> bool
    { //
        if (Dx11Viewport* vp = findViewport(id); vp && vp->isValid())
        {
            vp->release();
            vp->initialize(globals.d3d_device, args);
            return true;
        }
        return false;
    }
    auto removeViewport(u32 id) -> bool
    {
        if (Dx11Viewport* vp = findViewport(id); vp && vp->isValid())
        {
            vp->release();
            return true;
        }
        return false;
    }
};

// call impl
vp::DirectX11App::~DirectX11App()
{
    if (impl)
    {
        impl->release();
        delete impl;
    }
}
i32 vp::DirectX11App::init(vp::Application& owner)
{
    if (!checkAlways(impl == nullptr, "must not call init more than once"))
        return 1;
    auto tst = SDL_GetBasePath();
    spdlog::info("path base : {}", tst);

    impl = new Dx11RenderInterface();
    auto err = impl->init(owner);
    if (err == 0)
    {
        SDL_Window* sdl_window = owner.getMainWindow()->getRawWindow();
        [[maybe_unused]] auto c = ImGui::CreateContext();
        ImGui_ImplDX11_Init(impl->globals.d3d_device, impl->globals.im_ctx);
        ImGui_ImplSDL2_InitForSDLRenderer(sdl_window);

        // vp::g_view_hub.views.emplace(g_view_name, vp::ImView{g_view_name});
    }

    ViewportInitArgs args;
    u32 id = impl->addViewport(args);
    imgui_views.push_back({true, false, id});

    return err;
}
void vp::DirectX11App::preFrame(vp::Application& owner)
{
    checkLethal(impl != nullptr, "unexpected null in vp::DirectX11App::preFrame");
    ImGui_ImplSDL2_NewFrame(); // window & inputs related frame init specific for SDL
    ImGui::NewFrame();

    impl->preFrame(owner);
}
void vp::DirectX11App::frame(vp::Application& owner) { impl->frame(owner); }
void vp::DirectX11App::postFrame(vp::Application& owner)
{
    // clear screen buffer
    impl->postFrame(owner);
    // ImGui pass 
    ImGuiIO& io = ImGui::GetIO();
    auto nav_old = io.ConfigWindowsMoveFromTitleBarOnly;
    defer_ { io.ConfigWindowsMoveFromTitleBarOnly = true; };
    // add to menu
    if (ImGui::BeginMainMenuBar())
    {
        defer_ { ImGui::EndMainMenuBar(); };
        // imgui stuff
        if (ImGui::BeginMenu("Viewports"))
        {
            defer_ { ImGui::EndMenu(); };

            if (impl->canAddViewport() && ImGui::Button("Add Viewport"))
            {
                ViewportInitArgs args;
                u32 id = impl->addViewport(args);
                imgui_views.push_back({true, false, id});
            }
            for (ImViewport& vp : imgui_views)
            {
                auto s = fmt::format("Viewport {}", vp.native_vp_id);
                if (ImGui::MenuItem(s.data(), nullptr, vp.visible))
                {
                    vp.visible = !vp.visible;
                }
                if (!vp.visible)
                    continue;
            }
        }

        ImGui::Bullet();
        ImGui::Text(" perf: %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
            ImGui::GetIO().Framerate);
    }

    // draw enabled viewports
    for (ImViewport& vp : imgui_views)
    {
        auto s = fmt::format("Viewport {}", vp.native_vp_id);
        if (!vp.visible)
            continue;

        bool visible = ImGui::Begin(s.data());
        defer_ { ImGui::End(); };

        ImVec2 vmin = ImGui::GetWindowContentRegionMin();
        ImVec2 vmax = ImGui::GetWindowContentRegionMax();
        auto deltas = ImVec2{vmax.x - vmin.x, vmax.y - vmin.y};
        v2i32 cur_size = {deltas.x > 0 ? deltas.x : 32, deltas.y > 0 ? deltas.y : 32};

        Dx11Viewport* native_vp = impl->findViewport(vp.native_vp_id);
        check_(native_vp);

        if (!visible || native_vp == nullptr || !native_vp->isValid())
            continue;

        if (cur_size != vp.last_seen_size)
        {
            ViewportInitArgs args = native_vp->createdWithArgs;
            args.size = cur_size;
            impl->resetViewport(vp.native_vp_id, args);
        }
        vp.last_seen_size = cur_size;

        ImGui::Image(
            (void*)(uintptr_t)native_vp->srv, deltas, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));

        ImGui::SetCursorPos({20, 40});
        if (ImGui::Checkbox("pause", &vp.paused))
        {
        }
        ImGui::Separator();
    }

    ImGui_ImplDX11_NewFrame(); // compiles shaders when called first time
    ImGuizmo::BeginFrame();
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    // present frame
    impl->endFrame();
}
void vp::DirectX11App::teardown(vp::Application& owner)
{
    impl->release();
    delete impl;
    impl = nullptr;
}
void vp::DirectX11App::handleWindowResize(vp::Application& owner, v2u32 size)
{ //
    impl->resizeWindow(owner, size.x, size.y);
}
