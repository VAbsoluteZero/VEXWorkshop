#pragma once

#include <VFramework/VEXBase.h>
#include <gfx/GfxUtils.h>

#ifdef VEX_GFX_WEBGPU_DAWN
    #include <webgpu.h>
    #define WGPU_REL(x, a)       \
        if (a)                   \
        {                        \
            wgpu##x##Release(a); \
            a = nullptr;         \
        }
#else
    #include <wgpu/webgpu.h>
    #include <wgpu/wgpu.h>
    #define WGPU_REL(x, a) \
        if (a)             \
        x##Drop
#endif

#ifdef __EMSCRIPTEN__
    #define EMSCRIPTEN_WGPU = 1
#else
    #define EMSCRIPTEN_WGPU = 0
#endif
/*
 * #fixme
 * No RAII until I figure out API && object relationship (lifetimes) better.
 * To be rewritten
 */
namespace vex
{
    struct TextShaderLib;
}
struct SDL_Window;
namespace wgfx
{
    struct Globals;
    static constexpr bool k_using_emscripten =
#ifdef __EMSCRIPTEN__
        true;
#else
        false;
#endif
    template <typename Fptr, typename Callback, typename... TArgs>
    auto wgpuRequest(Fptr request, Callback&& callback, TArgs... args)
    {
        static auto lambda_decay_to_ptr = [](auto... params)
        {
            auto last = (vex::traits::identityFunc(params), ...);
            static_assert(std::is_same_v<decltype(last), void*>, "expected payload as last asrg");
            auto callback_ptr = reinterpret_cast<std::decay_t<Callback>*>(last);
            (*callback_ptr)(params...);
        };
        request(args..., lambda_decay_to_ptr, reinterpret_cast<void*>(&callback));
    }

    void wgpuWait(std::atomic_bool& flag);
    // microsec_timeout < 0 => no timeout
    void wgpuPollWait(
        const struct GpuContext& context, std::atomic_bool& flag, double microsec_timeout = -1);
    WGPUInstance createInstance(WGPUInstanceDescriptor desc);
    void requestDevice(Globals& globals, WGPUDeviceDescriptor const* descriptor);
    void requestAdapter(Globals& globals, WGPURequestAdapterOptions& options);
    WGPUSurface getWGPUSurface(WGPUInstance instance, SDL_Window* sdl_window);
    void swapchainPresent(WGPUSwapChain swap_chain);


    struct GpuContext
    {
        WGPUDevice device = nullptr;
        WGPUCommandEncoder encoder = nullptr;
        WGPUQueue queue = nullptr;
        WGPURenderPassEncoder render_pass = nullptr;
        WGPUTextureView cur_tex_view = nullptr;

        void release(bool release_view = false)
        {
            if (release_view)
                WGPU_REL(TextureView, cur_tex_view);
            WGPU_REL(RenderPassEncoder, render_pass);
            WGPU_REL(CommandEncoder, encoder);
        };
    };
    struct CompContext
    {
        WGPUDevice device = nullptr;
        WGPUCommandEncoder encoder = nullptr;
        WGPUQueue queue = nullptr;
        WGPUComputePassEncoder comp_pass = nullptr;

        void release(bool release_view = false)
        { 
            WGPU_REL(CommandEncoder, encoder);
        };
    };

    struct Globals
    {
        WGPUInstance instance = nullptr;
        WGPUAdapter adapter = nullptr;
        WGPUDevice device = nullptr;
        WGPUQueue queue = nullptr;
        WGPUSurface surface = nullptr;

        WGPUSwapChain swap_chain = nullptr;
        WGPUTextureFormat main_texture_fmt{};

        //WGPUPipelineLayout debug_layout = nullptr;
        //WGPURenderPipeline debug_pipeline = nullptr;

        auto isValid() const -> bool;
        void release();

        GpuContext asContext() const { return GpuContext{.device = device, .queue = queue}; }
    };

    struct Viewport
    {
        static constexpr WGPUCommandEncoderDescriptor encoder_desc = {
            .nextInChain = nullptr,
            .label = "viewport cmd encoder",
        };
        static inline u32 g_generation = 0;
        const u32 uid = ++g_generation;

        vex::ViewportOptions options;
        WGPUTexture texture = nullptr;
        WGPUTextureView view = nullptr;

        WGPUTexture depth_texture = nullptr;
        WGPUTextureView depth_view = nullptr;
        wgfx::GpuContext context;

        WGPURenderPassColorAttachment color_attachment{
            .resolveTarget = nullptr,
            .loadOp = WGPULoadOp_Clear,
            .storeOp = WGPUStoreOp_Store,
            .clearValue = WGPUColor{0.0f, 0.0f, 0.0f, 1.0},
        };
        WGPURenderPassDepthStencilAttachment depth_attachment{
            .depthLoadOp = WGPULoadOp_Clear,
            .depthStoreOp = WGPUStoreOp_Store,
            .depthClearValue = 1.0f,
            .stencilLoadOp = WGPULoadOp_Clear,
            .stencilStoreOp = WGPUStoreOp_Store,
            .stencilClearValue = 0,
        };

        auto size() -> v2u32& { return options.size; }
        auto isValid() const -> bool { return texture && view; }

        void initialize(WGPUDevice device, const vex::ViewportOptions& args);
        wgfx::GpuContext& setupForDrawing(const Globals& globals);
        void finishAndSubmit()
        {
            WGPUCommandBufferDescriptor ctx_desc_fin{nullptr, "viewport submit"};
            check_(context.render_pass);
            wgpuRenderPassEncoderEnd(context.render_pass);
            auto cmd_buf = wgpuCommandEncoderFinish(context.encoder, &ctx_desc_fin);
            wgpuQueueSubmit(context.queue, 1, &cmd_buf);
            WGPU_REL(CommandBuffer, cmd_buf);
        }

        void release()
        {
            WGPU_REL(Texture, texture);
            WGPU_REL(TextureView, view);
            WGPU_REL(Texture, depth_texture);
            WGPU_REL(TextureView, depth_view);
            context.release();
        }
    };

    struct GpuBuffer
    {
        struct Desc
        {
            const char* label;
            WGPUBufferUsageFlags usage{WGPUBufferUsage_CopySrc};
            u32 size = 0;
            u32 count = 0;
        };
        struct CpyArgs
        {
            WGPUDevice device = nullptr;
            WGPUCommandEncoder encoder = nullptr;
            u32 at_offset = 0;
            u8* data = nullptr;
            u32 data_len = 0;
        };

        WGPUBuffer buffer = nullptr;
        Desc desc;

        inline bool isValid() const { return buffer != nullptr; }

        static GpuBuffer create(WGPUDevice device, GpuBuffer::Desc desc);
        static GpuBuffer create(
            WGPUDevice device, GpuBuffer::Desc desc, u8* data, u32 data_len_bytes);
        void copyViaStaging(CpyArgs args);
        void release() { WGPU_REL(Buffer, buffer); }
    };

    template <typename CTX, typename UBO>
    static void updateUniform(
        const CTX& context, GpuBuffer& unibuf, UBO ubo_transforms)
    {
        wgpuQueueWriteBuffer(context.queue, unibuf.buffer, 0, (u8*)&ubo_transforms, sizeof(UBO));
    }

    struct CopyBuffToTexArgs
    {
        WGPUDevice device;
        WGPUExtent3D texture_size;
        WGPUImageCopyBuffer* source_view;
        WGPUImageCopyTexture* dest_view;
    };
    WGPUCommandBuffer copyBufferToTexture(wgfx::CopyBuffToTexArgs args);

    struct LoadImgResult
    {
        u32 width = 0;
        u32 height = 0;
        u32 channel_count = 0;
        u8* data = 0;
        void release() { free(data); }
    };
    LoadImgResult loadImage(const char* filename, bool flip_y = false);
    struct Texture
    {
        WGPUTexture texture;
        u32 width;
        u32 height;
        u32 depth;
        u32 mip_level_count;
        WGPUTextureFormat format;
        WGPUTextureDimension dimension;

        bool isValid() const { return texture != nullptr && (width * height > 0); }

        struct LoadArgs
        {
            const char* label = nullptr;
            WGPUTextureUsageFlags usage = (WGPUTextureUsage_CopyDst |
                                           WGPUTextureUsage_TextureBinding);
            WGPUTextureFormat format = WGPUTextureFormat_RGBA8Unorm;
            bool flip_y = false;
        };

        static Texture loadFormData(
            const GpuContext& ctx, LoadImgResult& data, LoadArgs options);
        static Texture loadFormFile(
            const GpuContext& ctx, const char* filename, LoadArgs options);
        static void copyImageToTexture(const GpuContext& ctx, WGPUTexture texture, void* pixels,
            WGPUExtent3D size, u32 channels);
    };

    inline constexpr bool is_power_of_2(int n) { return (n & (n - 1)) == 0; }
    struct TextureView
    {
        v3u32 size;
        u32 mip_level_count = 0;
        WGPUTextureFormat format{WGPUTextureFormat_RGBA8UnormSrgb};
        WGPUTextureDimension dimension{WGPUTextureDimension_2D};
        WGPUTexture texture;
        WGPUTextureView view;
        WGPUSampler sampler;

        bool isValid() const { return texture && view && sampler; }
        void release()
        {
            WGPU_REL(Sampler, sampler);
            WGPU_REL(TextureView, view);
            WGPU_REL(Texture, texture);
        }
        typedef struct CreationArgs
        {
            WGPUAddressMode address_mode{WGPUAddressMode_ClampToEdge};
        } wgpu_texture_load_options;

        static TextureView create(WGPUDevice device, const Texture& from_tex, CreationArgs options);
    };
    WGPUShaderModule shaderFromSrc(WGPUDevice device, const char* src);

    WGPUShaderModule reloadShader(
        vex::TextShaderLib& shader_lib, const GpuContext& context, const char* shader_name);

    enum class ShaderOrigin : u32
    {
        Source,
        ModuleProvided,
        Undefined
    };
    struct VertShaderDesc
    {
        ShaderOrigin from = ShaderOrigin::Source;
        const char* label = nullptr;
        const char* entry = "vs_main";
        const char* source = nullptr;
        WGPUShaderModule module = nullptr;
        u32 constant_count = 0;
        u32 buffer_count = 1;
        const WGPUConstantEntry* constants = nullptr;
        const WGPUVertexBufferLayout* buffers = nullptr;
    };
    WGPUVertexState makeVertexState(WGPUDevice device, const VertShaderDesc& desc);

    struct FragShaderDesc
    {
        ShaderOrigin from = ShaderOrigin::Source;
        const char* label = nullptr;
        const char* entry = "fs_main";
        const char* source = nullptr;
        WGPUShaderModule module = nullptr;
        u32 constant_count = 0;
        const WGPUConstantEntry* constants = nullptr;
        u32 target_count = 0;
        const WGPUColorTargetState* targets = nullptr;
    };
    WGPUFragmentState makeFragmentState(WGPUDevice device, const FragShaderDesc& desc);


    // struct PipelineData
    //{
    //     WGPUVertexState vert_state;
    //     WGPUFragmentState frag_state;
    //     WGPURenderPipelineDescriptor pipeline_desc = {
    //         .vertex = vert_state,
    //         .fragment = &frag_state,
    //     };
    //     WGPUPipelineLayout pipeline_layout = nullptr;
    //     WGPURenderPipeline pipeline;

    //    void release()
    //    {
    //        WGPU_REL(PipelineLayout, pipeline_layout);
    //        WGPU_REL(RenderPipeline, pipeline);
    //    }
    //};
} // namespace wgfx

namespace wgfx
{
    template <typename T>
    struct Def;
    template <>
    struct Def<WGPUColorTargetState>
    {
        static constexpr WGPUBlendState bl_state{
            .color =
                {
                    .operation = WGPUBlendOperation_Add,
                    .srcFactor = WGPUBlendFactor_SrcAlpha,
                    .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
                },
            .alpha =
                {
                    .operation = WGPUBlendOperation_Add,
                    .srcFactor = WGPUBlendFactor_Zero,
                    .dstFactor = WGPUBlendFactor_One,
                },
        };
        // naming follows corresponding WGPU type:
        const WGPUChainedStruct* nextInChain = nullptr;
        WGPUTextureFormat format = WGPUTextureFormat_RGBA8Unorm;
        const WGPUBlendState* blend = &bl_state;
        WGPUColorWriteMaskFlags writeMask = WGPUColorWriteMask_All;

        FORCE_INLINE constexpr WGPUColorTargetState toWgpu() const
        {
            return WGPUColorTargetState{
                .nextInChain = nextInChain,
                .format = format,
                .blend = blend,
                .writeMask = writeMask,
            };
        };
        constexpr operator WGPUColorTargetState() const { return toWgpu(); };
    };
    template <>
    struct Def<WGPUDepthStencilState>
    {
        static constexpr WGPUStencilFaceState face_state = {
            .compare = WGPUCompareFunction_Always,
            .failOp = WGPUStencilOperation_Keep,
            .depthFailOp = WGPUStencilOperation_Keep,
            .passOp = WGPUStencilOperation_Keep,
        };
        const WGPUChainedStruct* nextInChain = nullptr;
        WGPUTextureFormat format = WGPUTextureFormat_Depth24PlusStencil8;
        bool depthWriteEnabled = true;
        WGPUCompareFunction depthCompare = WGPUCompareFunction_LessEqual;
        WGPUStencilFaceState stencilFront = face_state;
        WGPUStencilFaceState stencilBack = face_state;
        u32 stencilReadMask = 0xFFFFFFFF;
        u32 stencilWriteMask = 0xFFFFFFFF;
        i32 depthBias = 0;
        float depthBiasSlopeScale = 0.0f;
        float depthBiasClamp = 0.0f;

        FORCE_INLINE constexpr WGPUDepthStencilState toWgpu() const
        {
            return WGPUDepthStencilState{

                .format = format,
                .depthWriteEnabled = depthWriteEnabled,
                .depthCompare = depthCompare,
                .stencilFront = stencilFront,
                .stencilBack = stencilBack,
                .stencilReadMask = stencilReadMask,
                .stencilWriteMask = stencilWriteMask,
                .depthBias = 0,
                .depthBiasSlopeScale = 0.0f,
                .depthBiasClamp = 0.0f,
            };
        };
        constexpr operator WGPUDepthStencilState() const { return toWgpu(); };
    };
    template <>
    struct Def<WGPUPrimitiveState>
    {
        WGPUPrimitiveTopology topology = WGPUPrimitiveTopology_TriangleList;
        WGPUFrontFace frontFace = WGPUFrontFace_CCW;
        WGPUIndexFormat stripIndexFormat = WGPUIndexFormat_Undefined;
        WGPUCullMode cullMode = WGPUCullMode_None;
        FORCE_INLINE constexpr auto toWgpu() const
        {
            return WGPUPrimitiveState{
                .topology = topology,
                .stripIndexFormat = stripIndexFormat,
                .frontFace = frontFace,
                .cullMode = cullMode,
            };
        };
        constexpr operator WGPUPrimitiveState() const { return toWgpu(); };
    };
    template <>
    struct Def<WGPUMultisampleState>
    {
        u32 count = 1;
        u32 mask = 0xFFFFFFFF;
        bool alphaToCoverageEnabled = false;
        FORCE_INLINE constexpr auto toWgpu() const
        {
            return WGPUMultisampleState{
                .count = count,
                .mask = mask,
                .alphaToCoverageEnabled = alphaToCoverageEnabled,
            };
        };
        constexpr operator WGPUMultisampleState() const { return toWgpu(); };
    };
    // creates WGPU type T that is inititalized with sensible defaults
    template <typename T>
    FORCE_INLINE constexpr auto makeDefault(Def<T> var = {})
    {
        return var.toWgpu();
    };
    static constexpr auto def_primitive_state = makeDefault<WGPUPrimitiveState>();
    static constexpr auto def_color_target_state = makeDefault<WGPUColorTargetState>({
        .format = WGPUTextureFormat_RGBA8Unorm,
    });
    static constexpr auto def_depth_stencil_state24p8 = makeDefault<WGPUDepthStencilState>();
} // namespace wgfx
