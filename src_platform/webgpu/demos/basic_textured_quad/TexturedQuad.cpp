#include "TexturedQuad.h"

#include <VCore/Containers/Tuple.h>
#include <application/Environment.h>
#include <imgui/imgui_internal.h>
#include <utils/CLI.h>
#include <utils/ImGuiUtils.h>
#include <webgpu/render/LayoutManagement.h>

#include <glm/gtx/hash.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/norm.hpp>
#ifdef VEX_GFX_WEBGPU_DAWN
    #include <tint/tint.h>
#endif
/*
 * NOTE:
 * this whole thing is pretty rough, it is first working thing in WEBGPU
 * so it is very messy as I am figuring out API & what I can do with it.
 *
 */

constexpr const char* console_name = "Console##quad";
const char* g_demo_name_readable = "Textured Quad Demo";

using namespace vex;
using namespace vex::quad_demo;
using namespace wgfx;
using namespace std::literals::string_view_literals;

static inline WGPUCommandEncoderDescriptor ctx_desc{nullptr, "TexturedQuadDemo cmd descriptor"};
static inline WGPUCommandBufferDescriptor ctx_desc_fin{nullptr, "TexturedQuadDemo cmd buf finish"};


struct UBOCamera
{
    mtx4 projection = mtx4_identity;
    mtx4 model_view = mtx4_identity;
    mtx4 view_pos = mtx4_identity;
};

namespace pl_init_data
{
    constexpr auto primitive_state = makeDefault<WGPUPrimitiveState>();
    constexpr auto color_target_state = makeDefault<WGPUColorTargetState>({
        .format = WGPUTextureFormat_RGBA8Unorm,
    });
    constexpr auto depth_stencil_state = makeDefault<WGPUDepthStencilState>();
    constexpr const WGPUVertexBufferLayout vbl = VertLayout<vex::PosNormUv>::buffer_layout;
    constexpr WGPUMultisampleState multisample_state = makeDefault<WGPUMultisampleState>();

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
        .fragment = &frag_state,
    };
} // namespace pl_init_data

static vex::Tuple<WGPUBindGroupLayout, WGPUPipelineLayout> createPipelineLayout(
    const GpuContext& context, u32 unibuf_size)
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
        },
        //  Sampler (Fragment shader)
        {
            .binding = 2,
            .visibility = WGPUShaderStage_Fragment,
            .sampler = {.type = WGPUSamplerBindingType_Filtering},
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

static WGPUBindGroup createBindGroups(const GpuContext& context,
    WGPUBindGroupLayout bind_group_layout, const GpuBuffer& unibuf, const TextureView& tex_view)
{
    WGPUBindGroup out_bind_group = BGBuilder{} //
                                       .add(unibuf)
                                       .add(tex_view.view)
                                       .add(tex_view.sampler)
                                       .createBindGroup(context.device, bind_group_layout);

    check_(out_bind_group);
    return out_bind_group;
}

static WGPURenderPipeline createPipeline(
    const TextShaderLib& shader_lib, const GpuContext& context, WGPUPipelineLayout pipeline_layout)
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
    TextShaderLib shader_lib, const GpuContext& context, WGPUPipelineLayout pipeline_layout)
{
    shader_lib.reload();
#ifdef VEX_GFX_WEBGPU_DAWN
    #ifndef NDEBUG
    std::string f = "content/shaders/wgsl/basic_unlit.wgsl";
    std::string cont = shader_lib.shad_src.find("content/shaders/wgsl/basic_unlit.wgsl")->text;
    auto source = std::make_unique<tint::Source::File>(f, cont);
    auto reparsed_program = tint::reader::wgsl::Parse(source.get());
    if (!reparsed_program.IsValid())
    {
        for (auto& it : reparsed_program.Diagnostics())
        {
            SPDLOG_INFO("tint error: {}", it.message);
        }
        return nullptr;
    }
    #endif
#endif
    using namespace pl_init_data;

    auto* src = shader_lib.shad_src.find("content/shaders/wgsl/basic_unlit.wgsl"sv);
    if (!check(src, "shader not found"))
        return {};
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
            success = (status == WGPUCompilationInfoRequestStatus_Success);
            sync_ready = true;
        },
        shad_vert_frag);
    wgpuPollWait(context, sync_ready, 1000);
    if (!success)
        return nullptr;

    vert_state.module = shad_vert_frag;
    frag_state.module = shad_vert_frag;
    pipeline_desc.vertex = vert_state;
    pipeline_desc.layout = pipeline_layout;

    auto pipeline = wgpuDeviceCreateRenderPipeline(context.device, &pipeline_desc);
    return pipeline;
}

void TexturedQuadDemo::init(Application& owner, InitArgs args)
{
    AGraphicsBackend* backend = owner.getGraphicsBackend();
    checkLethal(backend->id == GfxBackendID::Webgpu, "unsupported gfx backend");
    this->wgpu_backend = static_cast<WebGpuBackend*>(backend);
    auto& globals = wgpu_backend->getGlobalResources();

    vex::InlineBufferAllocator<32 * 1024> temp_alloc_resource;
    auto tmp_alloc = temp_alloc_resource.makeAllocatorHandle();
    GpuContext ctx = {
        .device = globals.device,
        .encoder = wgpuDeviceCreateCommandEncoder(globals.device, &ctx_desc),
        .queue = globals.queue,
    };
    ViewportOptions options{.name = g_demo_name_readable, .size = {800, 600}};
    viewports.add(ctx, options);
    scene.camera = BasicCamera::makeOrtho(
        {0.f, 0.f, -2.0}, {default_cam_height * 1.333f, default_cam_height}, -10, 10);

    ui.console_wnd.name = console_name;
    // init wgpu state
    {
        // quad
        bg_quad = [&]() -> QuadGpuData
        {
            auto tmp_mesh = makeQuadUV(tmp_alloc, 4, 4);

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
                    .size = tmp_mesh.vtxBufSize(),
                    .count = (u32)tmp_mesh.vertices.size(),
                },
                (u8*)tmp_mesh.vertices.data(), tmp_mesh.vtxBufSize());

            auto ind_buf = GpuBuffer::create(ctx.device,
                {
                    .label = "bg quad idx buf",
                    .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index,
                    .size = tmp_mesh.idxBufSize(),
                    .count = (u32)tmp_mesh.indices.size(),
                },
                (u8*)tmp_mesh.indices.data(), tmp_mesh.idxBufSize());

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
        WGPU_REL(CommandEncoder, ctx.encoder);
        temp_alloc_resource.reset();
    } // end quad pipeline init
    // init temp geometry stuff
    {
        using namespace pl_init_data;
        auto* src = wgpu_backend->text_shad_lib.shad_src.find(
            "content/shaders/wgsl/gizmo_color.wgsl"sv);
        if (!check(src, "shader not found"))
            return;
        WGPUShaderModule shad = shaderFromSrc(globals.device, src->text.c_str());

        temp_geom.uniform_buf = [&]() -> GpuBuffer
        {
            UBOMvp ubo_transforms;
            ubo_transforms.model_view_proj = scene.camera.mtx.projection * scene.camera.mtx.view;
            auto out = GpuBuffer::create(ctx.device,
                {
                    .label = "bg quad idx buf",
                    .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
                    .size = sizeof(UBOMvp),
                },
                (u8*)&ubo_transforms, sizeof(UBOMvp));
            return out;
        }();
        temp_geom.vtx_buf = GpuBuffer::create(
            ctx.device, {
                            .label = "bg quad ver buf",
                            .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex,
                            .size = temp_geom.max_vtx_buff_size,
                        });

        temp_geom.idx_buf = GpuBuffer::create(
            ctx.device, {
                            .label = "bg quad idx buf",
                            .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index,
                            .size = temp_geom.max_idx_buff_size,
                        });

        auto [layout, binding] = BGLCombinedBuilder{.al = tmp_alloc} //
                                     .addUniform(sizeof(UBOMvp), temp_geom.uniform_buf)
                                     .createLayoutAndGroup(ctx.device);

        SimplePipeline<PosNormColor> pl_builder;
        pl_builder.setShader(shad);
        temp_geom.pipeline = pl_builder.createPipeline(ctx, layout);
        temp_geom.bind_group = binding;
        check(temp_geom.isValid(), "temp_geometry initialization failed");
    }
    // register input
    {
        using namespace input;
        owner.input.addTrigger("ClearPoints"_trig,
            Trigger{
                .fn_logic =
                    [](Trigger& self, const InputState& state)
                {
                    const auto code = (u8)(SignalId::KeySpace);
                    return state.this_frame[code].state == SignalState::Started;
                },
            },
            true);
        owner.input.addTrigger("LeftMBKDownWithCtrl"_trig,
            Trigger{
                .fn_logic =
                    [](Trigger& self, const InputState& state)
                {
                    const auto code = (u8)(SignalId::MouseLBK);
                    const auto code2 = (u8)(SignalId::KeyModCtrl);
                    if (state.this_frame[code].ia_startedOrGoing() &&
                        state.this_frame[code2].ia_startedOrGoing())
                    {
                        return true;
                    }
                    return false;
                },
            },
            true);
    }
}

void TexturedQuadDemo::update(Application& owner)
{
    if (viewports.imgui_views.size() < 1)
        return;
    auto& options = owner.getSettings();
    auto opt_dict = options.settings;
    if (auto entry = opt_dict.find("gfx.pause"); entry && entry->value.getValueOr<bool>(false))
        return;
    if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
        return;

    auto& globals = wgpu_backend->getGlobalResources();
    DynMeshBuilder<PosNormColor> mesh_data = {alloc, 512, 256};
    auto& main_view = viewports.imgui_views[0];

    const v2i32 pos = owner.input.global.mouse_pos_window;
    const v2i32 delta = owner.input.global.mouse_delta;
    const v2f pos_cam_space = main_view.viewportToNormalizedView(pos);
    // input update
    if (viewport_was_active && glm::abs(pos_cam_space).x < 1 && glm::abs(pos_cam_space).y < 1)
    {
        using namespace input;
        const v3f world_loc = scene.camera.normalizedViewToWorld(pos_cam_space);
        v3f world_delta = scene.camera.viewportToCamera(
            main_view.pixelScaleToNormalizedView(delta));

        owner.input.ifTriggered("ClearPoints"_trig,
            [&](const input::Trigger& self)
            {
                mouse_points.points.clear();
                scene.camera.pos.x = 0;
                scene.camera.pos.y = 0;
                return true;
            });
        owner.input.ifTriggered("MouseRightDown"_trig,
            [&](const input::Trigger& self)
            {
                // SPDLOG_INFO(" clicked at {} => {}, adding point", pos_cam_space, world_loc);
                mouse_points.points.push(Point{world_loc, mouse_points.color});
                return true;
            });
        owner.input.ifTriggered("MouseMidMove"_trig,
            [&](const input::Trigger& self)
            {
                world_delta.z = 0;
                scene.camera.pos = scene.camera.pos + world_delta;
                return true;
            });
        owner.input.ifTriggered("LeftMBKDownWithCtrl"_trig,
            [&](const input::Trigger& self)
            {
                // SPDLOG_INFO(" down at {} => {}, adding point", pos_cam_space, world_loc);
                if (mouse_points.points.size() == 0)
                {
                    mouse_points.points.push(Point{world_loc, mouse_points.color});
                }
                else if (glm::length2(world_loc - mouse_points.points.peek()->pos) >= (0.1f * 0.1f))
                {
                    mouse_points.points.push(Point{world_loc, mouse_points.color});
                }
                return true;
            });
    }
    // wgpu update
    {
        Viewport& viewport = main_view.render_target;
        auto& wgpu_ctx = viewport.setupForDrawing(globals);
        {
            scene.camera.aspect = viewport.options.size.x / (float)viewport.options.size.y;
            scene.camera.update();

            UBOCamera ubo_transforms;
            ubo_transforms.model_view = scene.camera.mtx.view;
            ubo_transforms.projection = scene.camera.mtx.projection;
            updateUniform(wgpu_ctx, gpu_data.uniform_buf, ubo_transforms);

            UBOMvp ubo_temp_geo;
            ubo_temp_geo.model_view_proj = scene.camera.mtx.projection * scene.camera.mtx.view;
            updateUniform(wgpu_ctx, temp_geom.uniform_buf, ubo_temp_geo);
        }
        // create temp geometry
        {
            const float geom_scale = 1.5f;
            vex::Buffer<v3f> lines{alloc, MouseDraw::max_points};
            vex::Buffer<v4f> colors{alloc, MouseDraw::max_points};
            for (auto& p : mouse_points.points.reverseEnum())
            {
                lines.add(p.pos);
            }
            vex::PolyLine::buildPolyXY(mesh_data, //
                PolyLine::Args{
                    .points = lines.data(),
                    .len = lines.size(),
                    .color = mouse_points.color,
                    .corner_type = PolyLine::Corners::CloseTri,
                    .thickness = mouse_points.width,
                    .closed = mouse_points.loop,
                });

            auto& verts = mesh_data.vertices;
            auto& indices = mesh_data.indices;

            wgpuQueueWriteBuffer(wgpu_ctx.queue, temp_geom.vtx_buf.buffer, 0, (u8*)verts.data(),
                mesh_data.vtxBufSize());
            wgpuQueueWriteBuffer(wgpu_ctx.queue, temp_geom.idx_buf.buffer, 0, (u8*)indices.data(),
                mesh_data.idxBufSize());
        }

        wgpuDeviceTick(wgpu_ctx.device);

        auto cmd_buf = [&]() -> WGPUCommandBuffer
        {
            auto rpass_enc = wgpu_ctx.render_pass;
            // quad
            {
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
            }
            // temp geometry
            {
                auto& indices = mesh_data.indices;

                wgpuRenderPassEncoderPushDebugGroup(rpass_enc, "temp geometry setup");
                wgpuRenderPassEncoderSetPipeline(rpass_enc, temp_geom.pipeline);
                wgpuRenderPassEncoderSetBindGroup(rpass_enc, 0, temp_geom.bind_group, 0, 0);

                wgpuRenderPassEncoderSetVertexBuffer(
                    rpass_enc, 0, temp_geom.vtx_buf.buffer, 0, mesh_data.vtxBufSize());
                wgpuRenderPassEncoderSetIndexBuffer(rpass_enc, temp_geom.idx_buf.buffer,
                    WGPUIndexFormat_Uint32, 0, mesh_data.idxBufSize());
                wgpuRenderPassEncoderPopDebugGroup(rpass_enc);

                wgpuRenderPassEncoderPushDebugGroup(rpass_enc, "lines");
                wgpuRenderPassEncoderDrawIndexed(rpass_enc, indices.size(), 1, 0, 0, 0);
                wgpuRenderPassEncoderPopDebugGroup(rpass_enc);
            }

            wgpuRenderPassEncoderEnd(rpass_enc);

            return wgpuCommandEncoderFinish(wgpu_ctx.encoder, &ctx_desc_fin);
        }();
        wgpuQueueSubmit(wgpu_ctx.queue, 1, &cmd_buf);
        WGPU_REL(CommandBuffer, cmd_buf);

        wgpuDeviceTick(wgpu_ctx.device);
    }
}

void TexturedQuadDemo::drawUI(Application& owner)
{
    ui.drawStandardUI(owner, &viewports);
    viewport_was_active = ImGui::Begin(g_demo_name_readable);
    defer_ { ImGui::End(); };
    if (viewport_was_active)
    {
        if (ImGui::Button("reload shaders"))
        {
            auto& globals = wgpu_backend->getGlobalResources();

            GpuContext ctx = {
                .device = globals.device,
                .encoder = nullptr,
                .queue = globals.queue,
            };
            auto new_pipeline = realoadShaders(
                wgpu_backend->text_shad_lib, ctx, gpu_data.pipe_layout);
            if (new_pipeline)
            {
                WGPU_REL(RenderPipeline, gpu_data.pipeline);
                gpu_data.pipeline = new_pipeline;
                SPDLOG_INFO("New webgpu pipeline created.");
            }
            else
            {
                SPDLOG_INFO("Failed to create webgpu pipeline.");
            }
        }
        if (ImGui::TreeNode("Options"))
        {
            ImGui::PushFont(g_view_hub.visuals.fnt_accent);
            defer_ { ImGui::PopFont(); };
            ImGui::Checkbox("  Closed loop", &mouse_points.loop);
            ImGui::SameLine();
            ImGui::Bullet();
            ImGui::SameLine();
            ImGui::SetNextItemWidth(64);
            ImGui::SliderFloat("Line width", &mouse_points.width, 0.1f, 2.f);
            ImGui::Text("  Press [right mouse button] to add points ot temp geometry. (%d/ %d)",
                mouse_points.points.size(), mouse_points.max_points);
            ImGui::Text("  Hold [left mouse button]+[ctrl] to draw freely.");
            ImGui::Text("  Hold [mid mouse button] to move camera.");
            ImGui::Text("  Press [space] to clear dynamic geometry & reset camera.");
            ImGui::TreePop();
        }
    }
    {
        ImGui::Begin("Color picker");

        ImGui::SetNextItemWidth(256);
        ImGui::ColorPicker3("Color", (float*)&mouse_points.color.x);
        ImGui::End();
    }
}

void TexturedQuadDemo::postFrame(Application& owner)
{
    viewports.postFrame();
    frame_alloc.reset();
}
