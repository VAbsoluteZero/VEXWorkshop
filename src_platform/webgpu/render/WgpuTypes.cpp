#include "WgpuTypes.h"

#include <SDL.h>
#include <SDL_filesystem.h>
#include <SDL_syswm.h>
#include <SDL_video.h>
#include <VCore/Utils/VUtilsBase.h>
#include <gfx/GfxUtils.h>
#include <spdlog/stopwatch.h>
#include <stb/stb_image.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#ifdef __EMSCRIPTEN__
    #include <emscripten.h>
    #include <emscripten/html5.h>
    #include <emscripten/html5_webgpu.h>
#else
#endif


void wgfx::Viewport::initialize(WGPUDevice device, const vex::ViewportOptions& args)
{
    options = args;
    {
        WGPUTextureDescriptor tex_desc{
            .label = "viewport texture",
            .usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_RenderAttachment |
                     WGPUTextureUsage_CopySrc,
            .dimension = WGPUTextureDimension_2D,
            .size =
                {
                    .width = args.size.x,
                    .height = args.size.y,
                    .depthOrArrayLayers = 1,
                },
            .format = args.srgb ? WGPUTextureFormat_RGBA8UnormSrgb : WGPUTextureFormat_RGBA8Unorm,
            .mipLevelCount = 1,
            .sampleCount = 1,
        };
        texture = wgpuDeviceCreateTexture(device, &tex_desc);

        WGPUTextureViewDescriptor view_desc{
            .label = "viewport texture view",
            .format = tex_desc.format,
            .dimension = WGPUTextureViewDimension_2D,
            .mipLevelCount = 1,
            .arrayLayerCount = 1,
            .aspect = WGPUTextureAspect_All,
        };
        view = wgpuTextureCreateView(texture, &view_desc);
        color_attachment.view = view;
    }
    // depth
    if (options.depth_enabled)
    {
        WGPUTextureFormat format = WGPUTextureFormat_Depth24PlusStencil8;
        u32 sample_count = 1;

        WGPUTextureDescriptor depth_texture_desc = {
            .usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc,
            .dimension = WGPUTextureDimension_2D,
            .size =
                {
                    .width = args.size.x,
                    .height = args.size.y,
                    .depthOrArrayLayers = 1,
                },
            .format = format,
            .mipLevelCount = 1,
            .sampleCount = sample_count,
        };
        depth_texture = wgpuDeviceCreateTexture(device, &depth_texture_desc);

        WGPUTextureViewDescriptor view_desc = {
            .format = depth_texture_desc.format,
            .dimension = WGPUTextureViewDimension_2D,
            .mipLevelCount = 1,
            .arrayLayerCount = 1,
            .aspect = WGPUTextureAspect_All,
        };
        depth_view = wgpuTextureCreateView(depth_texture, &view_desc);
        depth_attachment.view = depth_view;
    }
}

wgfx::RenderContext& wgfx::Viewport::setupForDrawing(const wgfx::Globals& globals)
{
    auto encoder = wgpuDeviceCreateCommandEncoder(globals.device, &this->encoder_desc);

    this->color_attachment.view = view;
    this->depth_attachment.view = depth_view;
    WGPURenderPassDescriptor pass{
        .label = "viewport render pass",
        .colorAttachmentCount = 1,
        .colorAttachments = &this->color_attachment,
        .depthStencilAttachment = depth_view ? &this->depth_attachment : nullptr,
    };
    auto render_pass_encoder = wgpuCommandEncoderBeginRenderPass(encoder, &pass);
    // #fixme REVERSE DEPTH ORDER
    wgpuRenderPassEncoderSetViewport(
        render_pass_encoder, 0.0f, 0.0f, options.size.x, options.size.y, 0.0f, 1.0f);
    wgpuRenderPassEncoderSetScissorRect(
        render_pass_encoder, 0u, 0u, options.size.x, options.size.y);

    context = wgfx::RenderContext{
        .device = globals.device,
        .encoder = encoder,
        .queue = globals.queue,
        .render_pass = render_pass_encoder,
        .cur_tex_view = view,
    };
    return context;
}


void wgfx::wgpuWait(std::atomic_bool& flag)
{
    while (!flag.load())
    {
#ifndef __EMSCRIPTEN__
        std::this_thread::yield();
#else
        SDL_Delay(1);
#endif
    }
}

void wgfx::wgpuPollWait(
    const RenderContext& context, std::atomic_bool& flag, double microsec_timeout)
{
    using namespace std::chrono_literals;
    spdlog::stopwatch sw;
    while (!flag)
    { // #fixme - in dawn there is Tick to poll events, in wgfx this is workaround
#ifdef VEX_GFX_WEBGPU_DAWN
        wgpuDeviceTick(context.device);
#else
        wgpuQueueSubmit(context.queue, 0, nullptr);
#endif
        double mic_sec = sw.elapsed() / 1.0us;
        if (microsec_timeout > 0 && mic_sec > microsec_timeout)
        {
            SPDLOG_ERROR(" wgpuPollWait timeout ");
            return;
        }
    }
}

WGPUInstance wgfx::createInstance(WGPUInstanceDescriptor desc)
{
    return k_using_emscripten ? nullptr : wgpuCreateInstance(&desc);
}

void wgfx::requestDevice(Globals& globals, WGPUDeviceDescriptor const* descriptor)
{
#ifndef __EMSCRIPTEN__
    std::atomic_bool request_done = ATOMIC_VAR_INIT(false);
    auto cb = [&](WGPURequestDeviceStatus status, WGPUDevice in_device, char const* message, void*)
    {
        if (!check(status == 0, "expected success result code"))
            SPDLOG_ERROR(" wgpu error: {} ", message);
        globals.device = in_device;
        request_done = true;
    };
    wgpuRequest(&wgpuAdapterRequestDevice, cb, globals.adapter, descriptor);
    // wgpuPollWait(instance, request_done);
    checkLethal(request_done.load(), "should exec synchronously");
#else
    globals.device = emscripten_webgpu_get_device();
#endif
}

void wgfx::requestAdapter(Globals& globals, WGPURequestAdapterOptions& options)
{
    std::atomic_bool request_done = ATOMIC_VAR_INIT(false);
    auto cb =
        [&](WGPURequestAdapterStatus status, WGPUAdapter in_adapter, char const* message, void*)
    {
        if (!check(status == 0, "expected success result code"))
            SPDLOG_ERROR(" wgpu error: {} ", message);
        globals.adapter = in_adapter;
        request_done = true;
    };
    wgpuRequest(&wgpuInstanceRequestAdapter, cb, globals.instance, &options);

#ifdef __EMSCRIPTEN__
    wgfx::wgpuWait(request_done);
#else
    checkLethal(request_done.load(), "should exec synchronously");
#endif
}

WGPUSurface wgfx::getWGPUSurface(WGPUInstance instance, SDL_Window* sdl_window)
{
#ifdef _WIN64
    {
        SDL_SysWMinfo sdl_wm_info;
        SDL_VERSION(&sdl_wm_info.version);
        SDL_GetWindowWMInfo(sdl_window, &sdl_wm_info);
        HWND hwnd = (HWND)sdl_wm_info.info.win.window;
        HINSTANCE hinstance = sdl_wm_info.info.win.hinstance;
        WGPUSurfaceDescriptorFromWindowsHWND hwnd_desc{
            .chain =
                {
                    .next = nullptr,
                    .sType = WGPUSType_SurfaceDescriptorFromWindowsHWND,
                },
            .hinstance = hinstance,
            .hwnd = hwnd,
        };
        WGPUSurfaceDescriptor d{
            .nextInChain = (const WGPUChainedStruct*)&hwnd_desc, .label = "surface"};
        return wgpuInstanceCreateSurface(instance, &d);
    }
#elif __EMSCRIPTEN__
    WGPUSurfaceDescriptorFromCanvasHTMLSelector canvDesc = {};
    canvDesc.chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector;
    canvDesc.selector = "canvas";

    WGPUSurfaceDescriptor surfDesc = {};
    surfDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&canvDesc);
    return wgpuInstanceCreateSurface(instance, &surfDesc);
#endif
}

#ifdef __EMSCRIPTEN__
EM_BOOL v_emscripten_stub(double /*time*/, void* userData)
{
    //  userData;
    return true;
}
void wgfx::swapchainPresent(WGPUSwapChain swap_chain)
{
    emscripten_request_animation_frame(v_emscripten_stub, nullptr);
}
#else
void wgfx::swapchainPresent(WGPUSwapChain swap_chain) { wgpuSwapChainPresent(swap_chain); }
#endif

auto wgfx::Globals::isValid() const -> bool
{
#ifdef __EMSCRIPTEN__
    return device && swap_chain;
#else
    return instance && device && instance && queue //
           && surface && swap_chain;
#endif
}

void wgfx::Globals::release()
{ //
    WGPU_REL(Surface, surface);
    WGPU_REL(SwapChain, swap_chain);
    // WGPU_REL(Device, device);
    wgpuDeviceDestroy(device);
    WGPU_REL(Adapter, adapter);
    WGPU_REL(Instance, instance);
}

wgfx::GpuBuffer wgfx::GpuBuffer::create(WGPUDevice device, GpuBuffer::Desc desc)
{
    const uint32_t size = (desc.size + 3) & ~3;
    GpuBuffer out_buf = {.desc = desc};
    WGPUBufferDescriptor buffer_desc = {
        .label = desc.label,
        .usage = (WGPUBufferUsageFlags)desc.usage,
        .size = size,
        .mappedAtCreation = false,
    };
    out_buf.buffer = wgpuDeviceCreateBuffer(device, &buffer_desc);
    return out_buf;
}

wgfx::GpuBuffer wgfx::GpuBuffer::create(
    WGPUDevice device, GpuBuffer::Desc desc, u8* data, u32 data_len_bytes)
{
    const uint32_t size = (desc.size + 3) & ~3;
    GpuBuffer wgpu_buffer = {.desc = desc};
    WGPUBufferDescriptor buffer_desc = {
        .label = desc.label,
        .usage = (WGPUBufferUsageFlags)desc.usage,
        .size = size,
        .mappedAtCreation = false,
    };
    if (checkAlwaysRel(data && data_len_bytes <= size, "invalid source buffer"))
    {
        buffer_desc.mappedAtCreation = true;
        WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &buffer_desc);
        void* dst = wgpuBufferGetMappedRange(buffer, 0, size);
        memcpy(dst, data, data_len_bytes);
        wgpuBufferUnmap(buffer);
        wgpu_buffer.buffer = buffer;
    }

    return wgpu_buffer;
}

void wgfx::GpuBuffer::copyViaStaging(CpyArgs args)
{
    auto& [device, encoder, at_offset, data, data_len] = args;

    const auto buff_size = (data_len + 3) & ~3;
    WGPUBufferDescriptor stag_desc = {
        .usage = WGPUBufferUsage_MapWrite | WGPUBufferUsage_CopySrc,
        .size = buff_size,
        .mappedAtCreation = true,
    };

    WGPUBuffer staging = wgpuDeviceCreateBuffer(device, &stag_desc);
    checkAlways_(staging != nullptr);

    void* dst = wgpuBufferGetMappedRange(staging, 0, buff_size);
    checkAlways_(dst != nullptr);

    memcpy(dst, data, data_len);
    wgpuBufferUnmap(staging);

    wgpuCommandEncoderCopyBufferToBuffer(encoder, staging, 0, buffer, at_offset, buff_size);
    WGPU_REL(Buffer, staging);
}

WGPUCommandBuffer wgfx::copyBufferToTexture(wgfx::CopyBuffToTexArgs args)
{
    WGPUCommandEncoder cmd_encoder = wgpuDeviceCreateCommandEncoder(args.device, nullptr);
    wgpuCommandEncoderCopyBufferToTexture(
        cmd_encoder, args.source_view, args.dest_view, &args.texture_size);
    WGPUCommandBuffer command_buffer = wgpuCommandEncoderFinish(cmd_encoder, nullptr);
    WGPU_REL(CommandEncoder, cmd_encoder);

    return command_buffer;
}

WGPUShaderModule wgfx::shaderFromSrc(WGPUDevice device, const char* src)
{
    checkLethal(src, "Unrecoverable : got nullptr instead of WGSL shader source");
    WGPUShaderModuleWGSLDescriptor shaderCodeDesc{};
    shaderCodeDesc.chain.next = nullptr;
    shaderCodeDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    shaderCodeDesc.source = src;
    WGPUShaderModuleDescriptor desc{};
    desc.nextInChain = nullptr;
    desc.nextInChain = &shaderCodeDesc.chain;
    return wgpuDeviceCreateShaderModule(device, &desc);
}
WGPUShaderModule wgfx::reloadShader(
    vex::TextShaderLib& shader_lib, const RenderContext& context, const char* shader_name)
{
    shader_lib.reload(shader_name);
    auto* src = shader_lib.shad_src.find(shader_name);
    if (!check(src, "shader not found"))
        return false;
    WGPUShaderModule shad_vert_frag = shaderFromSrc(context.device, src->text.c_str());
    std::atomic_bool sync_ready = ATOMIC_VAR_INIT(false);
    bool success = false;
    wgpuRequest(
        wgpuShaderModuleGetCompilationInfo,
        [&](WGPUCompilationInfoRequestStatus status, WGPUCompilationInfo const* compilationInfo,
            void*)
        {
            if (compilationInfo)
            {
                for (auto it : vex::Range(compilationInfo->messageCount))
                {
                    WGPUCompilationMessage message = compilationInfo->messages[it];
                    SPDLOG_WARN("Error (line: {}, pos: {}): {}", message.lineNum, message.linePos,
                        message.message);
                }
            }
            success = (compilationInfo->messageCount == 0);
            sync_ready = true;
        },
        shad_vert_frag);
    wgpuPollWait(context, sync_ready, 1000);
    if (!success)
    {
        WGPU_REL(ShaderModule, shad_vert_frag);
        return nullptr;
    }

    return shad_vert_frag;
}

WGPUVertexState wgfx::makeVertexState(WGPUDevice device, const VertShaderDesc& desc)
{
    WGPUVertexState vertex_state{
        .entryPoint = desc.entry,
        .constantCount = desc.constant_count,
        .constants = desc.constants,
        .bufferCount = desc.buffer_count,
        .buffers = desc.buffers,
    };

    if (desc.from == ShaderOrigin::Source)
    {
        vertex_state.module = shaderFromSrc(device, desc.source);
    }
    else if (desc.from == ShaderOrigin::ModuleProvided)
    {
        vertex_state.module = desc.module;
    }
    else if (desc.from != ShaderOrigin::Undefined)
    {
        check(false, "not implemented");
    }
    if (desc.from != ShaderOrigin::Undefined)
        check_(vertex_state.module);

    return vertex_state;
}

WGPUFragmentState wgfx::makeFragmentState(WGPUDevice device, const FragShaderDesc& desc)
{
    WGPUFragmentState fragment_state{
        .entryPoint = desc.entry,
        .constantCount = desc.constant_count,
        .constants = desc.constants,
        .targetCount = desc.target_count,
        .targets = desc.targets,
    };

    if (desc.from == ShaderOrigin::Source)
    {
        fragment_state.module = shaderFromSrc(device, desc.source);
    }
    else if (desc.from == ShaderOrigin::ModuleProvided)
    {
        fragment_state.module = desc.module;
    }
    else if (desc.from != ShaderOrigin::Undefined)
    {
        check(false, "not implemented");
    }
    if (desc.from != ShaderOrigin::Undefined)
        check_(fragment_state.module);

    return fragment_state;
}


wgfx::Texture wgfx::Texture::loadFormData(
    const RenderContext& ctx, LoadImgResult& loaded_img, LoadArgs options)
{
    u8* data = loaded_img.data;
    if (!checkAlways(data, "failed to load texture"))
    {
        return {};
    }

    WGPUExtent3D texture_size = {
        .width = loaded_img.width,
        .height = loaded_img.height,
        .depthOrArrayLayers = 1,
    };
    WGPUTextureDescriptor texture_desc = {
        .label = options.label,
        .usage = (WGPUTextureUsageFlags)options.usage,
        .dimension = WGPUTextureDimension_2D,
        .size = texture_size,
        .format = options.format,
        .mipLevelCount = 1,
        .sampleCount = 1,
    };
    WGPUTexture texture = wgpuDeviceCreateTexture(ctx.device, &texture_desc);

    copyImageToTexture(ctx, texture, data, texture_size, loaded_img.channel_count);

    return Texture{
        .texture = texture,
        .width = texture_desc.size.width,
        .height = texture_desc.size.height,
        .depth = texture_desc.size.depthOrArrayLayers,
        .mip_level_count = texture_desc.mipLevelCount,
        .format = texture_desc.format,
        .dimension = texture_desc.dimension,
    };
}

wgfx::Texture wgfx::Texture::loadFormFile(
    const RenderContext& ctx, const char* filename, LoadArgs options)
{
    LoadImgResult loaded_img = loadImage(filename, options.flip_y);
    auto tex = loadFormData(ctx, loaded_img, options);
    loaded_img.release();
    return tex;
}

void wgfx::Texture::copyImageToTexture(const RenderContext& ctx, WGPUTexture texture, void* pixels,
    WGPUExtent3D size, uint32_t channels)
{
    const u64 data_size = size.width * size.height * size.depthOrArrayLayers * channels;
    WGPUImageCopyTexture cpy_desc{
        .texture = texture,
        .mipLevel = 0,
        .origin =
            {
                .x = 0,
                .y = 0,
                .z = 0,
            },
        .aspect = WGPUTextureAspect_All,
    };
    WGPUTextureDataLayout layout{
        .offset = 0,
        .bytesPerRow = (u32)size.width * channels,
        .rowsPerImage = size.height,
    };
    wgpuQueueWriteTexture(ctx.queue, &cpy_desc, pixels, data_size, &layout, &size);
}

wgfx::TextureView wgfx::TextureView::create(
    WGPUDevice device, const Texture& from_tex, CreationArgs options)
{
    const bool is_cubemap = from_tex.depth == 6u;
    WGPUTextureViewDescriptor texture_view_dec = {
        .format = from_tex.format,
        .dimension = is_cubemap ? WGPUTextureViewDimension_Cube : WGPUTextureViewDimension_2D,
        .baseMipLevel = 0,
        .mipLevelCount = from_tex.mip_level_count,
        .baseArrayLayer = 0,
        .arrayLayerCount = from_tex.depth,
    };
    WGPUTextureView texture_view = wgpuTextureCreateView(from_tex.texture, &texture_view_dec);

    const bool pow2 = is_power_of_2(from_tex.width) && is_power_of_2(from_tex.height);
    WGPUFilterMode mipmapFilter = (pow2 && !is_cubemap) ? WGPUFilterMode_Linear
                                                        : WGPUFilterMode_Nearest;
    WGPUAddressMode address_mode = options.address_mode;

    WGPUSamplerDescriptor sampler_desc = {
        .addressModeU = address_mode,
        .addressModeV = address_mode,
        .addressModeW = address_mode,
        .magFilter = WGPUFilterMode_Linear,
        .minFilter = WGPUFilterMode_Linear,
        .mipmapFilter = mipmapFilter,
        .lodMinClamp = 0.0f,
        .lodMaxClamp = (float)from_tex.mip_level_count,
        .maxAnisotropy = 1,
    };
    WGPUSampler sampler = wgpuDeviceCreateSampler(device, &sampler_desc);

    return TextureView{
        .size = {from_tex.width, from_tex.height, from_tex.depth},
        .mip_level_count = from_tex.mip_level_count,
        .format = from_tex.format,
        .dimension = from_tex.dimension,
        .texture = from_tex.texture,
        .view = texture_view,
        .sampler = sampler,
    };
}

wgfx::LoadImgResult wgfx::loadImage(const char* filename, bool flip_y)
{
    int width = 0, height = 0;
    int read_comps = 4;
    stbi_set_flip_vertically_on_load(flip_y);
    stbi_uc* data = stbi_load(filename, &width, &height, &read_comps, STBI_rgb_alpha);
    if (nullptr == data)
    {
        SPDLOG_WARN("Image {} failed to load", filename);
    }

    return wgfx::LoadImgResult{
        .width = (u32)width,
        .height = (u32)height,
        .channel_count = 4,
        .data = data,
    };
}
