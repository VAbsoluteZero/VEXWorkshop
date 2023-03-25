#pragma once

#include <VFramework/VEXBase.h>
#include <application/Application.h>
#include <application/Platfrom.h>
#include <webgpu/demos/ViewportHandler.h>
#include <webgpu/render/WgpuApp.h>

#include "GpuResources.h"
#include "PFState.h"

namespace vex::flow
{
    namespace ids
    {
        static inline const char* main_vp_name = "MainViewport";
        static inline const char* debug_vp_name = "Debug Layer";

        static inline const char* setting_grid_sz = "pf.grid_size";
    } // namespace ids

    // static inline const auto opt_grid_enabled = SettingsContainer::EntryDesc<bool>{
    //     .key_name = "pf.grid.enabled",
    //     .info = "Show or hide grid",
    //     .default_val = 4u,
    // };

    struct MapGrid_v1
    {
        // 255 => blocked time, 0 free tile, in-between move cost
        vex::Buffer<u32> array;
        v2i32 size{0, 0};
        bool contains(v2u32 index) const { return index.x < size.x && index.y < size.y; }
        FORCE_INLINE u32& operator[](v2u32 index) { return array[index.y * size.x + index.x]; }
        FORCE_INLINE u32& operator[](v2i32 index) { return array[index.y * size.x + index.x]; }
        FORCE_INLINE u32* at(v2i32 index) { return &array[index.y * size.x + index.x]; }
        FORCE_INLINE u32* at(i32 x, i32 y) { return &array[y * size.x + x]; }
        FORCE_INLINE u32* atOffset(u32 byte_offset) { return (array.data() + byte_offset); }
        FORCE_INLINE u32* row(i32 row) { return &array[row * size.x]; }
    };
    struct InitData_v1
    {
        MapGrid_v1 grid;
        v2u32 initial_goal;
        v2u32 spawn_location;
    };

    template <bool k_diagonal = false>
    struct GridMap1b
    {
        static constexpr u32 min_row_size = 8;
        // grid 8b+8b flow vector, 15b distance so far, 1b mask (16th) for blocked
        // neighbors as bitmask, starting at 1 as Top and going clockwise (e.g. top+left:00000101
        // zero means blocked
        vex::Buffer<u8> matrix; // neighbor matrix
        v2u32 size{0, 0};

        FORCE_INLINE u32* getData(v2u32 cell)
        {
            return data_full.first + (cell.y) * size.x + cell.x;
        }
        FORCE_INLINE u32* getData(u32 x, u32 y) { return getData({x, y}); }
        FORCE_INLINE u32* getData(u32 offset) { return data_full.first + offset; }


        FORCE_INLINE u32* matrixCell(v2u32 cell)
        {
            return matrix.first + (cell.y) * size.x + cell.x;
        }
        FORCE_INLINE u8* matrixCell(u32 offset) { return matrix.first + offset; }
    };

    struct GridFlowGen
    {
        struct Args
        {
            v2u32 start{0, 0};
        };

        template <bool k_diagonal = false>
        void BFS(Args args, GridMap1b<k_diagonal>& grid)
        {
            vex::InlineBufferAllocator<2096> temp_alloc_resource;
            auto al = temp_alloc_resource.makeAllocatorHandle();

            vex::StaticRing<v2i32, 2024, true> stack;
            vex::Buffer<v2u32> neighbors{al, 64};

            struct HS
            {
                inline static i32 hash(v2u32 key) { return (int)(key.x << 16 | key.y); }
                inline static bool is_equal(v2u32 a, v2u32 b) { return a.x == b.x && a.y == b.y; }
            };
            Dict<v2u32, float, HS> previous{1024};

            stack.push(start);
            u32 iter = 0;
            previous[start] = 0;
            while (stack.size() > 0)
            {
                auto current = stack.popUnchecked();
                // bool added_new = false;

                neighbors.len = 0;
                neighbors.add(current - v2i32{-1, 0});
                neighbors.add(current - v2i32{1, 0});
                neighbors.add(current - v2i32{0, 1});
                neighbors.add(current - v2i32{0, -1});

                for (int i = 0; i < neighbors.len; ++i)
                {
                    iter++;
                    auto next = neighbors[i];
                    u32 tile = *(grid.at(next)) & 0x000000ff;
                    bool can_move = tile != 255;

                    if (!can_move)
                        continue;
                    if (next.x >= grid.size.x || next.y >= grid.size.y)
                        continue;

                    i32 dist_so_far = 1;
                    if (previous.contains(current))
                    {
                        dist_so_far += previous[current];
                    }
                    if (!previous.contains(next) || previous[next] > dist_so_far)
                    {
                        // added_new = true;
                        stack.push(next);
                        previous[next] = dist_so_far;
                    }
                }
            }

            for (auto& [k, d] : previous)
            {
                grid[k] &= 0x000000ff;
                grid[k] |= (~0x000000ffu) & (((u32)d) << 8);
            }
            owner.input.ifTriggered("EscDown"_trig,
                [&](const input::Trigger& self)
                {
                    std::string result = "map:\n";
                    result.reserve(4096 * 4);
                    auto inserter = std::back_inserter(result);
                    for (i32 i = 0; i < grid.array.len; ++i)
                    {
                        fmt::format_to(
                            inserter, "{:2x} ", (((u8)0xffffff00u | *(grid.atOffset(i))) >> 8));
                        if ((i + 1) % grid.size.x == 0)
                            fmt::format_to(inserter, "\n");
                    }
                    spdlog::info(result);
                    return true;
                });
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
        wgfx::ui::ViewportHandler viewports;
        wgfx::ui::BasicDemoUI ui;
        vex::InlineBufferAllocator<2 * 1024 * 1024> frame_alloc_resource;
        // bump allocator that resets each frame
        vex::Allocator frame_alloc = frame_alloc_resource.makeAllocatorHandle();
        bool docking_not_configured = true;

        InitData_v1 init_data{};

        ColorQuad background;
        TempGeometry temp_geom;
        ViewportGrid view_grid;
        CellHeatmapV1 heatmap;

        WebGpuBackend* wgpu_backend = nullptr;

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