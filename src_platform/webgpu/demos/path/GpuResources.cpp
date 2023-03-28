#include "GpuResources.h"

#include <spdlog/stopwatch.h>
#include <webgpu/render/LayoutManagement.h>
using namespace vex;
using namespace vex::flow;
using namespace wgfx;
using namespace std::literals::string_view_literals;
using namespace std::literals::chrono_literals;


void ViewportGrid::init(const wgfx::GpuContext& ctx, const TextShaderLib& text_shad_lib)
{
    vex::InlineBufferAllocator<1024> temp_alloc_resource;
    auto tmp_alloc = temp_alloc_resource.makeAllocatorHandle();

    auto* src = text_shad_lib.shad_src.find(grid_shader_file);
    if (!check(src, "shader not found"))
        return;
    WGPUShaderModule shad = shaderFromSrc(ctx.device, src->text.c_str());

    auto tmp_mesh = buildPlane<PosNormUv>(tmp_alloc, 2, {-1.0f, -1.0f}, {1.0f, 1.0f}, 8.0);

    for (auto& vtx : tmp_mesh.vertices)
    {
        vtx.pos.z = -3;
        vtx.norm = {0.3f, 0.9f, 0.2f};
    }

    uniform_buf = [&]() -> GpuBuffer
    {
        UBOMvp4Colors4Floats ubo_transforms;
        auto out = GpuBuffer::create(ctx.device,
            {
                .label = "grid uni buf",
                .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
                .size = sizeof(UBOMvp4Colors4Floats),
            },
            (u8*)&ubo_transforms, sizeof(UBOMvp4Colors4Floats));
        return out;
    }();
    vtx_buf = GpuBuffer::create(ctx.device,
        {
            .label = "grid ver buf",
            .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex,
            .size = tmp_mesh.vtxBufSize(),
        },
        (u8*)tmp_mesh.vertices.data(), tmp_mesh.vtxBufSize());
    idx_buf = GpuBuffer::create(ctx.device,
        {
            .label = "grid idx buf",
            .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index,
            .size = tmp_mesh.idxBufSize(),
            .count = (u32)tmp_mesh.indices.size(),
        },
        (u8*)tmp_mesh.indices.data(), tmp_mesh.idxBufSize());

    auto [layout, binding] = BGLCombinedBuilder{.al = tmp_alloc} //
                                 .addUniform(sizeof(UBOMvp4Colors4Floats), uniform_buf, 0,
                                     WGPUShaderStage_Fragment | WGPUShaderStage_Vertex)
                                 .createLayoutAndGroup(ctx.device);

    bgl_layout = layout;
    pipeline_data.setShader(shad);
    pipeline_data.depth_stencil_state.depthWriteEnabled = false;
    pipeline = pipeline_data.createPipeline(ctx, layout);
    bind_group = binding;
}

void vex::flow::ViewportGrid::draw(const wgfx::GpuContext& ctx, const DrawContext& draw_ctx)
{
    auto grid_thicness_px = draw_ctx.settings->valueOr(
        opt_grid_thickness.key_name, opt_grid_thickness.default_val);
    v4f grid_color = draw_ctx.settings->valueOr(
        opt_grid_color.key_name, opt_grid_color.default_val);

    if (grid_thicness_px < 1 || draw_ctx.grid_half_size < 0.0001f)
        return;

    const float grid_thickness_scaled = grid_thicness_px / (draw_ctx.viewport_size.y * 0.5);
    const float cell_len = (draw_ctx.viewport_size.y * 0.5) / draw_ctx.grid_half_size;
    auto half_num_lines = draw_ctx.grid_half_size;
    auto cell_div_line_width = cell_len / grid_thicness_px;
    if (cell_div_line_width < 2)
    {
        return;
    }
    if (cell_len / grid_thicness_px < 4)
    {
        half_num_lines *= 0.5f;
    }

    UBOMvp4Colors4Floats vbo{
        .model_view_proj = draw_ctx.camera_mvp,
        .data1 = grid_color,
        .data2 = {half_num_lines, grid_thickness_scaled, draw_ctx.pixel_size_nm.x,
            draw_ctx.pixel_size_nm.y},
    };
    updateUniform(ctx, uniform_buf, vbo);

    auto rpass_enc = ctx.render_pass;
    wgpuRenderPassEncoderPushDebugGroup(rpass_enc, "draw grid");
    wgpuRenderPassEncoderSetPipeline(rpass_enc, pipeline);
    wgpuRenderPassEncoderSetBindGroup(rpass_enc, 0, bind_group, 0, 0);

    // #todo look at wgpu examples to see proper way
    wgpuRenderPassEncoderSetVertexBuffer(rpass_enc, 0, vtx_buf.buffer, 0, vtx_buf.desc.size);
    wgpuRenderPassEncoderSetIndexBuffer(
        rpass_enc, idx_buf.buffer, WGPUIndexFormat_Uint32, 0, idx_buf.desc.size);

    wgpuRenderPassEncoderDrawIndexed(rpass_enc, idx_buf.desc.count, 1, 0, 0, 0);
    wgpuRenderPassEncoderPopDebugGroup(rpass_enc);
}

bool ViewportGrid::reloadShaders(TextShaderLib& shader_lib, const GpuContext& context)
{
    WGPUShaderModule shad_vert_frag = reloadShader(shader_lib, context, grid_shader_file);
    if (!shad_vert_frag)
        return false;

    pipeline_data.vert_state.module = shad_vert_frag;
    pipeline_data.frag_state.module = shad_vert_frag;
    pipeline_data.pipeline_desc.vertex = pipeline_data.vert_state;
    pipeline_data.pipeline_desc.fragment = &pipeline_data.frag_state;

    WGPU_REL(RenderPipeline, pipeline);
    pipeline = pipeline_data.createPipeline(context, bgl_layout);
    check_(pipeline);
    return true;
}

void vex::flow::TempGeometry::init(const wgfx::GpuContext& ctx, const TextShaderLib& text_shad_lib)
{
    vex::InlineBufferAllocator<1024> temp_alloc_resource;
    auto tmp_alloc = temp_alloc_resource.makeAllocatorHandle();
    auto* src = text_shad_lib.shad_src.find("content/shaders/wgsl/gizmo_color.wgsl"sv);
    if (!check(src, "shader not found"))
        return;
    WGPUShaderModule shad = shaderFromSrc(ctx.device, src->text.c_str());

    uniform_buf = [&]() -> GpuBuffer
    {
        auto out = GpuBuffer::create(
            ctx.device, {
                            .label = "bg quad idx buf",
                            .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
                            .size = sizeof(UBOMvp),
                        });
        return out;
    }();
    vtx_buf = GpuBuffer::create(
        ctx.device, {
                        .label = "bg quad ver buf",
                        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex,
                        .size = max_vtx_buff_size,
                    });

    idx_buf = GpuBuffer::create(
        ctx.device, {
                        .label = "bg quad idx buf",
                        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index,
                        .size = max_idx_buff_size,
                    });

    auto [layout, binding] = BGLCombinedBuilder{.al = tmp_alloc} //
                                 .addUniform(sizeof(UBOMvp), uniform_buf)
                                 .createLayoutAndGroup(ctx.device);

    SimplePipeline<PosNormColor> pl_builder;
    pl_builder.setShader(shad);
    pipeline = pl_builder.createPipeline(ctx, layout);
    bind_group = binding;
    check(isValid(), "temp_geometry initialization failed");
}

void vex::flow::TempGeometry::draw(const wgfx::GpuContext& ctx, const DrawContext& draw_ctx,
    ROSpan<u32> indices, ROSpan<PosNormColor> vertices)
{
    checkAlwaysRel(
        max_vtx_buff_size >= indices.byteSize(), "input data larger than target gpu buffer");
    checkAlwaysRel(
        max_idx_buff_size >= vertices.byteSize(), "input data larger than target gpu buffer");

    UBOMvp ubo_temp_geo;
    ubo_temp_geo.model_view_proj = draw_ctx.camera_mvp;
    updateUniform(ctx, uniform_buf, ubo_temp_geo);

    auto rpass_enc = ctx.render_pass;

    wgpuQueueWriteBuffer(ctx.queue, vtx_buf.buffer, 0, (u8*)vertices.data, vertices.byteSize());
    wgpuQueueWriteBuffer(ctx.queue, idx_buf.buffer, 0, (u8*)indices.data, indices.byteSize());

    wgpuRenderPassEncoderPushDebugGroup(rpass_enc, "temp geometry setup");
    wgpuRenderPassEncoderSetPipeline(rpass_enc, pipeline);
    wgpuRenderPassEncoderSetBindGroup(rpass_enc, 0, bind_group, 0, 0);

    wgpuRenderPassEncoderSetVertexBuffer(rpass_enc, 0, vtx_buf.buffer, 0, vertices.byteSize());
    wgpuRenderPassEncoderSetIndexBuffer(
        rpass_enc, idx_buf.buffer, WGPUIndexFormat_Uint32, 0, indices.byteSize());
    wgpuRenderPassEncoderPopDebugGroup(rpass_enc);

    wgpuRenderPassEncoderPushDebugGroup(rpass_enc, "lines");
    wgpuRenderPassEncoderDrawIndexed(rpass_enc, indices.size(), 1, 0, 0, 0);
    wgpuRenderPassEncoderPopDebugGroup(rpass_enc);
}


void vex::flow::ColorQuad::init(const wgfx::GpuContext& ctx, const TextShaderLib& text_shad_lib)
{
    vex::InlineBufferAllocator<1024> temp_alloc_resource;
    auto tmp_alloc = temp_alloc_resource.makeAllocatorHandle();

    auto* src = text_shad_lib.shad_src.find(bg_shader_file);
    if (!check(src, "shader not found"))
        return;
    WGPUShaderModule shad = shaderFromSrc(ctx.device, src->text.c_str());

    uniform_buf = [&]() -> GpuBuffer
    {
        UBOHeatMap ubo_transforms;
        auto out = GpuBuffer::create(ctx.device,
            {
                .label = "grid uni buf",
                .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
                .size = sizeof(UBOHeatMap),
            },
            (u8*)&ubo_transforms, sizeof(UBOHeatMap));
        return out;
    }();

    auto [layout, binding] = BGLCombinedBuilder{.al = tmp_alloc} //
                                 .addUniform(sizeof(UBOHeatMap), uniform_buf, 0,
                                     WGPUShaderStage_Fragment | WGPUShaderStage_Vertex)
                                 .createLayoutAndGroup(ctx.device);

    bgl_layout = layout;
    pipeline_data.setShader(shad);
    pipeline_data.vert_state.bufferCount = 0;
    pipeline_data.vert_state.buffers = nullptr;
    pipeline = pipeline_data.createPipeline(ctx, layout);
    bind_group = binding;
}

void vex::flow::ColorQuad::draw(const wgfx::GpuContext& ctx, const DrawContext& draw_ctx)
{
    UBOHeatMap vbo{
        .camera_vp = draw_ctx.camera_mvp,
        .data1 = {draw_ctx.time, draw_ctx.delta_time, 0, 0},
        .data2 = {},
        .data3 = {},
        .data4 = {},
        .quad_size = {1, 1},
    };
    updateUniform(ctx, uniform_buf, vbo);
    {
        auto rpass_enc = ctx.render_pass;
        wgpuRenderPassEncoderPushDebugGroup(rpass_enc, "draw background");
        wgpuRenderPassEncoderSetPipeline(rpass_enc, pipeline);
        wgpuRenderPassEncoderSetBindGroup(rpass_enc, 0, bind_group, 0, 0);

        wgpuRenderPassEncoderDraw(rpass_enc, 6, 1, 0, 0);
        wgpuRenderPassEncoderPopDebugGroup(rpass_enc);
    }
}

bool vex::flow::ColorQuad::reloadShaders(
    vex::TextShaderLib& shader_lib, const wgfx::GpuContext& context)
{
    WGPUShaderModule shad_vert_frag = reloadShader(shader_lib, context, bg_shader_file);
    if (!shad_vert_frag)
        return false;

    pipeline_data.vert_state.module = shad_vert_frag;
    pipeline_data.frag_state.module = shad_vert_frag;
    pipeline_data.pipeline_desc.vertex = pipeline_data.vert_state;
    pipeline_data.pipeline_desc.fragment = &pipeline_data.frag_state;

    WGPU_REL(RenderPipeline, pipeline);
    pipeline = pipeline_data.createPipeline(context, bgl_layout);
    check_(pipeline);


    return true;
}

void vex::flow::CellHeatmapV1::init(const wgfx::GpuContext& ctx, const TextShaderLib& text_shad_lib,
    ROSpan<u32> heatmap, const char* in_shader_file)
{
    hm_shader_file = in_shader_file;
    vex::InlineBufferAllocator<1024> temp_alloc_resource;
    auto tmp_alloc = temp_alloc_resource.makeAllocatorHandle();

    auto* src = text_shad_lib.shad_src.find(hm_shader_file);
    if (!checkAlwaysRel(src, "shader not found"))
        return;
    WGPUShaderModule shad = shaderFromSrc(ctx.device, src->text.c_str());

    uniform_buf = [&]() -> GpuBuffer
    {
        UBOHeatMap ubo_transforms;
        auto out = GpuBuffer::create(ctx.device,
            {
                .label = "grid uni buf",
                .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
                .size = sizeof(UBOHeatMap),
            },
            (u8*)&ubo_transforms, sizeof(UBOHeatMap));
        return out;
    }();

    storage_buf = GpuBuffer::create(
        ctx.device, {
                        .label = "grid uni buf",
                        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage,
                        .size = (u32)heatmap.byteSize(),
                    });

    auto [layout, binding] = BGLCombinedBuilder{.al = tmp_alloc} //
                                 .addUniform(sizeof(UBOHeatMap), uniform_buf, 0,
                                     WGPUShaderStage_Fragment | WGPUShaderStage_Vertex)
                                 .addStorageBuffer(16 * 16, storage_buf,
                                     WGPUShaderStage_Vertex | WGPUShaderStage_Fragment)
                                 .createLayoutAndGroup(ctx.device);

    bgl_layout = layout;
    pipeline_data.setShader(shad);
    pipeline_data.vert_state.bufferCount = 0;
    pipeline_data.vert_state.buffers = nullptr;
    pipeline = pipeline_data.createPipeline(ctx, layout);
    bind_group = binding;
}

void vex::flow::CellHeatmapV1::draw(
    const wgfx::GpuContext& ctx, const DrawContext& draw_ctx, const HeatmapDynamicData& args)
{
    UBOHeatMap vbo{
        .camera_vp = draw_ctx.camera_mvp,
        .data1 = args.color1,
        .data2 = args.color2,
        .data3 = {},
        .data4 = {draw_ctx.time, 0, 0, 0},
        .quad_size = {draw_ctx.camera_h, draw_ctx.camera_h},
        .buffer_dimensions = args.bounds,
    };
    checkAlwaysRel(
        args.buffer.byteSize() % 4 == 0, "buffer size must satisfy constraints (mul of 4)");

    updateUniform(ctx, uniform_buf, vbo);
    wgpuQueueWriteBuffer(
        ctx.queue, storage_buf.buffer, 0, args.buffer.data, args.buffer.byteSize());
    {
        auto rpass_enc = ctx.render_pass;
        wgpuRenderPassEncoderPushDebugGroup(rpass_enc, "draw heatmap");
        wgpuRenderPassEncoderSetPipeline(rpass_enc, pipeline);
        wgpuRenderPassEncoderSetBindGroup(rpass_enc, 0, bind_group, 0, 0);

        wgpuRenderPassEncoderDraw(rpass_enc, 6, 1, 0, 0);
        wgpuRenderPassEncoderPopDebugGroup(rpass_enc);
    }
}

bool vex::flow::CellHeatmapV1::reloadShaders(
    vex::TextShaderLib& shader_lib, const wgfx::GpuContext& context)
{
    WGPUShaderModule shad_vert_frag = reloadShader(shader_lib, context, hm_shader_file);
    if (!shad_vert_frag)
        return false;

    pipeline_data.vert_state.module = shad_vert_frag;
    pipeline_data.frag_state.module = shad_vert_frag;
    pipeline_data.pipeline_desc.vertex = pipeline_data.vert_state;
    pipeline_data.pipeline_desc.fragment = &pipeline_data.frag_state;

    WGPU_REL(RenderPipeline, pipeline);
    pipeline = pipeline_data.createPipeline(context, bgl_layout);
    check_(pipeline);

    return true;
}

void vex::flow::ComputeFields::init(const wgfx::GpuContext& ctx, const TextShaderLib& text_shad_lib,
    const char* in_shader_file, wgfx::GpuBuffer& map_data_buf, v2u32 size)
{
    cf_shader_file = in_shader_file;
    vex::InlineBufferAllocator<1024> temp_alloc_resource;
    auto tmp_alloc = temp_alloc_resource.makeAllocatorHandle();

    auto* src = text_shad_lib.shad_src.find(cf_shader_file);
    if (!checkAlwaysRel(src, "shader not found"))
        return;
    WGPUShaderModule shad = shaderFromSrc(ctx.device, src->text.c_str());

    uniform_buf = GpuBuffer::create(
        ctx.device, {
                        .label = "compute uni buf",
                        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
                        .size = sizeof(UBO),
                    });

    output_buf = GpuBuffer::create(
        ctx.device, {
                        .label = "f32 vec output",
                        .usage = WGPUBufferUsage_CopySrc | WGPUBufferUsage_Storage,
                        .size = (u32)(size.x * size.y * sizeof(v2f)),
                    });
    staging_buf = GpuBuffer::create(
        ctx.device, {
                        .label = "f32 vec stage",
                        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead,
                        .size = (u32)(size.x * size.y * sizeof(v2f)),
                    });

    auto [layout,
        binding] = BGLCombinedBuilder{.al = tmp_alloc} //
                       .addUniform(sizeof(UBO), uniform_buf, 0, WGPUShaderStage_Compute)
                       .addStorageBuffer(16 * 16, map_data_buf, WGPUShaderStage_Compute, true)
                       .addStorageBuffer(16 * 16, output_buf, WGPUShaderStage_Compute, false)
                       .createLayoutAndGroup(ctx.device);

    bgl_layout = layout;
    pipeline = pipeline_data.createPipeline(ctx, shad, layout);
    bind_group = binding;
}

void vex::flow::ComputeFields::compute(const wgfx::GpuContext& ctx, const ComputeArgs& args)
{ //
    WGPUCommandBufferDescriptor ctx_desc_fin{nullptr, "compute submit"};

    auto encoder = wgpuDeviceCreateCommandEncoder(ctx.device, nullptr);
    auto compute_pass = wgpuCommandEncoderBeginComputePass(encoder, nullptr);
    wgpuComputePassEncoderSetPipeline(compute_pass, pipeline);
    wgpuComputePassEncoderSetBindGroup(compute_pass, 0, bind_group, 0, nullptr);

    UBO vbo{.size = args.map_size};
    updateUniform(ctx, uniform_buf, vbo);

    const auto work_size = args.map_size.x * args.map_size.y;
    check_((work_size % 64) == 0);

    wgpuComputePassEncoderDispatchWorkgroups(compute_pass, work_size / 64, 1, 1);

    wgpuComputePassEncoderEnd(compute_pass);
    WGPU_REL(ComputePassEncoder, compute_pass);
    // vex::Buffer<v2f> dbg;
    // dbg.addZeroed(work_size);

    wgpuCommandEncoderCopyBufferToBuffer(
        encoder, output_buf.buffer, 0, staging_buf.buffer, 0, staging_buf.desc.size);

    auto cmd_buf = wgpuCommandEncoderFinish(encoder, &ctx_desc_fin);
    wgpuQueueSubmit(ctx.queue, 1, &cmd_buf);

    if constexpr (false)
    {
        std::atomic_bool sync_ready = ATOMIC_VAR_INIT(false);
        spdlog::stopwatch sw;
        wgpuRequest(
            wgpuBufferMapAsync,
            [&, sw](WGPUBufferMapAsyncStatus status, void*)
            {
                defer_ { SPDLOG_WARN(" mapping took approx {} microsec", sw.elapsed() / 1us); };
                if (status == 0)
                {
                    auto mapped_data = (f32*)wgpuBufferGetConstMappedRange(
                        staging_buf.buffer, 0, staging_buf.desc.size);
                    wgpuBufferUnmap(staging_buf.buffer);
                }
                sync_ready = true;
            },
            staging_buf.buffer, WGPUMapMode_Read, 0, staging_buf.desc.size);

        wgpuQueueSubmit(ctx.queue, 0, nullptr);
        wgpuPollWait(ctx, sync_ready, -1);
    }

    wgpuDeviceTick(ctx.device);
}

bool vex::flow::ComputeFields::reloadShaders(
    vex::TextShaderLib& shader_lib, const wgfx::GpuContext& context)
{
    WGPUShaderModule compute = reloadShader(shader_lib, context, cf_shader_file);
    if (!compute)
        return false;

    WGPU_REL(ComputePipeline, pipeline);
    pipeline = pipeline_data.createPipeline(context, compute, bgl_layout);
    check_(pipeline);
    return true;
}

void vex::flow::FlowFieldsOverlay::init(const wgfx::GpuContext& ctx,
    const TextShaderLib& text_shad_lib, wgfx::GpuBuffer& flow_v2f_buf, const char* in_shader_file)
{
    shader_file = in_shader_file;
    vex::InlineBufferAllocator<1024> temp_alloc_resource;
    auto tmp_alloc = temp_alloc_resource.makeAllocatorHandle();

    auto* src = text_shad_lib.shad_src.find(shader_file);
    if (!checkAlwaysRel(src, "shader not found"))
        return;
    WGPUShaderModule shad = shaderFromSrc(ctx.device, src->text.c_str());

    uniform_buf = [&]() -> GpuBuffer
    {
        UBOHeatMap ubo_transforms;
        auto out = GpuBuffer::create(ctx.device,
            {
                .label = "flow uni buf",
                .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
                .size = sizeof(UBOHeatMap),
            },
            (u8*)&ubo_transforms, sizeof(UBOHeatMap));
        return out;
    }();

    auto [layout, binding] = BGLCombinedBuilder{.al = tmp_alloc} //
                                 .addUniform(sizeof(UBOHeatMap), uniform_buf, 0,
                                     WGPUShaderStage_Fragment | WGPUShaderStage_Vertex)
                                 .addStorageBuffer(16 * 16, flow_v2f_buf,
                                     WGPUShaderStage_Fragment | WGPUShaderStage_Vertex)
                                 .createLayoutAndGroup(ctx.device);

    bgl_layout = layout;

    pipeline_data.depth_stencil_state.depthWriteEnabled = false;
    pipeline_data.depth_stencil_state.depthCompare = WGPUCompareFunction_Always;

    pipeline_data.setShader(shad);
    pipeline_data.vert_state.bufferCount = 0;
    pipeline_data.vert_state.buffers = nullptr;
    pipeline = pipeline_data.createPipeline(ctx, layout);
    bind_group = binding;
}

void vex::flow::FlowFieldsOverlay::draw(
    const wgfx::GpuContext& ctx, const DrawContext& draw_ctx, OverlayData args)
{
    const float cell_h = (draw_ctx.camera_h / args.bounds.y);
    const float cell_w = cell_h;
    const v2f arrow_size = {cell_h * 0.2, cell_h};
    //const v2f orig_loc_space = { cell_h * 0.2, cell_h };

    UBOHeatMap vbo{
        .camera_vp = draw_ctx.camera_mvp,
        .data1 = args.color1,
        .data2 = args.color2,
        .data3 = {arrow_size.x, arrow_size.y, cell_w, cell_h},
        .data4 = {draw_ctx.time, 0, 0, 0},
        .quad_size = {draw_ctx.camera_h, draw_ctx.camera_h},
        .buffer_dimensions = args.bounds,
    };
    updateUniform(ctx, uniform_buf, vbo);
    {
        auto rpass_enc = ctx.render_pass;
        wgpuRenderPassEncoderPushDebugGroup(rpass_enc, "draw flow overlays");
        wgpuRenderPassEncoderSetPipeline(rpass_enc, pipeline);
        wgpuRenderPassEncoderSetBindGroup(rpass_enc, 0, bind_group, 0, 0);

        wgpuRenderPassEncoderDraw(rpass_enc, args.bounds.x * args.bounds.y * 6, 1, 0, 0);
        wgpuRenderPassEncoderPopDebugGroup(rpass_enc);
    }
}

bool vex::flow::FlowFieldsOverlay::reloadShaders(
    vex::TextShaderLib& shader_lib, const wgfx::GpuContext& context)
{
    WGPUShaderModule shad_vert_frag = reloadShader(shader_lib, context, shader_file);
    if (!shad_vert_frag)
        return false;

    pipeline_data.vert_state.module = shad_vert_frag;
    pipeline_data.frag_state.module = shad_vert_frag;
    pipeline_data.pipeline_desc.vertex = pipeline_data.vert_state;
    pipeline_data.pipeline_desc.fragment = &pipeline_data.frag_state;

    WGPU_REL(RenderPipeline, pipeline);
    pipeline = pipeline_data.createPipeline(context, bgl_layout);
    check_(pipeline);

    return true;
}
