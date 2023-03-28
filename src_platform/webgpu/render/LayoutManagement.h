#pragma once

#include <VFramework/VEXBase.h>
#include <gfx/GfxUtils.h>
#include <webgpu/render/WgpuTypes.h>


namespace wgfx
{
    template <typename T>
    struct VertLayout;
    template <>
    struct VertLayout<vex::PosNormUv>
    {
        using Vert = vex::PosNormUv;
        static constexpr WGPUVertexAttribute attributes[3] = {
            {
                .format = WGPUVertexFormat_Float32x3,
                .offset = offsetof(Vert, pos),
                .shaderLocation = 0,
            },
            {
                .format = WGPUVertexFormat_Float32x3,
                .offset = offsetof(Vert, norm),
                .shaderLocation = 1,
            },
            {
                .format = WGPUVertexFormat_Float32x2,
                .offset = offsetof(Vert, tex),
                .shaderLocation = 2,
            },
        };
        static constexpr WGPUVertexBufferLayout buffer_layout = {
            .arrayStride = sizeof(Vert),
            .stepMode = WGPUVertexStepMode_Vertex,
            .attributeCount = std::size(attributes),
            .attributes = attributes,
        };
    };
    template <>
    struct VertLayout<vex::PosNormColor>
    {
        using Vert = vex::PosNormColor;
        static constexpr WGPUVertexAttribute attributes[3] = {
            {
                .format = WGPUVertexFormat_Float32x3,
                .offset = offsetof(Vert, pos),
                .shaderLocation = 0,
            },
            {
                .format = WGPUVertexFormat_Float32x3,
                .offset = offsetof(Vert, norm),
                .shaderLocation = 1,
            },
            {
                .format = WGPUVertexFormat_Float32x4,
                .offset = offsetof(Vert, color),
                .shaderLocation = 2,
            },
        };
        static constexpr WGPUVertexBufferLayout buffer_layout = {
            .arrayStride = sizeof(Vert),
            .stepMode = WGPUVertexStepMode_Vertex,
            .attributeCount = std::size(attributes),
            .attributes = attributes,
        };
    };
    struct EmptyVertex
    {
    };
    template <>
    struct VertLayout<EmptyVertex>
    {
        using Vert = EmptyVertex;
        static constexpr WGPUVertexBufferLayout buffer_layout = {
            .stepMode = WGPUVertexStepMode_VertexBufferNotUsed,
            .attributeCount = 0,
            .attributes = nullptr,
        };
    };

    struct BGLayoutBuilder
    {
        vex::Buffer<WGPUBindGroupLayoutEntry> entries;

        inline BGLayoutBuilder& addUniform(u32 min_size,
            WGPUShaderStageFlags visibility = WGPUShaderStage_Vertex, bool dyn_offset = false)
        {
            entries.add(WGPUBindGroupLayoutEntry{
                .binding = (u32)entries.size(),
                .visibility = visibility,
                .buffer =
                    {
                        .type = WGPUBufferBindingType_Uniform,
                        .hasDynamicOffset = dyn_offset,
                        .minBindingSize = min_size,
                    },
            });
            return *this;
        }
        inline BGLayoutBuilder& addTexView(bool multi_sample = false)
        {
            entries.add(WGPUBindGroupLayoutEntry{
                .binding = (u32)entries.size(),
                .visibility = WGPUShaderStage_Fragment,
                .texture =
                    {
                        .sampleType = WGPUTextureSampleType_Float,
                        .viewDimension = WGPUTextureViewDimension_2D,
                        .multisampled = multi_sample,
                    },
            });
            return *this;
        }
        inline BGLayoutBuilder& addSampler(
            WGPUSamplerBindingType type = WGPUSamplerBindingType_Filtering)
        {
            entries.add(WGPUBindGroupLayoutEntry{
                .binding = (u32)entries.size(),
                .visibility = WGPUShaderStage_Fragment,
                .sampler = {.type = WGPUSamplerBindingType_Filtering},
            });
            return *this;
        }
        // will override binding
        inline BGLayoutBuilder& add(WGPUBindGroupLayoutEntry entry)
        {
            entry.binding = (u32)entries.size();
            entries.add(entry);
            return *this;
        }

        inline auto toDescriptor() const
        {
            check_(entries.size() && entries.data());
            return WGPUBindGroupLayoutDescriptor{
                .entryCount = (u32)entries.size(),
                .entries = entries.data(),
            };
        }
        inline auto createLayout(WGPUDevice device) const
        {
            auto desc = toDescriptor();
            return wgpuDeviceCreateBindGroupLayout(device, &desc);
        }
    };

    struct BGBuilder
    {
        vex::Buffer<WGPUBindGroupEntry> entries;

        inline BGBuilder add(const GpuBuffer& buf, u32 offset = 0)
        {
            entries.add(WGPUBindGroupEntry{
                .binding = (u32)entries.size(),
                .buffer = buf.buffer,
                .offset = offset,
                .size = buf.desc.size,
            });
            return *this;
        }
        inline BGBuilder add(WGPUTextureView view)
        {
            entries.add(WGPUBindGroupEntry{
                .binding = (u32)entries.size(),
                .textureView = view,
            });
            return *this;
        }
        inline BGBuilder add(WGPUSampler sampler)
        {
            entries.add(WGPUBindGroupEntry{
                .binding = (u32)entries.size(),
                .sampler = sampler,
            });
            return *this;
        }
        inline BGBuilder add(WGPUBindGroupEntry entry)
        {
            entry.binding = (u32)entries.size();
            entries.add(entry);
            return *this;
        }

        inline auto toDescriptor(WGPUBindGroupLayout bind_group_layout = nullptr) const
        {
            check_(entries.size() && entries.data());
            return WGPUBindGroupDescriptor{
                .layout = bind_group_layout,
                .entryCount = (u32)entries.size(),
                .entries = entries.data(),
            };
        }

        // if bind_group_layout is nullptr, then one will be created from entries
        inline auto createBindGroup(WGPUDevice device, WGPUBindGroupLayout bind_group_layout) const
        {
            check_(bind_group_layout);
            auto desc = toDescriptor(bind_group_layout);
            auto out = wgpuDeviceCreateBindGroup(device, &desc);
            WGPU_REL(BindGroupLayout, bind_group_layout);
            check_(out);
            return out;
        }
    };

    struct BGLCombinedBuilder
    {
        static constexpr auto def_tex_layout = WGPUTextureBindingLayout{
            .sampleType = WGPUTextureSampleType_Float,
            .viewDimension = WGPUTextureViewDimension_2D,
            .multisampled = false,
        };
        vex::Allocator al = vex::gMallocator;
        BGLayoutBuilder layout_builder{.entries = {al, 4}};
        BGBuilder group_builder{.entries = {al, 4}};

        inline BGLCombinedBuilder& addUniform(u32 min_size, const GpuBuffer& buf, u32 offset = 0,
            WGPUShaderStageFlags visibility = WGPUShaderStage_Vertex, bool dyn_offset = false)
        {
            layout_builder.add(WGPUBindGroupLayoutEntry{
                .visibility = visibility,
                .buffer =
                    {
                        .type = WGPUBufferBindingType_Uniform,
                        .hasDynamicOffset = dyn_offset,
                        .minBindingSize = min_size,
                    },
            });
            group_builder.add(buf);
            return *this;
        }

        inline BGLCombinedBuilder& addStorageBuffer(u32 min_size, const GpuBuffer& buf,
            WGPUShaderStageFlags visibility = WGPUShaderStage_Vertex, bool read_only = true)
        {
            layout_builder.add(WGPUBindGroupLayoutEntry{
                .visibility = visibility,
                .buffer =
                    {
                        .type = read_only ? WGPUBufferBindingType_ReadOnlyStorage
                                          : WGPUBufferBindingType_Storage,
                        //.hasDynamicOffset = dyn_offset,
                        .minBindingSize = min_size,
                    },
            });
            group_builder.add(buf);
            return *this;
        }
        inline BGLCombinedBuilder& addTexView(
            WGPUTextureView view, WGPUTextureBindingLayout tex_layout = def_tex_layout)
        {
            layout_builder.add(WGPUBindGroupLayoutEntry{
                .visibility = WGPUShaderStage_Fragment,
                .texture = tex_layout,
            });
            group_builder.add(view);
            return *this;
        }
        inline BGLCombinedBuilder& addSampler(WGPUSampler sampler)
        {
            layout_builder.add(WGPUBindGroupLayoutEntry{
                .visibility = WGPUShaderStage_Fragment,
                .sampler = {.type = WGPUSamplerBindingType_Filtering},
            });
            group_builder.add(sampler);
            return *this;
        }
        // will override binding
        inline BGLCombinedBuilder& add(
            WGPUBindGroupEntry bg_entry, WGPUBindGroupLayoutEntry layout_entry)
        {
            layout_builder.add(layout_entry);
            group_builder.add(bg_entry);
            return *this;
        }

        inline auto createLayoutAndGroup(WGPUDevice device) const
        {
            auto layout = layout_builder.createLayout(device);
            auto group = group_builder.createBindGroup(device, layout);
            check_(group);
            return vex::Tuple<WGPUBindGroupLayout, WGPUBindGroup>{layout, group};
        }
    };

    template <typename VertType>
    struct SimplePipeline
    {
        static constexpr const WGPUVertexBufferLayout vert_layout =
            VertLayout<VertType>::buffer_layout;
        WGPUPrimitiveState primitive_state = makeDefault<WGPUPrimitiveState>();
        WGPUColorTargetState color_target_state = makeDefault<WGPUColorTargetState>({
            .format = WGPUTextureFormat_RGBA8Unorm,
        });
        WGPUMultisampleState multisample_state = makeDefault<WGPUMultisampleState>();
        WGPUDepthStencilState depth_stencil_state = makeDefault<WGPUDepthStencilState>();

        WGPUVertexState vert_state = makeVertexState(nullptr, //
            VertShaderDesc{
                .from = ShaderOrigin::Undefined,
                .buffer_count = 1,
                .buffers = &vert_layout,
            });
        WGPUFragmentState frag_state = makeFragmentState(nullptr, //
            FragShaderDesc{.from = ShaderOrigin::Undefined,
                .target_count = 1,
                .targets = &color_target_state});

        WGPURenderPipelineDescriptor pipeline_desc = {
            .vertex = vert_state,
            .primitive = primitive_state,
            .depthStencil = &depth_stencil_state,
            .multisample = multisample_state,
            .fragment = &frag_state,
        };

        inline void setShader(WGPUShaderModule module)
        {
            vert_state.module = module;
            frag_state.module = module;
        }

        WGPURenderPipeline createPipeline(const GpuContext& context, WGPUBindGroupLayout layout)
        {
            WGPUPipelineLayoutDescriptor pl_desc{
                .bindGroupLayoutCount = 1,
                .bindGroupLayouts = &layout,
            };
            auto pipeline_layout = wgpuDeviceCreatePipelineLayout(context.device, &pl_desc);
            check_(pipeline_layout);
            pipeline_desc.vertex = vert_state;
            pipeline_desc.layout = pipeline_layout;
            auto pl = wgpuDeviceCreateRenderPipeline(context.device, &pipeline_desc);
            check_(pl);
            return pl;
        }
    };
    struct ComputePipeline
    {
        const char* label = "compute";
        WGPUProgrammableStageDescriptor descriptor{
            .nextInChain = nullptr,
            .module = nullptr,
            .entryPoint = "cs_main",
            .constantCount = 0,
            .constants = nullptr,
        }; 

        inline WGPUComputePipeline createPipeline(
            const GpuContext& context, WGPUShaderModule shader, WGPUBindGroupLayout layout)
        {
            WGPUPipelineLayoutDescriptor pl_desc{
                .bindGroupLayoutCount = 1,
                .bindGroupLayouts = &layout,
            };
            auto pipeline_layout = wgpuDeviceCreatePipelineLayout(context.device, &pl_desc);
            check_(pipeline_layout);
            descriptor.module  = shader;
            check_(descriptor.module);
            WGPUComputePipelineDescriptor pipeline_desc{
                .label = label,
                .layout = pipeline_layout,
                .compute = descriptor,
            };
            auto pl = wgpuDeviceCreateComputePipeline(context.device, &pipeline_desc);
            check_(pl);
            return pl;
        }
    };

} // namespace wgfx
