#pragma once

#include <VFramework/VEXBase.h>
#include <application/Application.h>
#include <application/Platfrom.h>
#include <webgpu/demos/ViewportHandler.h>
#include <webgpu/render/WgpuApp.h>

#include "GpuResources.h" 

namespace vex::flow
{
    namespace ids
    {
        static inline const char* main_vp_name = "MainViewport";
        static inline const char* debug_vp_name = "Debug Layer";

        // static inline const char* setting_grid_sz = "pf.grid_size";
    } // namespace ids

    static inline const auto opt_show_dbg_overlay = SettingsContainer::EntryDesc<bool>{
        .key_name = "pf.NeighborNumOverlay",
        .info = "Show or hide overlay that shows number of neighbors",
        .default_val = false,
        .flags = 0,
    };
    static inline const auto opt_allow_diagonal = SettingsContainer::EntryDesc<bool>{
        .key_name = "pf.AllowDiagonalMovement",
        .info = "Show or hide overlay that shows number of neighbors",
        .default_val = true,
        .flags = 0,
    };
    static inline const auto opt_show_numbers = SettingsContainer::EntryDesc<bool>{
        .key_name = "pf.ShowCost",
        .info = "Show or hide movement cost",
        .default_val = true,
        .flags = SettingsContainer::Flags::k_visible_in_ui,
    };

    struct ProcessedData
    {
        static constexpr u32 dist_mask = ~(1 << 15);
        // grid 8b+8b flow vector, 15b distance so far, 1b mask (16th) for blocked
        vex::Buffer<u32> data;
        v2i32 size{0, 0};
        bool contains(v2u32 index) const { return index.x < size.x && index.y < size.y; }
        FORCE_INLINE u32& operator[](v2u32 index) { return data[index.y * size.x + index.x]; }
        FORCE_INLINE u32& operator[](v2i32 index) { return data[index.y * size.x + index.x]; }
        FORCE_INLINE u32& operator[](i32 offset) { return *(data.first + offset); }
        FORCE_INLINE u32 at(i32 offset) { return *(data.first + offset); }
        FORCE_INLINE u32& atRef(i32 offset) { return *(data.first + offset); }
    };

    struct Flow
    {
        static constexpr u8 mask_top = 0b0000'0001;
        static constexpr u8 mask_top_right = 0b0000'0010;
        static constexpr u8 mask_right = 0b0000'0100;
        static constexpr u8 mask_bot_right = 0b0000'1000;
        static constexpr u8 mask_bot = 0b0001'0000;
        static constexpr u8 mask_bot_left = 0b0010'0000;
        static constexpr u8 mask_left = 0b0100'0000;
        static constexpr u8 mask_top_left = 0b1000'0000;

        static constexpr u8 i_top = 0;
        static constexpr u8 i_top_right = 1;
        static constexpr u8 i_right = 2;
        static constexpr u8 i_bot_right = 3;
        static constexpr u8 i_bot = 4;
        static constexpr u8 i_bot_left = 5;
        static constexpr u8 i_left = 6;
        static constexpr u8 i_top_left = 7;
        struct Args
        {
            v2u32 start{0, 0};
        };


        struct Map1b
        {
            static void fromImage(Flow::Map1b& out, const char* img);
            // neighbors as bitmask, starting at 1 as Top and going clockwise (e.g.
            // top+right:00000101 zero means blocked
            vex::Buffer<u8> source;
            vex::Buffer<u8> matrix;       // neighbor matrix
            vex::Buffer<u32> debug_layer; // 1st byte is the same as in matrix
            v2u32 size{0, 0};

            bool contains(v2u32 index) const { return index.x < size.x && index.y < size.y; }

            bool isBlocked(v2u32 index) const { return source[index.y * size.x + index.x] == 0; }

            FORCE_INLINE u8* cellMaskPtr(v2u32 cell)
            {
                return matrix.first + (cell.y) * size.x + cell.x;
            }
            FORCE_INLINE u8 cellMask(v2u32 cell) { return *(cellMaskPtr(cell)); }
            FORCE_INLINE u8 cellMask(u32 x, u32 y) { return *(cellMaskPtr({x, y})); }
            FORCE_INLINE u8 cellMask(u32 offset) const { return *(matrix.first + offset); }
        };

        template <bool allow_diagonal = false>
        inline static void gridSyncBFS(Args args, const Map1b& grid, ProcessedData& out)
        {
            constexpr u8 diag_mask = allow_diagonal ? 0xff : 0b01010101;
            struct OffsetTableVal
            {
                i16 mul = 0;
                i16 offset = 0;
            };
            // struct
            vex::StaticRing<i32, 2024, true> frontier;
            out.size = grid.size;
            out.data.reserve(grid.size.x * grid.size.y);
            out.data.len = 0;
            for (u8 c : grid.source)
                out.data.add(c ? 0 : ~ProcessedData::dist_mask);

            [[maybe_unused]] u32 iter = 0;

            const i32 neighbor_offsets[8] = {
                // only 4 would be used in case grid does not allow diag move
                -(i32)grid.size.x + 0, // top (CW sart)
                -(i32)grid.size.x + 1, // top-right
                /*same row       */ 1, // right
                +(i32)grid.size.x + 1, // bot-right
                +(i32)grid.size.x + 0, // bot
                +(i32)grid.size.x - 1, // bot-left
                /*same row      */ -1, // left
                -(i32)grid.size.x - 1, // top-left
            };

            const auto start_cell = args.start.y * grid.size.x + args.start.x;
            frontier.push(start_cell);
            out[start_cell] = 0;

            constexpr auto dist_mask = ProcessedData::dist_mask;
            while (frontier.size() > 0)
            {
                i32 current = (i32)frontier.dequeueUnchecked();
                const u8 cell = grid.cellMask(current)  & diag_mask;
                u32 dist_so_far = out[current];
                for (u8 i = 0; (i < 8) && cell; ++i) 
                {
                    u8 cur_i = ((cell & (1u << i)) > 0);
                    if (cur_i)
                    {
                        u32 next = current + neighbor_offsets[i]; 
                        // checkAlways_(next < grid.size.x * grid.size.y);
                        bool visited = (out.at(next) & dist_mask) > 0;
                        if (visited || (next == start_cell))
                            continue;
                        out[next] |= dist_so_far + 1; // add diagonal cost
                        frontier.push(next);
                    }
                }
            }
        }
    };


    struct FlowfieldPF : public IDemoImpl
    {
        struct InitArgs
        {
        };
        void init(Application& owner, InitArgs args);
        void update(Application& owner) override;
        void drawUI(Application& owner) override;
        void postFrame(Application& owner) override;

        virtual ~FlowfieldPF();

    private:
        // void drawMainViewport(Application& owner);
        // void drawDebugViewport(Application& owner);

        wgfx::ui::ViewportHandler viewports;
        wgfx::ui::BasicDemoUI ui;
        vex::InlineBufferAllocator<2 * 1024 * 1024> frame_alloc_resource;
        // bump allocator that resets each frame
        vex::Allocator frame_alloc = frame_alloc_resource.makeAllocatorHandle();
        bool docking_not_configured = true;

        Flow::Map1b init_data;
        ProcessedData processed_map;

        ColorQuad background;
        TempGeometry temp_geom;
        ViewportGrid view_grid;
        CellHeatmapV1 heatmap;
        CellHeatmapV1 debug_overlay;

        ComputeFields compute_pass;
        FlowFieldsOverlay flow_overlay;

        WebGpuBackend* wgpu_backend = nullptr;

        v2u32 goal_cell = { 10, 10 };

        struct
        {
            v4f grid_color = Color::gray();
            bool options_shown = false;
        } gui_options;

        // to be able to tie some initialization code with cleanup, that needs to capture stuff
        // like Application& or gfx or other resource that outlives Demo
        std::vector<std::function<void()>> defer_till_dtor;
    };
} // namespace vex::flow