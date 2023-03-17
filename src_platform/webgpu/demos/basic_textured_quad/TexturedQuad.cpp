#include "TexturedQuad.h"

#include <VCore/Containers/Tuple.h>
#include <utils/CLI.h>

#include <glm/gtx/hash.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#ifdef VEX_GFX_WEBGPU_DAWN
    #include <tint/tint.h>
#endif

using namespace vex;
using namespace wgfx;
using namespace std::literals::string_view_literals;

static inline WGPUCommandEncoderDescriptor ctx_desc{nullptr, "TexturedQuadDemo cmd descriptor"};
static inline WGPUCommandBufferDescriptor ctx_desc_fin{nullptr, "TexturedQuadDemo cmd buf finish"};
struct UBOCamera
{
    mtx4 projection;
    mtx4 model_view;
    mtx4 view_pos;
};

static vex::Tuple<WGPUBindGroupLayout, WGPUPipelineLayout> createPipelineLayout(
    const Context& context, u32 unibuf_size)
{
    // Bind group layout
    WGPUBindGroupLayoutEntry bgl_entries[3] = {
        {
            .binding = 0,
            .visibility = WGPUShaderStage_Vertex,
            .buffer =
                {
                    .type = WGPUBufferBindingType_Uniform,
                    .hasDynamicOffset = false,
                    .minBindingSize = unibuf_size,
                },
            .sampler = {},
        },
        // Texture view (Fragment shader)
        {
            .binding = 1,
            .visibility = WGPUShaderStage_Fragment,
            .texture =
                {
                    .sampleType = WGPUTextureSampleType_Float,
                    .viewDimension = WGPUTextureViewDimension_2D,
                    .multisampled = false,
                },
            .storageTexture = {},
        },
        //  Sampler (Fragment shader)
        {
            .binding = 2,
            .visibility = WGPUShaderStage_Fragment,
            .sampler = {.type = WGPUSamplerBindingType_Filtering},
            .texture = {},
        },
    };

    WGPUBindGroupLayoutDescriptor bgl_desc{
        .entryCount = (u32)std::size(bgl_entries),
        .entries = bgl_entries,
    };

    auto bgl = wgpuDeviceCreateBindGroupLayout(context.device, &bgl_desc);
    check_(bgl);
    WGPUPipelineLayoutDescriptor pl_desc{
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = &bgl,
    };
    auto pl = wgpuDeviceCreatePipelineLayout(context.device, &pl_desc);
    check_(pl);
    return {bgl, pl};
}

static WGPUBindGroup createBindGroups(const Context& context, WGPUBindGroupLayout bind_group_layout,
    const GpuBuffer& unibuf, const TextureView& tex_view)
{
    WGPUBindGroupEntry bg_entries[3] = {
        {
            .binding = 0,
            .buffer = unibuf.buffer,
            .offset = 0,
            .size = unibuf.desc.size,
        },
        {
            .binding = 1,
            .textureView = tex_view.view,
        },
        {
            .binding = 2,
            .sampler = tex_view.sampler,
        },
    };
    WGPUBindGroupDescriptor bg_desc{
        .layout = bind_group_layout,
        .entryCount = (u32)std::size(bg_entries),
        .entries = bg_entries,
    };
    auto out_bind_group = wgpuDeviceCreateBindGroup(context.device, &bg_desc);
    check_(out_bind_group);
    return out_bind_group;
}

namespace pl_init_data
{
    constexpr auto primitive_state = make<WGPUPrimitiveState>();
    constexpr auto color_target_state = make<WGPUColorTargetState>({
        .format = WGPUTextureFormat_RGBA8Unorm,
    });
    constexpr auto depth_stencil_state = make<WGPUDepthStencilState>();
    constexpr const WGPUVertexBufferLayout vbl = VertLayout<vex::PosNormUv>::buffer_layout;
    constexpr WGPUMultisampleState multisample_state = make<WGPUMultisampleState>();

    auto vert_state = makeVertexState(nullptr, //
        VertShaderDesc{
            .from = ShaderOrigin::Undefined,
            .label = "quad shader plain",
            .buffer_count = 1,
            .buffers = &vbl,
        });
    auto frag_state = makeFragmentState(nullptr, //
        FragShaderDesc{.from = ShaderOrigin::Undefined,
            .label = "quad shader plain",
            .target_count = 1,
            .targets = &color_target_state});
    WGPURenderPipelineDescriptor pipeline_desc = {
        .label = "quad render pipeline",
        .vertex = vert_state,
        .primitive = primitive_state,
        .depthStencil = &depth_stencil_state,
        .multisample = multisample_state,
        .fragment = &frag_state, // ??? why ref (webgpu weirdness)
    };
} // namespace pl_init_data

static WGPURenderPipeline createPipeline(
    const TextShaderLib& shader_lib, const Context& context, WGPUPipelineLayout pipeline_layout)
{
    using namespace pl_init_data;
    auto* src = shader_lib.shad_src.find("content/shaders/wgsl/basic_unlit.wgsl"sv);
    if (!check(src, "shader not found"))
        return {};
    WGPUShaderModule shad_vert_frag = shaderFromSrc(context.device, src->text.c_str());
    vert_state.module = shad_vert_frag;
    frag_state.module = shad_vert_frag;
    pipeline_desc.vertex = vert_state;
    pipeline_desc.layout = pipeline_layout;

    auto pipeline = wgpuDeviceCreateRenderPipeline(context.device, &pipeline_desc);
    return pipeline;
}
// tmp hack, make it proper hot reload #fixme
static WGPURenderPipeline realoadShaders(
    TextShaderLib shader_lib, const Context& context, WGPUPipelineLayout pipeline_layout)
{
    shader_lib.reload();
#ifdef VEX_GFX_WEBGPU_DAWN
    std::string f = "content/shaders/wgsl/basic_unlit.wgsl";
    std::string cont = shader_lib.shad_src.find("content/shaders/wgsl/basic_unlit.wgsl")->text;
    auto source = std::make_unique<tint::Source::File>(f, cont);
    auto reparsed_program = tint::reader::wgsl::Parse(source.get());
    if (!reparsed_program.IsValid())
    {
        auto diag_printer = tint::diag::Printer::create(stderr, true);
        for (auto& it : reparsed_program.Diagnostics())
        {
            SPDLOG_INFO("tint error: {}", it.message);
        }
        tint::diag::Formatter diag_formatter;
        diag_formatter.format(reparsed_program.Diagnostics(), diag_printer.get());
        return nullptr;
    }
#endif
    using namespace pl_init_data;

    auto* src = shader_lib.shad_src.find("content/shaders/wgsl/basic_unlit.wgsl"sv);
    if (!check(src, "shader not found"))
        return {};
    WGPUShaderModule shad_vert_frag = shaderFromSrc(context.device, src->text.c_str());
    vert_state.module = shad_vert_frag;
    frag_state.module = shad_vert_frag;
    pipeline_desc.vertex = vert_state;
    pipeline_desc.layout = pipeline_layout;

    auto pipeline = wgpuDeviceCreateRenderPipeline(context.device, &pipeline_desc);
    return pipeline;
}

static void updateUniform(wgfx::Context& context, GpuBuffer& unibuf, UBOCamera ubo_transforms)
{
    wgpuQueueWriteBuffer(context.queue, unibuf.buffer, 0, (u8*)&ubo_transforms, sizeof(UBOCamera));
}

void TexturedQuadDemo::init(Application& owner, InitArgs args)
{
    AGraphicsBackend* backend = owner.getGraphicsBackend();
    checkLethal(backend->id == GfxBackendID::Webgpu, "unsupported gfx backend");
    this->wgpu_backend = static_cast<WebGpuBackend*>(backend);
    auto& globals = wgpu_backend->getGlobalResources();
    // init wgpu state
    {
        auto st = make<WGPUColorTargetState>({
            .writeMask = WGPUColorWriteMask_All,
        });

        Context ctx = {
            .device = globals.device,
            .encoder = wgpuDeviceCreateCommandEncoder(globals.device, &ctx_desc),
            .queue = globals.queue,
        };
        ViewportOptions options{.name = "Textured Quad Demo", .size = {800, 600}};
        viewports.add(ctx, options);

        scene.camera = BasicCamera::makeOrtho({0.f, 0.f, -2.0}, {2, 2}, -10, 10);

        vex::InlineBufferAllocator<32 * 1024> temp_alloc_resource;
        auto tmp_alloc = temp_alloc_resource.makeAllocatorHandle();

        // quad
        bg_quad = [&]() -> QuadGpuData
        {
            auto tmp_mesh = makeQuad<PosNormUv>(tmp_alloc, 0.4f, 0.4f);

            auto texture = loadImage("content/sprites/quad_demo/gobi.png");
            defer_ { texture.release(); };
            auto bg_tex = Texture::loadFormData(ctx, texture,
                {
                    .usage = (WGPUTextureUsage_CopyDst | WGPUTextureUsage_TextureBinding),
                });
            check_(bg_tex.isValid());

            auto bg_tex_view = TextureView::create(ctx.device, bg_tex, {});

            auto vert_buf = GpuBuffer::create(ctx.device,
                {
                    .label = "bg quad ver buf",
                    .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex,
                    .size = tmp_mesh.vertBufSize(),
                    .count = (u32)tmp_mesh.vertices.size(),
                },
                (u8*)tmp_mesh.vertices.data(), tmp_mesh.vertBufSize());

            auto ind_buf = GpuBuffer::create(ctx.device,
                {
                    .label = "bg quad idx buf",
                    .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index,
                    .size = tmp_mesh.indBufSize(),
                    .count = (u32)tmp_mesh.indices.size(),
                },
                (u8*)tmp_mesh.indices.data(), tmp_mesh.indBufSize());

            check_(ind_buf.isValid());
            check_(vert_buf.isValid());
            check_(bg_tex_view.isValid());
            return QuadGpuData{
                .idx_buf = ind_buf,
                .vtx_buf = vert_buf,
                .tex_view = bg_tex_view,
            };
        }();

        gpu_data.uniform_buf = [&]() -> GpuBuffer
        {
            UBOCamera ubo_transforms;
            ubo_transforms.model_view = scene.camera.mtx.view;
            ubo_transforms.projection = scene.camera.mtx.projection;
            auto out = GpuBuffer::create(ctx.device,
                {
                    .label = "bg quad idx buf",
                    .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
                    .size = sizeof(UBOCamera),
                },
                (u8*)&ubo_transforms, sizeof(UBOCamera));
            return out;
        }();
        check_(gpu_data.uniform_buf.isValid());
        // create layout & bindings
        {
            auto [bind_layout, pipeline_layout] = createPipelineLayout(ctx, sizeof(UBOCamera));
            gpu_data.pipeline = createPipeline(wgpu_backend->text_shad_lib, ctx, pipeline_layout);
            gpu_data.bind_group = createBindGroups(
                ctx, bind_layout, gpu_data.uniform_buf, bg_quad.tex_view);

            gpu_data.bind_group_layout = bind_layout;
            gpu_data.pipe_layout = pipeline_layout;
        }
        wgpuDeviceTick(ctx.device);
        WGPU_REL(wgpuCommandEncoder, ctx.encoder);
    } // end wgpu init
    // register input
    {
        using namespace input;
        owner.input.addTrigger("ReloadShaders"_trig,
            Trigger{
                .fn_logic =
                    [](Trigger& self, const InputState& state)
                {
                    if (state.this_frame[(u8)(SignalId::KeySpace)].state == SignalState::Started)
                    {
                        return true;
                    }

                    return false;
                },
            });
    }
}


void TexturedQuadDemo::update(Application& owner)
{
    auto& globals = wgpu_backend->getGlobalResources();

    vex::InlineBufferAllocator<32 * 1024> temp_alloc_resource;
    auto tmp_alloc = temp_alloc_resource.makeAllocatorHandle();

    // wgpu update
    {
        if (viewports.imgui_views.size() < 1)
            return;

        UBOCamera ubo_transforms;
        ubo_transforms.model_view = scene.camera.mtx.view;
        ubo_transforms.projection = scene.camera.mtx.projection;

        Viewport& viewport = viewports.imgui_views[0].render_target;

        auto& wgpu_ctx = viewport.setupForDrawing(globals);
        updateUniform(wgpu_ctx, gpu_data.uniform_buf, ubo_transforms);

        wgpuDeviceTick(wgpu_ctx.device);

        auto cmd_buf = [&]() -> WGPUCommandBuffer
        {
            auto rpass_enc = wgpu_ctx.render_pass;
            wgpuRenderPassEncoderPushDebugGroup(rpass_enc, "quad1 setup");
            wgpuRenderPassEncoderSetPipeline(rpass_enc, gpu_data.pipeline);
            wgpuRenderPassEncoderSetBindGroup(rpass_enc, 0, gpu_data.bind_group, 0, 0);

            wgpuRenderPassEncoderSetVertexBuffer(
                rpass_enc, 0, bg_quad.vtx_buf.buffer, 0, bg_quad.vtx_buf.desc.size);
            wgpuRenderPassEncoderSetIndexBuffer(rpass_enc, bg_quad.idx_buf.buffer,
                WGPUIndexFormat_Uint32, 0, bg_quad.idx_buf.desc.size);
            wgpuRenderPassEncoderPopDebugGroup(rpass_enc);

            wgpuRenderPassEncoderPushDebugGroup(rpass_enc, "quad1");
            wgpuRenderPassEncoderDrawIndexed(rpass_enc, bg_quad.idx_buf.desc.count, 1, 0, 0, 0);
            wgpuRenderPassEncoderPopDebugGroup(rpass_enc);

            wgpuRenderPassEncoderEnd(rpass_enc);

            return wgpuCommandEncoderFinish(wgpu_ctx.encoder, &ctx_desc_fin);
        }();
        wgpuQueueSubmit(wgpu_ctx.queue, 1, &cmd_buf);
        WGPU_REL(wgpuCommandBuffer, cmd_buf);
        WGPU_REL(wgpuCommandEncoder, wgpu_ctx.encoder);
        wgpu_ctx.encoder = wgpuDeviceCreateCommandEncoder(globals.device, &ctx_desc);

        wgpuDeviceTick(wgpu_ctx.device);
    }
    // input update
    {
        using namespace input;
        owner.input.if_triggered("ReloadShaders"_trig,
            [&](const input::Trigger& self)
            {
                Context ctx = {
                    .device = globals.device,
                    .encoder = nullptr,
                    .queue = globals.queue,
                };
                auto new_pipeline = realoadShaders(
                    wgpu_backend->text_shad_lib, ctx, gpu_data.pipe_layout);
                if (new_pipeline)
                {
                    WGPU_REL(wgpuRenderPipeline, gpu_data.pipeline);
                    gpu_data.pipeline = new_pipeline;
                    SPDLOG_INFO("New webgpu pipeline created.");
                }
                else
                {
                    SPDLOG_INFO("Failed to create webgpu pipeline.");
                }
                return true;
            });
    }
}

void TexturedQuadDemo::drawUI(Application& owner)
{ //
    ui.drawStandardUI(owner, &viewports);
    ImGui::Begin("Textured Quad Demo");
    if (ImGui::Button("reload shaders"))
    {
        auto& globals = wgpu_backend->getGlobalResources();

        Context ctx = {
            .device = globals.device,
            .encoder = nullptr,
            .queue = globals.queue,
        };
        auto new_pipeline = realoadShaders(wgpu_backend->text_shad_lib, ctx, gpu_data.pipe_layout);
        if (new_pipeline)
        {
            WGPU_REL(wgpuRenderPipeline, gpu_data.pipeline);
            gpu_data.pipeline = new_pipeline;
            SPDLOG_INFO("New webgpu pipeline created.");
        }
        else
        {
            SPDLOG_INFO("Failed to create webgpu pipeline.");
        }
    }
    ImGui::End();
}

void vex::TexturedQuadDemo::postFrame(Application& owner)
{
    for (auto& vex : viewports.imgui_views)
    {
        vex.render_target.context.release();
        if (vex.changed_last_frame)
        {
            vex.changed_last_frame = false;
            vex.render_target.release();
            vex.render_target.initialize(vex.render_target.context.device, vex.args);
        }
    }
}
