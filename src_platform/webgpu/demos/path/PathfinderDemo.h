#pragma once

#include <VFramework/VEXBase.h>
#include <application/Application.h>
#include <application/Platfrom.h>
#include <webgpu/demos/ViewportHandler.h>
#include <webgpu/render/WgpuApp.h>

#include "GpuResources.h"
#include "PFState.h"

namespace vex::pf_demo
{
    namespace ids
    {
        static inline const char* main_vp_name = "MainViewport";
        static inline const char* debug_vp_name = "Debug Layer";
    } // namespace ids

    struct MapGrid
    {
        // 255 => blocked time, 0 free tile, in-between move cost
        vex::Buffer<u32> array;
        v2i32 size{ 0, 0 };     
        bool contains(v2u32 index) const
        {
            return index.x < size.x && index.y < size.y;
        }
        FORCE_INLINE u32& operator[](v2u32 index) { return array[index.y * size.x + index.x]; }
        FORCE_INLINE u32& operator[](v2i32 index) { return array[index.y * size.x + index.x]; }
        FORCE_INLINE u32* at(v2i32 index) { return &array[index.y * size.x + index.x]; }
        FORCE_INLINE u32* at(i32 x, i32 y) { return &array[y * size.x + x]; }
        FORCE_INLINE u32* atOffset(u32 byte_offset) { return (array.data() + byte_offset); }
        FORCE_INLINE u32* row(i32 row) { return &array[row * size.x]; }
    };
    struct InitData
    {
        MapGrid grid;
        v2u32 initial_goal;
        v2u32 spawn_location;
    };

    struct PathfinderDemo : public IDemoImpl
    {
        struct InitArgs
        {
        };
        void init(Application& owner, InitArgs args);
        void update(Application& owner) override;
        void drawUI(Application& owner) override;
        void postFrame(Application& owner) override;

        virtual ~PathfinderDemo();

    private:
        wgfx::ui::ViewportHandler viewports;
        wgfx::ui::BasicDemoUI ui;
        vex::InlineBufferAllocator<2 * 1024 * 1024> frame_alloc_resource;
        // bump allocator that resets each frame
        vex::Allocator frame_alloc = frame_alloc_resource.makeAllocatorHandle();
        bool docking_not_configured = true;

        InitData init_data{};

        ColorQuad background;
        TempGeometry temp_geom;
        ViewportGrid view_grid;
        CellHeatmapV1 heatmap;

        WebGpuBackend* wgpu_backend = nullptr;
    };
} // namespace vex::pf_demo