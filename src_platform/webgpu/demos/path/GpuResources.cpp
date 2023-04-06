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
    vex::InlineBufferAllocator<4096> temp_alloc_resource;
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
    vex::InlineBufferAllocator<4096> temp_alloc_resource;
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
    vex::InlineBufferAllocator<4096> temp_alloc_resource;
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
    vex::InlineBufferAllocator<4096> temp_alloc_resource;
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
        .data1 = {args.opacity, 0.0f, 0.0f, 0.0f},
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
    vex::InlineBufferAllocator<4096> temp_alloc_resource;
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
    subdiv_buf = GpuBuffer::create(
        ctx.device, {
                        .label = "f32 vec stage",
                        .usage = WGPUBufferUsage_CopySrc | WGPUBufferUsage_Storage,
                        .size = (u32)(size.x * size.y * sizeof(v2f) * 16),
                    });
    // make main pass pl
    {
        auto [layout,
            binding] = BGLCombinedBuilder{.al = tmp_alloc} //
                           .addUniform(sizeof(UBO), uniform_buf, 0, WGPUShaderStage_Compute)
                           .addStorageBuffer(16 * 16, map_data_buf, WGPUShaderStage_Compute, true)
                           .addStorageBuffer(16 * 16, output_buf, WGPUShaderStage_Compute, false)
                           .createLayoutAndGroup(ctx.device);

        //
        main_pass.bgl_layout = layout;
        main_pass.pipeline = main_pass.pipeline_data.createPipeline(ctx, shad, layout);
        main_pass.bind_group = binding;
    }
    // make subdiv pass pl
    {
        auto [layout,
            binding] = BGLCombinedBuilder{.al = tmp_alloc} //
                           .addUniform(sizeof(UBO), uniform_buf, 0, WGPUShaderStage_Compute)
                           .addStorageBuffer(16 * 16, map_data_buf, WGPUShaderStage_Compute, true)
                           .addStorageBuffer(16 * 16, output_buf, WGPUShaderStage_Compute, true)
                           .addStorageBuffer(16 * 16, subdiv_buf, WGPUShaderStage_Compute, false)
                           .createLayoutAndGroup(ctx.device);

        wgpuDeviceTick(ctx.device);

        subdiv_pass.bgl_layout = layout;
        subdiv_pass.pipeline = subdiv_pass.pipeline_data.createPipeline(ctx, shad, layout);
        subdiv_pass.bind_group = binding;
    }
}

void vex::flow::ComputeFields::compute(wgfx::CompContext& ctx, const ComputeArgs& args)
{ //
    UBO vbo{
        .size = args.map_size,
        .flags = args.flags,
    };
    updateUniform(ctx, uniform_buf, vbo);

    {
        const auto work_size = args.map_size.x * args.map_size.y;
        u32 num_groups = (work_size / (64 * 32));
        check_((work_size % 64) == 0);
        wgpuComputePassEncoderSetPipeline(ctx.comp_pass, main_pass.pipeline);
        wgpuComputePassEncoderSetBindGroup(ctx.comp_pass, 0, main_pass.bind_group, 0, nullptr);
        wgpuComputePassEncoderDispatchWorkgroups(
            ctx.comp_pass, num_groups > 0 ? num_groups : 1, 1, 1);
    }
    {
        wgpuComputePassEncoderSetPipeline(ctx.comp_pass, subdiv_pass.pipeline);
        wgpuComputePassEncoderSetBindGroup(ctx.comp_pass, 0, subdiv_pass.bind_group, 0, nullptr);
        wgpuComputePassEncoderDispatchWorkgroups(
            ctx.comp_pass, args.map_size.x / 8, args.map_size.y / 8, 1);
    }

    wgpuComputePassEncoderEnd(ctx.comp_pass);
    WGPU_REL(ComputePassEncoder, ctx.comp_pass);
}

bool vex::flow::ComputeFields::reloadShaders(
    vex::TextShaderLib& shader_lib, const wgfx::GpuContext& context)
{
    WGPUShaderModule compute = reloadShader(shader_lib, context, cf_shader_file);
    if (!compute)
        return false;

    WGPU_REL(ComputePipeline, main_pass.pipeline);
    main_pass.pipeline = main_pass.pipeline_data.createPipeline(
        context, compute, main_pass.bgl_layout);
    check_(main_pass.pipeline);

    WGPU_REL(ComputePipeline, subdiv_pass.pipeline);
    subdiv_pass.pipeline = subdiv_pass.pipeline_data.createPipeline(
        context, compute, subdiv_pass.bgl_layout);
    check_(subdiv_pass.pipeline);

    return true;
}

void vex::flow::FlowFieldsOverlay::init(const wgfx::GpuContext& ctx,
    const TextShaderLib& text_shad_lib, wgfx::GpuBuffer& flow_v2f_buf, wgfx::GpuBuffer& subdiv_buf,
    const char* in_shader_file)
{
    shader_file = in_shader_file;
    vex::InlineBufferAllocator<4096> temp_alloc_resource;
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
                                 .addStorageBuffer(16 * 16, flow_v2f_buf, WGPUShaderStage_Vertex)
                                 .addStorageBuffer(16 * 16, subdiv_buf, WGPUShaderStage_Vertex)
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
    // const v2f orig_loc_space = { cell_h * 0.2, cell_h };

    const bool use_subdiv = true;

    UBOHeatMap vbo{
        .camera_vp = draw_ctx.camera_mvp,
        .data1 = args.color1,
        .data2 = args.color2,
        .data3 = {arrow_size.x, arrow_size.y, cell_w, cell_h},
        .data4 = {draw_ctx.time, (float)use_subdiv, 0, 0},
        .quad_size = {draw_ctx.camera_h, draw_ctx.camera_h},
        .buffer_dimensions = args.bounds,
    };
    updateUniform(ctx, uniform_buf, vbo);
    {
        auto rpass_enc = ctx.render_pass;
        wgpuRenderPassEncoderPushDebugGroup(rpass_enc, "draw flow overlays");
        wgpuRenderPassEncoderSetPipeline(rpass_enc, pipeline);
        wgpuRenderPassEncoderSetBindGroup(rpass_enc, 0, bind_group, 0, 0);

        wgpuRenderPassEncoderDraw(
            rpass_enc, args.bounds.x * args.bounds.y * 6 * (use_subdiv ? 16 : 1), 1, 0, 0);
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

void vex::flow::ParticleSym::spawnForSymulation(const wgfx::GpuContext& ctx, ROSpan<Particle> parts)
{
    sym_data.num_particles = parts.len;
    wgpuQueueWriteBuffer(
        ctx.queue, hash_data.particle_data_buf.buffer, 0, (u8*)parts.data, parts.byteSize());
}

void vex::flow::ParticleSym::init(
    const wgfx::GpuContext& ctx, const TextShaderLib& text_shad_lib, InitArgs args)
{
    vex::InlineBufferAllocator<4096> temp_alloc_resource;
    auto tmp_alloc = temp_alloc_resource.makeAllocatorHandle();
    // init hashing compute
    {
        defer_ { temp_alloc_resource.reset(); };

        hash_data.shader = args.shader_hash;
        auto* src = text_shad_lib.shad_src.find(hash_data.shader);
        if (!checkAlwaysRel(src, "shader not found"))
            return;
        WGPUShaderModule shad = shaderFromSrc(ctx.device, src->text.c_str());

        hash_data.uniform_buf = GpuBuffer::create(
            ctx.device, {
                            .label = "hash uni buf",
                            .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
                            .size = sizeof(SimulateUBO),
                        });
        hash_data.particle_data_buf = GpuBuffer::create(
            ctx.device, {
                            .label = "particle buf",
                            .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage,
                            .size = (u32)(args.max_particles * sizeof(Particle)),
                        });
        auto table_size = args.bounds.x * args.bounds.y * spatial_table_depth * 16 * 4;
        hash_data.spatial_table = GpuBuffer::create(
            ctx.device, {
                            .label = "table buf",
                            .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage,
                            .size = (u32)(table_size), // any way to fetch struct size from shader?
                        });
        hash_data.counters = GpuBuffer::create(
            ctx.device, {
                            .label = "counter buf",
                            .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage,
                            .size = (u32)(table_size),
                        });
        auto dbg = GpuBuffer::create(
            ctx.device, {
                            .label = "counter buf",
                            .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage,
                            .size = (u32)(args.max_particles * 4),
                        });

        auto [layout, binding] =
            BGLCombinedBuilder{.al = tmp_alloc} //
                .addUniform(sizeof(SimulateUBO), hash_data.uniform_buf, 0, WGPUShaderStage_Compute)
                .addStorageBuffer(256, hash_data.particle_data_buf, WGPUShaderStage_Compute, true)
                .addStorageBuffer(256, hash_data.counters, WGPUShaderStage_Compute, false)
                .addStorageBuffer(256, hash_data.spatial_table, WGPUShaderStage_Compute, false)
                .addStorageBuffer(256, dbg, WGPUShaderStage_Compute, false)
                .createLayoutAndGroup(ctx.device);

        hash_data.bgl_layout = layout;
        hash_data.count_pipeline = hash_data.count_pipeline_data.createPipeline(ctx, shad, layout);
        hash_data.bind_group = binding;

        hash_data.zero_pipeline = hash_data.zero_pipeline_data.createPipeline(ctx, shad, layout);
        // hash_data.hash_pipeline = hash_data.hash_pipeline_data.createPipeline(ctx, shad, layout);
        // hash_data.calc_offsets_pipeline = hash_data.calc_offsets_pipeline_data.createPipeline(
        //     ctx, shad, layout);
    }
    // init simulation compute
    {
        defer_ { temp_alloc_resource.reset(); };

        sym_data.shader = args.shader_compute;

        auto* src = text_shad_lib.shad_src.find(sym_data.shader);
        if (!checkAlwaysRel(src, "shader not found"))
            return;
        WGPUShaderModule shad = shaderFromSrc(ctx.device, src->text.c_str());

        checkLethal(args.flow_v2f_buf != nullptr, "passed nullptr instead of input buffer");

        auto dbg = GpuBuffer::create(
            ctx.device, {
                            .label = "counter buf",
                            .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage,
                            .size = (u32)(args.max_particles * 4),
                        });

        // #fixme - separate bindings
        auto [layout, binding] =
            BGLCombinedBuilder{.al = tmp_alloc} //
                .addUniform(sizeof(SimulateUBO), hash_data.uniform_buf, 0, WGPUShaderStage_Compute)
                .addStorageBuffer(256, hash_data.particle_data_buf, WGPUShaderStage_Compute, false)
                .addStorageBuffer(256, *(args.flow_v2f_buf), WGPUShaderStage_Compute, true)
                .addStorageBuffer(256, *(args.flow_sub_v2f_buf), WGPUShaderStage_Compute, true)
                .addStorageBuffer(256, *(args.cells_buf), WGPUShaderStage_Compute, true)
                .addStorageBuffer(256, hash_data.spatial_table, WGPUShaderStage_Compute, true)
                .addStorageBuffer(256, dbg, WGPUShaderStage_Compute, false)
                .createLayoutAndGroup(ctx.device);

        sym_data.bgl_layout = layout;
        sym_data.move_pipeline = sym_data.move_pipeline_data.createPipeline(ctx, shad, layout);
        sym_data.bind_group = binding;

        sym_data.solve_pipeline = sym_data.solve_pipeline_data.createPipeline(ctx, shad, layout);
        sym_data.solve_pass2_pipeline = sym_data.solve_pass2_data.createPipeline(ctx, shad, layout);
    }
    // init visual
    {
        defer_ { temp_alloc_resource.reset(); };
        auto texture = loadImage(args.particle_texture);
        defer_ { texture.release(); };

        auto bg_tex = Texture::loadFormData(ctx, texture,
            {
                .usage = (WGPUTextureUsage_CopyDst | WGPUTextureUsage_TextureBinding),
            });
        check_(bg_tex.isValid());
        vis_data.tex_view = TextureView::create(ctx.device, bg_tex, {});


        vis_data.shader = args.shader_visual;

        auto* src = text_shad_lib.shad_src.find(vis_data.shader);
        if (!checkAlwaysRel(src, "shader not found"))
            return;
        WGPUShaderModule shad = shaderFromSrc(ctx.device, src->text.c_str());

        vis_data.uniform_buf = GpuBuffer::create(
            ctx.device, {
                            .label = "particles uni buf",
                            .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
                            .size = sizeof(VisualUBO),
                        });

        auto [layout, binding] = BGLCombinedBuilder{.al = tmp_alloc} //
                                     .addUniform(sizeof(VisualUBO), vis_data.uniform_buf, 0,
                                         WGPUShaderStage_Fragment | WGPUShaderStage_Vertex)
                                     .addStorageBuffer(256, hash_data.particle_data_buf,
                                         WGPUShaderStage_Fragment | WGPUShaderStage_Vertex)
                                     .addTexView(vis_data.tex_view.view)
                                     .addSampler(vis_data.tex_view.sampler)
                                     .createLayoutAndGroup(ctx.device);

        vis_data.bgl_layout = layout;
        WGPUBlendState bl_state{
            .color =
                {
                    .operation = WGPUBlendOperation_Add,
                    .srcFactor = WGPUBlendFactor_SrcAlpha,
                    .dstFactor = WGPUBlendFactor_One,
                },
            .alpha =
                {
                    .operation = WGPUBlendOperation_Add,
                    .srcFactor = WGPUBlendFactor_Zero,
                    .dstFactor = WGPUBlendFactor_One,
                },
        };

        vis_data.pipeline_data.color_target_state.blend = &bl_state;

        vis_data.pipeline_data.depth_stencil_state.depthWriteEnabled = false;
        vis_data.pipeline_data.depth_stencil_state.depthCompare = WGPUCompareFunction_Always;

        vis_data.pipeline_data.setShader(shad);
        vis_data.pipeline_data.vert_state.bufferCount = 0;
        vis_data.pipeline_data.vert_state.buffers = nullptr;
        vis_data.pipeline = vis_data.pipeline_data.createPipeline(ctx, layout);
        vis_data.bind_group = binding;
    }
}

void vex::flow::ParticleSym::compute(wgfx::CompContext& ctx, CompArgs args)
{
    defer_
    {
        wgpuComputePassEncoderEnd(ctx.comp_pass);
        WGPU_REL(ComputePassEncoder, ctx.comp_pass);
    };
    if (sym_data.num_particles < 1)
        return;

    checkLethal(args.settings, "nullptr settings container");

    auto rad = args.settings->valueOr(opt_part_radius.key_name, default_rel_radius);
    auto speed = args.settings->valueOr(opt_part_speed.key_name, 1.25f);
    auto inertia = args.settings->valueOr(opt_part_inertia.key_name, 1.0f);
    auto separation = args.settings->valueOr(opt_part_sep.key_name, 0.25f);
    auto drag = args.settings->valueOr(opt_part_drag.key_name, 0.05f);
    auto speed_max = args.settings->valueOr(opt_part_speed_max.key_name, 4.00f);

    u32 spatial_subdiv = u32(glm::round(1.0f / rad));
    SimulateUBO vbo{
        .bounds = args.bounds,
        .grid_min = args.grid_min,
        .grid_size = args.grid_size,
        .cell_size = args.cell_size,
        .num_particles = sym_data.num_particles,
        .speed_base = speed,
        .radius = rad * args.cell_size.x, // #fixme one constant
        .separation = separation,
        .inertia = inertia,
        .drag = drag,
        .speed_max = speed_max,
        .delta_time = args.dt,
    };

    vbo.flags = 0;
    vbo.spatial_table_size = v2u32{args.bounds.x * spatial_subdiv,
        args.bounds.y * spatial_subdiv}; // #fixme - use radius to calc density

    updateUniform(ctx, hash_data.uniform_buf, vbo);

    const auto work_size = sym_data.num_particles;

    const auto cells_in_table = vbo.spatial_table_size.x * vbo.spatial_table_size.y;
    // prepare
    if (experimental)
    {
        {
            wgpuComputePassEncoderSetPipeline(ctx.comp_pass, hash_data.zero_pipeline);
            wgpuComputePassEncoderSetBindGroup(ctx.comp_pass, 0, hash_data.bind_group, 0, nullptr);
            wgpuComputePassEncoderDispatchWorkgroups(
                ctx.comp_pass, (cells_in_table / 64) + 1, 1, 1);

            wgpuComputePassEncoderSetPipeline(ctx.comp_pass, hash_data.count_pipeline);
            wgpuComputePassEncoderSetBindGroup(ctx.comp_pass, 0, hash_data.bind_group, 0, nullptr);
            wgpuComputePassEncoderDispatchWorkgroups(ctx.comp_pass, (work_size / 64) + 1, 1, 1);
        }
        const u32 num_cubes = vbo.spatial_table_size.y / 4 + 1;
        const u32 solver_groups = (num_cubes * num_cubes) / (64);

        wgpuComputePassEncoderSetPipeline(ctx.comp_pass, sym_data.solve_pipeline);
        wgpuComputePassEncoderSetBindGroup(ctx.comp_pass, 0, sym_data.bind_group, 0, nullptr);
        wgpuComputePassEncoderDispatchWorkgroups(ctx.comp_pass, solver_groups, 1, 1);

        wgpuComputePassEncoderSetPipeline(ctx.comp_pass, sym_data.solve_pass2_pipeline);
        wgpuComputePassEncoderSetBindGroup(ctx.comp_pass, 0, sym_data.bind_group, 0, nullptr);
        wgpuComputePassEncoderDispatchWorkgroups(ctx.comp_pass, solver_groups, 1, 1);
    }
    else
    {
        u32 num_groups = (work_size / 64) + 1;
        const auto num_groups_clamped = num_groups > 0 ? num_groups : 1;
        // solve particle-to-particle collisions
        wgpuComputePassEncoderSetPipeline(ctx.comp_pass, sym_data.solve_pipeline);
        wgpuComputePassEncoderSetBindGroup(ctx.comp_pass, 0, sym_data.bind_group, 0, nullptr);
        wgpuComputePassEncoderDispatchWorkgroups(ctx.comp_pass, num_groups, 1, 1);
    }
    //  solve particle-to-wall & movement
    {
        u32 num_groups = (work_size / 64) + 1;
        const auto num_groups_clamped = num_groups > 0 ? num_groups : 1;
        wgpuComputePassEncoderSetPipeline(ctx.comp_pass, sym_data.move_pipeline);
        wgpuComputePassEncoderSetBindGroup(ctx.comp_pass, 0, sym_data.bind_group, 0, nullptr);
        wgpuComputePassEncoderDispatchWorkgroups(ctx.comp_pass, num_groups_clamped, 1, 1);
    }
}

void vex::flow::ParticleSym::draw(
    const wgfx::GpuContext& ctx, const DrawContext& draw_ctx, DrawArgs args)
{
    if (sym_data.num_particles < 1)
        return;

    auto rad = args.settings->valueOr(opt_part_radius.key_name, default_rel_radius);
    auto color1 = args.settings->valueOr<v4f>(opt_part_color.key_name, Color::green());
    auto auto_color = args.settings->valueOr<bool>(opt_part_auto_color, false);
    VisualUBO vbo{
        .camera_vp = draw_ctx.camera_mvp,
        .color1 = color1,
        .color2 = args.color2,
        .size = {args.cell_size.x * rad * 1, args.cell_size.y * rad * 1, 1, 1},
        .num_particles = args.num_particles,
        .flags = auto_color,
    };
    updateUniform(ctx, vis_data.uniform_buf, vbo);
    {
        auto rpass_enc = ctx.render_pass;
        wgpuRenderPassEncoderPushDebugGroup(rpass_enc, "draw particles");
        wgpuRenderPassEncoderSetPipeline(rpass_enc, vis_data.pipeline);
        wgpuRenderPassEncoderSetBindGroup(rpass_enc, 0, vis_data.bind_group, 0, 0);

        wgpuRenderPassEncoderDraw(rpass_enc, sym_data.num_particles * 6, 1, 0, 0);
        wgpuRenderPassEncoderPopDebugGroup(rpass_enc);
    }
}

bool vex::flow::ParticleSym::reloadShaders(
    vex::TextShaderLib& shader_lib, const wgfx::GpuContext& context)
{
    {
        WGPUShaderModule shader = reloadShader(shader_lib, context, hash_data.shader);
        if (shader)
        {
            WGPU_REL(ComputePipeline, hash_data.count_pipeline);
            hash_data.count_pipeline = hash_data.count_pipeline_data.createPipeline(
                context, shader, hash_data.bgl_layout);
            check_(hash_data.count_pipeline);
        }

        // WGPU_REL(ComputePipeline, hash_data.hash_pipeline);
        // hash_data.hash_pipeline = hash_data.hash_pipeline_data.createPipeline(
        //     context, shader, hash_data.bgl_layout);
        // check_(hash_data.hash_pipeline);

        // WGPU_REL(ComputePipeline, hash_data.calc_offsets_pipeline);
        // hash_data.calc_offsets_pipeline = hash_data.calc_offsets_pipeline_data.createPipeline(
        //     context, shader, hash_data.bgl_layout);
        // check_(hash_data.calc_offsets_pipeline);
    }
    {
        WGPUShaderModule shader = reloadShader(shader_lib, context, sym_data.shader);
        if (shader)
        {
            WGPU_REL(ComputePipeline, sym_data.move_pipeline);
            sym_data.move_pipeline = sym_data.move_pipeline_data.createPipeline(
                context, shader, sym_data.bgl_layout);
            check_(sym_data.move_pipeline);

            WGPU_REL(ComputePipeline, sym_data.solve_pipeline);
            sym_data.solve_pipeline = sym_data.solve_pipeline_data.createPipeline(
                context, shader, sym_data.bgl_layout);
            check_(sym_data.solve_pipeline);

            WGPU_REL(ComputePipeline, sym_data.solve_pass2_pipeline);
            sym_data.solve_pass2_pipeline = sym_data.solve_pass2_data.createPipeline(
                context, shader, sym_data.bgl_layout);
            check_(sym_data.solve_pass2_pipeline);
        }
    }
    {
        WGPUBlendState bl_state_add{
            .color =
                {
                    .operation = WGPUBlendOperation_Add,
                    .srcFactor = WGPUBlendFactor_SrcAlpha,
                    .dstFactor = WGPUBlendFactor_One,
                },
            .alpha =
                {
                    .operation = WGPUBlendOperation_Add,
                    .srcFactor = WGPUBlendFactor_Zero,
                    .dstFactor = WGPUBlendFactor_One,
                },
        };

        vis_data.pipeline_data.color_target_state.blend = &bl_state_add;
        WGPUShaderModule shad_vert_frag = reloadShader(shader_lib, context, vis_data.shader);
        if (shad_vert_frag)
        {
            vis_data.pipeline_data.vert_state.module = shad_vert_frag;
            vis_data.pipeline_data.frag_state.module = shad_vert_frag;
            vis_data.pipeline_data.pipeline_desc.vertex = vis_data.pipeline_data.vert_state;
            vis_data.pipeline_data.pipeline_desc.fragment = &vis_data.pipeline_data.frag_state;

            WGPU_REL(RenderPipeline, vis_data.pipeline);
            vis_data.pipeline = vis_data.pipeline_data.createPipeline(context, vis_data.bgl_layout);
            check_(vis_data.pipeline);
        }
    }
    return true;
}

void vex::flow::ParticleSym::release()
{ //
    SPDLOG_ERROR("#fixme - add release");
}
