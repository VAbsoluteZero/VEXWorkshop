#pragma once
#include <VCore/Containers/Dict.h>
#include <VFramework/VEXBase.h>

#include "SDLWindow.h"

#define ENABLE_IMGUI 1
namespace vex
{
    class Application;
    using tWindow = SDLWindow;
    struct StartupConfig
    {
        WindowParams WindowArgs;
        i32 target_framerate = -1;
        bool allow_demo_changes = false;
    };

    enum class LayersBaseOrder : u64
    {
        Earliest = 1000,
        Ealry = 2000,
        PreDefault = 3000,
        Default = 6000,
        PostDefault = 9000,
        Late = 12000,
        Latest = 14000
    };
    using DynVal = vex::Union<i32, f32, bool, v3f, v2i32, const char*, std::string>;
    using OptNumValue = vex::Union<i32, f32, bool, v4f>;

    enum class GfxBackendID : u8
    {
        Invalid = 0,
        Dx11_legacy,
        Dx11,
        Webgpu,
        // Dx12
    };

    struct AGraphicsBackend
    {
        GfxBackendID id;

        virtual i32 init(Application& owner) = 0;
        virtual void startFrame(Application& owner) {}
        virtual void frame(Application& owner) {}
        virtual void postFrame(Application& owner) {}
        // softReset is a 'reset/clear state', teardown is to destroy inner state fully
        // including resources
        virtual void softReset(Application& owner) {}
        virtual void teardown(Application& owner) {}
        virtual void handleWindowResize(Application& owner, v2u32 size){};

        virtual ~AGraphicsBackend() {}
    };

    struct IDemoImpl
    {
        virtual void update(Application& owner) = 0;
        virtual void postFrame(Application& owner) {}
        virtual void drawUI(Application& owner) {}
        virtual ~IDemoImpl() {}
    };

    // #todo - add scoped listener, that would auto remove setting (enable_shared_from_this?)
    struct SettingsContainer
    {
        struct Flags // predefined flags
        {
            static constexpr u32 k_visible_in_ui = 0x0000'0001u;
            static constexpr u32 k_ui_min_as_step = 0x0000'0002u;
        };
        struct Entry
        {
            const char* info = nullptr;
            OptNumValue value;
            OptNumValue min; // #todo limited support at least.
            OptNumValue max; // only make sense if both limist are set
            u32 flags = 0;
            u32 current_version = 0; // used as 'dirty flag' variant

            template <typename T>
            void setValue(T val)
            { 
                OptNumValue& opt_min = this->min;
                OptNumValue& opt_mix = this->max;
                if (T* stored_val = this->value.find<std::decay_t<T>>(); nullptr != stored_val)
                {
                    if (T* min = opt_min.find<std::decay_t<T>>(); nullptr != min)
                        val = val < *min ? *min : val;
                    if (T* max = opt_mix.find<std::decay_t<T>>(); nullptr != max)
                        val = val > *max ? *max : val;
                    *stored_val = val;
                }
            }
        };
        template <typename T>
        struct EntryDesc
        {
            const char* key_name = nullptr;
            const char* info = nullptr;
            T default_val;
            vex::Option<T> min;
            vex::Option<T> max;
            u32 flags = 0;

            void addTo(SettingsContainer& container) const
            {
                Entry ent{
                    .info = nullptr,
                    .value = default_val,
                    .flags = flags,
                };
                if (min.hasAnyValue())
                    ent.min = min.template get<T>();
                if (max.hasAnyValue())
                    ent.max = max.template get<T>();
                container.addSetting(key_name, std::move(ent));
            }
            void removeFrom(SettingsContainer& container) const
            {
                container.removeSetting(key_name);
            }
        };

        struct VersionFilter
        {
            std::string key;
            u32 last_observed_version = 0;
            template <typename T, typename TCallable>
            FORCE_INLINE void ifHasValue(SettingsContainer& container, TCallable&& callable)
            {
                last_observed_version = container.ifHasValue<T>(
                    key, callable, last_observed_version);
            }
            template <typename T>
            T valueOr(SettingsContainer& container, std::string_view key, T alternative)
            {
                return container.valueOr<T>(key, alternative, &last_observed_version);
            }
        };

        static constexpr auto k_ent_size = sizeof(Entry);
        vex::Dict<std::string, Entry> settings; // #todo replace with small string

        template <typename T, bool k_checked = false>
        bool setValue(std::string_view key, T val)
        {
            Entry* ent = settings.find(key);
            if (k_checked && checkAlways(ent != nullptr, "Entry does not exist in settings"))
            {
                return false;
            }
            OptNumValue& opt_val = ent->value;
            OptNumValue& opt_min = ent->min;
            OptNumValue& opt_mix = ent->max;
            if (T* stored_val = opt_val.find<std::decay_t<T>>(); nullptr != stored_val)
            {
                if (T* min = opt_val.find<std::decay_t<T>>(); nullptr != min)
                    val = val < *min ? *min : val;
                if (T* max = opt_val.find<std::decay_t<T>>(); nullptr != max)
                    val = val > *max ? *max : val;
                *stored_val = val;
            }
            return false;
        }

        // must_be_newer_than is optional filter to ignore unchanging values
        // returns current version id
        template <typename T, typename TCallable>
        u32 ifHasValue(std::string_view key, TCallable&& callable, u32 must_be_newer_than = 0)
        {
            constexpr bool v_is_supported = std::is_same_v<T, i32> || std::is_same_v<T, f32> ||
                                            std::is_same_v<T, bool>;
            static_assert(v_is_supported, "type is not supported");

            Entry* ent = settings.find(key);
            bool is_set = ent != nullptr;
            if (!checkAlways(is_set, "Entry does not exist in settings"))
            {
                return 0;
            }
            const auto version = ent->current_version;
            if (version >= must_be_newer_than)
            {
                if (T* stored_val = ent->value.find<std::decay_t<T>>(); nullptr != stored_val)
                {
                    callable(*stored_val);
                    return version;
                }
            }
            return version;
        }
        template <typename T>
        T valueOr(std::string_view key, T alternative, u32* out_current_version = nullptr)
        {
            constexpr bool v_is_supported = OptNumValue::mayHave<T>();
            static_assert(v_is_supported, "type is not supported");

            Entry* ent = settings.find(key);
            if (ent == nullptr)
            {
                return alternative;
            }
            if (T* stored_val = ent->value.find<std::decay_t<T>>(); nullptr != stored_val)
            {
                if (out_current_version)
                    *out_current_version = ent->current_version;
                return *stored_val;
            }
            return alternative;
        }

        template <typename T>
        SettingsContainer& addSetting(std::string key, T default_val, const char* info = nullptr)
        {
            constexpr bool v_is_supported = std::is_same_v<T, i32> || std::is_same_v<T, f32> ||
                                            std::is_same_v<T, bool>;
            static_assert(v_is_supported, "type is not supported");

            Entry ent{
                .info = info,
                .value = default_val,
            };
            settings.emplaceAndGet(std::move(key), std::move(ent));
            return *this;
        }

        SettingsContainer& addSetting(std::string key, Entry entry)
        {
            settings.emplaceAndGet(std::move(key), std::move(entry));
            return *this;
        }

        void removeSetting(std::string_view key) { settings.remove(key); }
    };

    struct DemoSamples
    {
        typedef IDemoImpl* (*CreateDemoCallback)(Application& owner);
        struct Entry
        {
            const char* readable_name;
            const char* description;
            CreateDemoCallback callback;
        };
        vex::Dict<const char*, Entry> demos;

        template <typename Func>
        void add(const char* k, const char* name, const char* desc, Func&& t)
        {
            static auto callback = t;
            demos.emplace(k, Entry{name, desc, callback});
        }
    };

} // namespace vex

// #define VEX_register_demo(x, y) static inline vex::AddDemo vpint_COMBINE(reg_demo_anon_,
// __LINE__)(x, y);