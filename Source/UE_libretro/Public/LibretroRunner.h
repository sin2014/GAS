#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Misc/Paths.h"

extern "C"
{
#include "libretro.h"
}

class UTexture2D;
class USoundWaveProcedural;
class FLibretroOpenGLRenderContext;

enum class ELibretroSystemType : uint8
{
    NES,
    NDS,
    ThreeDS
};

enum class ELibretroButton : uint8
{
    B = RETRO_DEVICE_ID_JOYPAD_B,
    Y = RETRO_DEVICE_ID_JOYPAD_Y,
    Select = RETRO_DEVICE_ID_JOYPAD_SELECT,
    Start = RETRO_DEVICE_ID_JOYPAD_START,
    Up = RETRO_DEVICE_ID_JOYPAD_UP,
    Down = RETRO_DEVICE_ID_JOYPAD_DOWN,
    Left = RETRO_DEVICE_ID_JOYPAD_LEFT,
    Right = RETRO_DEVICE_ID_JOYPAD_RIGHT,
    A = RETRO_DEVICE_ID_JOYPAD_A,
    X = RETRO_DEVICE_ID_JOYPAD_X,
    L = RETRO_DEVICE_ID_JOYPAD_L,
    R = RETRO_DEVICE_ID_JOYPAD_R,
    L2 = RETRO_DEVICE_ID_JOYPAD_L2,
    R2 = RETRO_DEVICE_ID_JOYPAD_R2,
    L3 = RETRO_DEVICE_ID_JOYPAD_L3,
    R3 = RETRO_DEVICE_ID_JOYPAD_R3,
    Count = 16
};

struct FLibretroLaunchConfig
{
    FString CorePath;
    FString RomPath;
    FString DisplayName;
    ELibretroSystemType SystemType = ELibretroSystemType::NES;
    TMap<FString, FString> CoreOptions;
};

class FLibretroRunner final : public FRunnable
{
public:
    FLibretroRunner();
    virtual ~FLibretroRunner() override;

    bool Start(const FLibretroLaunchConfig& Config);
    bool Start(const FString& InRomPath);
    void StopAndUnload();
    void Reset();

    UTexture2D* GetVideoTexture() const { return VideoTexture; }
    USoundWaveProcedural* GetSoundWave() const { return SoundWave; }

    void SetButtonState(ELibretroButton Button, bool bPressed);
    void SetPointerState(float NormalizedX, float NormalizedY, bool bPressed);
    bool IsRunning() const { return bRunning; }
    FString GetLastError() const;
    FString GetStatusText() const;
    FString GetLoadedRomPath() const;
    FString GetLoadedCorePath() const;

    virtual uint32 Run() override;
    virtual void Stop() override;

    bool ConsumeFrameForTextureUpdate();

private:
    struct FLibretroCoreApi
    {
        void (*retro_set_environment)(retro_environment_t) = nullptr;
        void (*retro_set_video_refresh)(retro_video_refresh_t) = nullptr;
        void (*retro_set_audio_sample)(retro_audio_sample_t) = nullptr;
        void (*retro_set_audio_sample_batch)(retro_audio_sample_batch_t) = nullptr;
        void (*retro_set_input_poll)(retro_input_poll_t) = nullptr;
        void (*retro_set_input_state)(retro_input_state_t) = nullptr;
        void (*retro_init)() = nullptr;
        void (*retro_deinit)() = nullptr;
        unsigned (*retro_api_version)() = nullptr;
        void (*retro_get_system_info)(retro_system_info*) = nullptr;
        void (*retro_get_system_av_info)(retro_system_av_info*) = nullptr;
        void (*retro_set_controller_port_device)(unsigned, unsigned) = nullptr;
        void (*retro_reset)() = nullptr;
        void (*retro_run)() = nullptr;
        size_t (*retro_serialize_size)() = nullptr;
        bool (*retro_serialize)(void*, size_t) = nullptr;
        bool (*retro_unserialize)(const void*, size_t) = nullptr;
        void (*retro_cheat_reset)() = nullptr;
        void (*retro_cheat_set)(unsigned, bool, const char*) = nullptr;
        bool (*retro_load_game)(const retro_game_info*) = nullptr;
        bool (*retro_load_game_special)(unsigned, const retro_game_info*, size_t) = nullptr;
        void (*retro_unload_game)() = nullptr;
        unsigned (*retro_get_region)() = nullptr;
        void* (*retro_get_memory_data)(unsigned) = nullptr;
        size_t (*retro_get_memory_size)(unsigned) = nullptr;
    };

    bool PrepareLaunch(const FLibretroLaunchConfig& Config);
    FString ResolvePath(const FString& Path) const;
    bool LoadCore();
    void UnloadCore();
    bool LoadGame(const FString& InRomPath);
    void CleanupCoreOnRunnerThread();
    void SaveSRAM();
    void SetError(const FString& Error);
    void SetStatus(const FString& Status);
    bool ExportSymbol(const ANSICHAR* Name, void*& OutPtr);
    void InitializeVideoTexture(unsigned Width, unsigned Height);
    void InitializeAudioStream();
    void QueueSoftwareVideoFrame(const void* Data, unsigned Width, unsigned Height, size_t Pitch);
    void QueueHardwareVideoFrame(unsigned Width, unsigned Height);
    void SubmitConvertedVideoFrame(TArray<uint8>&& Converted, unsigned Width, unsigned Height, const TCHAR* SourceName);
    void QueueAudio(const int16* Data, size_t Frames);
    int16 QueryInput(unsigned Port, unsigned Device, unsigned Index, unsigned Id) const;
    bool HandleEnvironment(unsigned Cmd, void* Data);
    bool ConfigureHardwareRendering(retro_hw_render_callback* Callback);
    const char* FindCoreOptionValue(const char* Key) const;

    static bool RetroEnvironment(unsigned Cmd, void* Data);
    static void RetroVideoRefresh(const void* Data, unsigned Width, unsigned Height, size_t Pitch);
    static void RetroAudioSample(int16 Left, int16 Right);
    static size_t RetroAudioSampleBatch(const int16* Data, size_t Frames);
    static void RetroInputPoll();
    static int16 RetroInputState(unsigned Port, unsigned Device, unsigned Index, unsigned Id);
    static void RetroLog(enum retro_log_level Level, const char* Fmt, ...);
    static uintptr_t RetroGetCurrentFramebuffer();
    static retro_proc_address_t RetroGetProcAddress(const char* Sym);

private:
    FLibretroCoreApi CoreApi;
    void* CoreHandle = nullptr;
    FRunnableThread* Thread = nullptr;

    FLibretroLaunchConfig LaunchConfig;
    FString RomPath;
    FString CorePath;
    FString SystemDir;
    FString SaveDir;
    FString ContentDir;
    FString CoreAssetsDir;
    FString LibretroPath;

    FTCHARToUTF8* SystemDirUtf8 = nullptr;
    FTCHARToUTF8* SaveDirUtf8 = nullptr;
    FTCHARToUTF8* ContentDirUtf8 = nullptr;
    FTCHARToUTF8* CoreAssetsDirUtf8 = nullptr;
    FTCHARToUTF8* LibretroPathUtf8 = nullptr;
    TMap<FString, TArray<ANSICHAR>> CoreOptionValueUtf8;

    retro_pixel_format PixelFormat = RETRO_PIXEL_FORMAT_XRGB8888;
    retro_system_av_info AvInfo = {};
    retro_frame_time_callback FrameTimeCallback = {};

    UTexture2D* VideoTexture = nullptr;
    USoundWaveProcedural* SoundWave = nullptr;
    TUniquePtr<FLibretroOpenGLRenderContext> OpenGLRenderContext;

    mutable FCriticalSection StateMutex;
    FString LastError;
    FString StatusText;
    bool ButtonStates[static_cast<uint8>(ELibretroButton::Count)] = {};
    float PointerX = 0.5f;
    float PointerY = 0.75f;
    bool bPointerPressed = false;

    FCriticalSection FrameMutex;
    TArray<uint8> FrameBGRA;
    unsigned FrameWidth = 0;
    unsigned FrameHeight = 0;
    unsigned TextureWidth = 0;
    unsigned TextureHeight = 0;
    bool bNewFrame = false;
    uint64 VideoFrameCounter = 0;
    bool bLoggedFirstFrame = false;
    double ActualRunFps = 0.0;
    double AverageRetroRunMs = 0.0;

    FThreadSafeBool bStopRequested = false;
    FThreadSafeBool bRunning = false;
    FThreadSafeBool bResetRequested = false;
    bool bCoreInitialized = false;
    bool bGameLoaded = false;

    double TargetFps = 60.0;
    double TargetSampleRate = 48000.0;
};
