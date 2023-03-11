#pragma once
#include <VCore/Containers/Union.h>
#include <VCore/Utils/VMath.h>
#include <VFramework/VEXBase.h>

#include "Platfrom.h" 

namespace vp
{
    struct FramerateArgs
    {
        static constexpr i32 k_max_allowed = 4096 * 16;
        i32 target_framerate = -1;

        u32 frame_number = 0;

        inline bool fpsUnconstrained() const { return target_framerate < 0; }
        inline u32 desiredFramerate() const
        {
            if (fpsUnconstrained())
                return (u32)k_max_allowed;
            return (u32)target_framerate;
        }
        inline auto framerateClamped(i32 Min = 8, i32 Max = k_max_allowed)
        {
            return glm::clamp<i32>(desiredFramerate(), Min, Max);
        }
    }; 
    /*
     * Scaled time exists for situations when whole simulation runs with time multiplier.
     * It is not to meant to handle any game logic, only naive cases and debug.
     */
    struct FrameTime
    {
        double unscaled_runtime = 0;

        double unscaled_dt = 0;
        double dt = 0; 
        float unscalled_dt_f32 = 0;
        float delta_time_f32 = 0;

        inline const void setTimeMutlipier(double value) { mult = (value < 0 ? 0 : value); }
        inline const double getTimeMutlipier() { return mult; }

        inline void update(double dt_sec)
        {
            unscaled_dt = dt_sec;
            dt = dt_sec * mult;

            unscalled_dt_f32 = (f32)dt_sec;
            delta_time_f32 = (f32)dt;

            unscaled_runtime += dt_sec;
        }

        friend class Application;

    private:
        double mult = 1.0f;
    };

    class Application
    {
    public:
        static Application& init(const StartupConfig& config, DemoSamples&& samples);

        static inline Application& get()
        {
            auto& me = staticSelf();
            checkAlwaysRel(me.created, "application was not created");
            return me;
        }

        inline bool isRunning() const { return running; }
        inline void quit() { pending_stop = true; }
        inline void setMaxFps(i32 target) { framerate.target_framerate = target; }
        inline i32 getMaxFps() { return framerate.desiredFramerate(); }
        inline tWindow* getMainWindow() const { return main_window.get(); }
        inline SettingsContainer& getSettings() { return settings; }
        inline const FrameTime& getTime() { return ftime; }

        template <typename T>
        void setGraphicsBackend(bool init);

        bool activateDemo(const char* id);
        i32 runLoop();  

    private:
        static Application& staticSelf()
        {
            static Application instance;
            return instance;
        }
        Application() = default;

        DemoSamples all_demos;
        std::unique_ptr<IDemoImpl> active_demo;
        std::unique_ptr<IGraphicsImpl> gfx_backend;
        std::unique_ptr<tWindow> main_window;
        SettingsContainer settings;

        FramerateArgs framerate;
        FrameTime ftime;

        bool running = false;
        bool pending_stop = false;
        bool created = false;

        // #todo move out/rewrite
        void showDemoSelection();
    };

    template <typename T>
    inline void Application::setGraphicsBackend(bool init)
    {
        if (gfx_backend)
        {
            gfx_backend->teardown(*this);
        }
        gfx_backend.reset(new T());
        if (init)
        {
            gfx_backend->init(*this);
        }
    }

    //=====================================================================================
    // variables
    inline FrameTime g_time = {};

} // namespace vp
