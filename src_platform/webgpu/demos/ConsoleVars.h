#pragma once
#include <application/Platfrom.h> 

namespace vex
{
    // grid
    static inline const auto opt_grid_thickness = SettingsContainer::EntryDesc<i32>{
        .key_name = "pf.GridSize",
        .info = "Thickness of the grid. Value less than 2 disables the grid.",
        .default_val = 2,
        .min = 0,
        .max = 8,
        .flags = SettingsContainer::Flags::k_visible_in_ui,
    }; 
    static inline const auto opt_grid_color = SettingsContainer::EntryDesc<v4f>{
        .key_name = "pf.GridColor",
        .info = "Color of the grid.",
        .default_val = v4f(Color::gray()) * 0.7f,
        .flags = SettingsContainer::Flags::k_visible_in_ui,
    }; 
    // heatmap
    static inline const auto opt_heatmap_opacity = SettingsContainer::EntryDesc<float>{
        .key_name = "pf.heatmapOpacity",
        .info = "Heatmap opacity.",
        .default_val = 1.0f,
        .min = 0.00f,
        .max = 1.00f,
        .flags = SettingsContainer::Flags::k_visible_in_ui,
    }; 

    // particles
    static inline const auto opt_part_color = SettingsContainer::EntryDesc<v4f>{
        .key_name = "pf.ParticleColor",
        .info = "Color of a particle.",
        .default_val = v4f(Color::green()),
        .flags = SettingsContainer::Flags::k_visible_in_ui,
    };
    static inline const auto opt_part_auto_color = SettingsContainer::EntryDesc<bool>{
        .key_name = "pf.AutoParticleColor",
        .info = "Color of a particle determined by its generation.",
        .default_val = false,
        .flags = SettingsContainer::Flags::k_visible_in_ui,
    }; 
    static inline const auto opt_part_speed = SettingsContainer::EntryDesc<float>{
        .key_name = "pf.PtSym_FlowFieldForce",
        .info = "Base speed of a particle in units per second.",
        .default_val = 2.0f,
        .min = 0.00f,
        .max = 8.00f,
        .flags = SettingsContainer::Flags::k_visible_in_ui,
    };
    static inline const auto opt_part_speed_max = SettingsContainer::EntryDesc<float>{
        .key_name = "pf.PtSym_SpeedClamp",
        .info = "Max normalized speed value.",
        .default_val = 2.0f,
        .min = 0.00f,
        .max = 12.00f,
        .flags = SettingsContainer::Flags::k_visible_in_ui,
    };
    static inline const auto opt_part_inertia = SettingsContainer::EntryDesc<float>{
        .key_name = "pf.PtSym_Inertia",
        .info = "Constant that determins how quickly velocity may change.",
        .default_val = 1.00f,
        .min = 0.00f,
        .max = 2.00f,
        .flags = SettingsContainer::Flags::k_visible_in_ui,
    };
    static inline const auto opt_part_radius = SettingsContainer::EntryDesc<float>{
        .key_name = "pf.PtSym_PhysicalRadius",
        .info = "Radius of a particle relative to cell size.",
        .default_val = 0.25f,
        .min = 0.125f,
        .max = 0.5f,
        .flags = SettingsContainer::Flags::k_visible_in_ui |
                 SettingsContainer::Flags::k_ui_min_as_step,
    };
    static inline const auto opt_part_drag = SettingsContainer::EntryDesc<float>{
        .key_name = "pf.PtSym_Drag",
        .info = "How fast particle should deccelarate.",
        .default_val = 0.05f,
        .min = 0.00f,
        .max = 0.75f,
        .flags = SettingsContainer::Flags::k_visible_in_ui,
    };
    static inline const auto opt_part_sep = SettingsContainer::EntryDesc<float>{
        .key_name = "pf.PtSym_Separation",
        .info = "Force that pushes particles away when they get too close.",
        .default_val = 0.05f,
        .min = 0.00f,
        .max = 0.75f,
        .flags = SettingsContainer::Flags::k_visible_in_ui,
    };


    static inline const auto opt_show_dbg_overlay = SettingsContainer::EntryDesc<bool>{
        .key_name = "pf.NeighborNumOverlay",
        .info = "Show or hide overlay that shows number of neighbors",
        .default_val = false,
        .flags = 0,
    };
    static inline const auto opt_show_ff_overlay = SettingsContainer::EntryDesc<bool>{
        .key_name = "pf.FlowDirOverlay",
        .info = "Show or hide flow field overlay",
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
        .key_name = "pf.ShowCostOnSmallMaps",
        .info = "Show or hide movement cost",
        .default_val = false,
        .flags = SettingsContainer::Flags::k_visible_in_ui,
    };
    static inline const auto opt_wallbias_numbers = SettingsContainer::EntryDesc<bool>{
        .key_name = "pf.WallBias",
        .info = "Walls will push flow vectors away a bit",
        .default_val = true,
        .flags = SettingsContainer::Flags::k_visible_in_ui,
    };
} // namespace vex