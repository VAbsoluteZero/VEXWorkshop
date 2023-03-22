#pragma once
#include <VCore/Containers/Array.h>
#include <VCore/Containers/Dict.h>
#include <VCore/Memory/Memory.h>
#include <VFramework/VEXBase.h>
#include <application/Platfrom.h>

// #todo replace with thin type
#include <functional>

namespace vex::input
{
    enum class SignalId : u8
    {
        sInvalid,
        MouseMove = 1,
        MouseLBK,
        MouseRBK,
        MouseMID,
        KeySpace,
        KeyEscape,
        KeyW,
        KeyA,
        KeyS,
        KeyD,

        KeyModAlt,
        KeyModShift,
        KeyModCtrl,
        // KeyAnykey,

        sMax,
    };
    enum class SignalState : u8
    {
        None,
        Started,  // down
        Finished, // up
        Going,    // held
        Canceled, // interrupted / lost focus
    };
    struct SignalData
    {
        enum class Flag : u8
        {
            None,
            Active
        };
        SignalId id = SignalId::sInvalid;
        Flag flag = Flag::None;
        SignalState state = SignalState::None;
        float time_active = 0;
        v2f value_raw = v2f_zero;

        inline constexpr float magnitudeSquared() const
        {
            return value_raw.x * value_raw.x + value_raw.y * value_raw.y;
        }
        inline constexpr bool asBool() const { return magnitudeSquared() > vex::epsilon_half; }
        inline constexpr float asFloat() const { return value_raw.x; }
        inline constexpr v2f asVec2f() const { return value_raw; }
        inline constexpr bool isActive() const { return Flag::Active == flag; }
        inline constexpr bool isInactive() const { return Flag::None == flag; }

        inline constexpr bool ia_startedOrGoing() const
        {
            return SignalState::Going == state || SignalState::Started == state;
        }
        inline constexpr bool ia_started() const
        {
            return  SignalState::Started == state;
        }
    };
    struct InputState
    {
        vex::Buffer<SignalData> prev_frame;
        vex::Buffer<SignalData> this_frame;
    };
    struct PreProcessOptions
    {
        float min_required = vex::epsilon_half;
        inline SignalData::Flag prepareValue(v2f value_raw = v2f_zero)
        {
            const auto magSq = value_raw.x * value_raw.x + value_raw.y * value_raw.y;
            return magSq > min_required ? SignalData::Flag::Active : SignalData::Flag::None;
        }
    };
    // #todo add post prepareValue for 'hold for N sec' triggers
    typedef bool (*processDelegate)(struct Trigger&, const InputState&);

    // #todo add rules for consuming events
    struct Trigger
    {
        // trigger may or may not write here
        vex::Dict<SignalId, SignalData> input_data{4};
        // can be used by logic callback to 'store' state in given N bytes
        vex::InlineBufferAllocator<64> local_storage;
        processDelegate fn_logic;
        bool triggered_this_frame = false;
        bool handled = false;
        bool storage_used = false;
        bool callProcessDelegate(Trigger& self, const InputState& state)
        {
            return fn_logic ? fn_logic(self, state) : false;
        }
    };
    struct InputSystem
    {
        static constexpr auto signal_type_size = sizeof(SignalData);
        static constexpr auto trigger_type_size = sizeof(Trigger);
        InputSystem();
        ~InputSystem() = default;
        void poll(float dt);
        void updateTriggers();
        void onFrameEnd();

        [[nodiscard]] inline bool is_triggered(const char* trigger)
        {
            Trigger* tg = triggers.find(util::fnv1a<true>(trigger));
            return tg ? tg->triggered_this_frame : false;
        }
        [[nodiscard]] inline bool is_triggered(u64 hash)
        {
            Trigger* tg = triggers.find(hash);
            return tg ? tg->triggered_this_frame : false;
        }
        template <typename Callable, bool checked = true>
        inline void if_triggered(u64 hash, Callable&& callable)
        {
            Trigger* tg = triggers.find(hash);
            if constexpr (checked)
            {
                check(tg, "trigger not found");
            }
            if (tg && tg->triggered_this_frame)
            {
                const Trigger* c_tg = tg;
                tg->handled = callable(*c_tg);
            }
        }
        struct
        {
            v2i32 mouse_delta{0, 0};
            v2i32 mouse_pos_window{};
            v2i32 mouse_pos_global{};

            std::function<void()> quit_delegate;
            std::function<void()> resize_delegate;
        } global;

        inline bool removeTrigger(u64 hash) { return triggers.remove(hash); }
        inline bool addTrigger(const char* name, const Trigger& trigger)
        {
            return addTrigger(util::fnv1a<true>(name), trigger);
        }
        bool addTrigger(u64 hash, const Trigger& trigger, bool replace = false);

        vex::Dict<u64, Trigger> triggers;
        InputState state;
        PreProcessOptions default_prep;
    };

    constexpr auto operator"" _trig(const char* cstr, u64 len)
    {
        return util::fnv1a<true>((u8*)cstr, len);
    }
} // namespace vex::input