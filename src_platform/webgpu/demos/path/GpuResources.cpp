#include "GpuResources.h"

#include <webgpu/render/LayoutManagement.h>
using namespace vex;
using namespace vex::pf_demo;
using namespace wgfx;
using namespace std::literals::string_view_literals;


void ViewportGrid::init(const wgfx::RenderContext& ctx, const TextShaderLib& text_shad_lib)
{
    vex::InlineBufferAllocator<1024> temp_alloc_resource;
    auto tmp_alloc = temp_alloc_resource.makeAllocatorHandle();

    auto* src = text_shad_lib.shad_src.find(grid_shader_file);
    if (!check(src, "shader not found"))
        return;
    WGPUShaderModule shad = shaderFromSrc(ctx.device, src->text.c_str());

    auto tmp_mesh = buildPlane<PosNormUv>(
        tmp_alloc, 2, {-1.0f, -1.0f}, {1.0f, 1.0f}, 8.0);

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

void vex::pf_demo::ViewportGrid::draw(const wgfx::RenderContext& ctx, const DrawContext& draw_ctx)
{
    // get pixel size
    UBOMvp4Colors4Floats vbo{
        .model_view_proj = draw_ctx.camera_mvp,
        .data1 = Color::blue(),
        .data2 = {draw_ctx.time, draw_ctx.grid_half_size, draw_ctx.pixel_scale, draw_ctx.grid_w},
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

bool ViewportGrid::reloadShaders(TextShaderLib& shader_lib, const RenderContext& context)
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

void vex::pf_demo::TempGeometry::init(
    const wgfx::RenderContext& ctx, const TextShaderLib& text_shad_lib)
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

void vex::pf_demo::TempGeometry::draw(const wgfx::RenderContext& ctx, const DrawContext& draw_ctx,
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


void vex::pf_demo::ColorQuad::init(
    const wgfx::RenderContext& ctx, const TextShaderLib& text_shad_lib)
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

void vex::pf_demo::ColorQuad::draw(const wgfx::RenderContext& ctx, const DrawContext& draw_ctx)
{
    UBOHeatMap vbo{
        .camera_vp = draw_ctx.camera_mvp,
        .data1 = {draw_ctx.time, draw_ctx.delta_time, draw_ctx.pixel_scale, 0},
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

bool vex::pf_demo::ColorQuad::reloadShaders(
    vex::TextShaderLib& shader_lib, const wgfx::RenderContext& context)
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

void vex::pf_demo::CellHeatmapV1::init(
    const wgfx::RenderContext& ctx, const TextShaderLib& text_shad_lib, ROSpan<u32> heatmap)
{
    vex::InlineBufferAllocator<1024> temp_alloc_resource;
    auto tmp_alloc = temp_alloc_resource.makeAllocatorHandle();

    auto* src = text_shad_lib.shad_src.find(hm_shader_file);
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

    storage_buf = GpuBuffer::create(ctx.device,
        {
            .label = "grid uni buf",
            .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage,
            .size = (u32)heatmap.byteSize(),
        },
        (u8*)heatmap.data, heatmap.byteSize());

    auto [layout, binding] = BGLCombinedBuilder{.al = tmp_alloc} //
                                 .addUniform(sizeof(UBOHeatMap), uniform_buf, 0,
                                     WGPUShaderStage_Fragment | WGPUShaderStage_Vertex)
                                 .addStorageBuffer(16 * 16, storage_buf,
                                     WGPUShaderStage_Vertex | WGPUShaderStage_Fragment, true)
                                 .createLayoutAndGroup(ctx.device);

    bgl_layout = layout;
    pipeline_data.setShader(shad);
    pipeline_data.vert_state.bufferCount = 0;
    pipeline_data.vert_state.buffers = nullptr;
    pipeline = pipeline_data.createPipeline(ctx, layout);
    bind_group = binding;
}

void vex::pf_demo::CellHeatmapV1::draw(
    const wgfx::RenderContext& ctx, const DrawContext& draw_ctx, const HeatmapDynamicData& args)
{
    UBOHeatMap vbo{
        .camera_vp = draw_ctx.camera_mvp,
        .data1 = args.color1,
        .data2 = args.color2,
        .data3 = {},
        .data4 = {},
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

bool vex::pf_demo::CellHeatmapV1::reloadShaders(
    vex::TextShaderLib& shader_lib, const wgfx::RenderContext& context)
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