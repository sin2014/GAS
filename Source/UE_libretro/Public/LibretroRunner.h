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

/**
 * 当前前端支持的平台粗分类。
 *
 * 这个枚举不是 libretro API 的一部分，而是 UE 侧用来区分启动配置、
 * 默认 core 选项、硬件渲染预期和日志说明的本地分类。
 */
enum class ELibretroSystemType : uint8
{
    /** FC/NES 内容，目前通过 fceumm_libretro.dll 启动。 */
    NES,

    /** Nintendo DS 内容，目前通过 desmume_libretro.dll 启动。 */
    NDS,

    /** Nintendo 3DS 内容，目前通过 azahar_libretro.dll 启动。 */
    ThreeDS
};

/**
 * libretro RetroPad 按键枚举。
 *
 * UE 键盘输入会被转换为这些按键状态，libretro core 在 input 回调中
 * 再按 RetroPad ID 查询当前状态。
 */
enum class ELibretroButton : uint8
{
    /** RetroPad 的 B / 南侧面键。 */
    B = RETRO_DEVICE_ID_JOYPAD_B,

    /** RetroPad 的 Y / 西侧面键。 */
    Y = RETRO_DEVICE_ID_JOYPAD_Y,

    /** Select 按键。 */
    Select = RETRO_DEVICE_ID_JOYPAD_SELECT,

    /** Start 按键。 */
    Start = RETRO_DEVICE_ID_JOYPAD_START,

    /** 十字键上。 */
    Up = RETRO_DEVICE_ID_JOYPAD_UP,

    /** 十字键下。 */
    Down = RETRO_DEVICE_ID_JOYPAD_DOWN,

    /** 十字键左。 */
    Left = RETRO_DEVICE_ID_JOYPAD_LEFT,

    /** 十字键右。 */
    Right = RETRO_DEVICE_ID_JOYPAD_RIGHT,

    /** RetroPad 的 A / 东侧面键。 */
    A = RETRO_DEVICE_ID_JOYPAD_A,

    /** RetroPad 的 X / 北侧面键。 */
    X = RETRO_DEVICE_ID_JOYPAD_X,

    /** 左肩键。 */
    L = RETRO_DEVICE_ID_JOYPAD_L,

    /** 右肩键。 */
    R = RETRO_DEVICE_ID_JOYPAD_R,

    /** 左扳机键，对应 3DS 的 ZL。 */
    L2 = RETRO_DEVICE_ID_JOYPAD_L2,

    /** 右扳机键，对应 3DS 的 ZR。 */
    R2 = RETRO_DEVICE_ID_JOYPAD_R2,

    /** 左摇杆按下。 */
    L3 = RETRO_DEVICE_ID_JOYPAD_L3,

    /** 右摇杆按下。 */
    R3 = RETRO_DEVICE_ID_JOYPAD_R3,

    /** ButtonStates 中保存的有效 RetroPad 按键数量。 */
    Count = 16
};

/**
 * 一次 libretro 启动请求所需的完整配置。
 *
 * Pawn 会根据 UI 按钮或命令行参数构造这个结构体；Runner 会负责校验路径、
 * 转换 UTF-8 字符串、保存 core 选项，并启动独立线程运行 core。
 */
struct FLibretroLaunchConfig
{
    /** libretro core DLL 的绝对路径或项目相对路径。 */
    FString CorePath;

    /** ROM/内容文件的绝对路径或项目相对路径。 */
    FString RomPath;

    /** 显示在状态文本和截图文件名中的可读名称。 */
    FString DisplayName;

    /** 平台分类，用于选择平台相关默认行为和日志提示。 */
    ELibretroSystemType SystemType = ELibretroSystemType::NES;

    /** 提供给 RETRO_ENVIRONMENT_GET_VARIABLE 的 core 选项值。 */
    TMap<FString, FString> CoreOptions;
};

/**
 * UE 侧最小 libretro 前端。
 *
 * 主要职责：
 * - 动态加载 libretro core DLL，并绑定 retro_* 导出函数。
 * - 向 core 提供 environment、video、audio、input、log 和硬件渲染回调。
 * - 在工作线程中按 core 报告的帧率循环调用 retro_run()。
 * - 把软件帧或 OpenGL 硬件帧转换成 UE 可上传的 BGRA 帧。
 * - 把 libretro 音频样本送入 USoundWaveProcedural。
 *
 * 注意：UObject 创建和 UTexture2D 更新必须放在游戏线程；core 生命周期和
 * retro_run() 保持在 Runner 线程中，避免大多数 core 的非重入问题。
 */
class FLibretroRunner final : public FRunnable
{
public:
    /** 创建默认 libretro 目录，并准备长期有效的 UTF-8 路径缓存。 */
    FLibretroRunner();

    /** 停止当前 core、卸载 DLL，并释放 UTF-8 路径缓存。 */
    virtual ~FLibretroRunner() override;

    /** 使用完整启动配置开始一次 libretro 会话。 */
    bool Start(const FLibretroLaunchConfig& Config);

    /** 兼容旧 FC/NES 调用路径的便捷重载。 */
    bool Start(const FString& InRomPath);

    /** 请求 Runner 线程停止，等待退出，保存 SRAM，并卸载 core。 */
    void StopAndUnload();

    /** 请求在下一帧执行一次 core 软重置。 */
    void Reset();

    /** 请求在 Runner 线程下一帧把当前模拟器状态写入单槽即时存档。 */
    void RequestQuickSave();

    /** 请求在 Runner 线程下一帧从单槽即时存档恢复当前模拟器状态。 */
    void RequestQuickLoad();

    /** 把模拟运行速度切换到下一个更慢档位，最低 0.5 倍速。 */
    void DecreaseSpeed();

    /** 把模拟运行速度切换到下一个更快档位，最高 2.0 倍速。 */
    void IncreaseSpeed();

    /** 返回当前模拟运行速度倍率。 */
    double GetSpeedMultiplier() const;

    /** 返回显示最新视频帧的临时纹理。 */
    UTexture2D* GetVideoTexture() const { return VideoTexture; }

    /** 返回接收 libretro 双声道样本的程序化音频流。 */
    USoundWaveProcedural* GetSoundWave() const { return SoundWave; }

    /** 更新一个 RetroPad 按键状态。 */
    void SetButtonState(ELibretroButton Button, bool bPressed);

    /** 更新归一化触摸/指针坐标和按下状态。 */
    void SetPointerState(float NormalizedX, float NormalizedY, bool bPressed);

    /** 返回 Runner 线程是否正在 retro_run() 主循环中。 */
    bool IsRunning() const { return bRunning; }

    /** 返回最近一次可显示给用户的错误。 */
    FString GetLastError() const;

    /** 返回当前 UI 状态文本。 */
    FString GetStatusText() const;

    /** 返回当前或上一次会话解析后的 ROM 路径。 */
    FString GetLoadedRomPath() const;

    /** 返回当前或上一次会话解析后的 core 路径。 */
    FString GetLoadedCorePath() const;

    /** FRunnable 入口；线程启动后在这里拥有 libretro 生命周期。 */
    virtual uint32 Run() override;

    /** FRunnable 停止钩子；只设置线程安全停止标记。 */
    virtual void Stop() override;

    /** 在游戏线程中把最新 BGRA 帧上传到 UTexture2D。 */
    bool ConsumeFrameForTextureUpdate();

private:
    /**
     * 从当前 libretro core DLL 动态解析出的函数表。
     *
     * 这些函数指针与 libretro.h 的 retro_* 导出一一对应，让前端无需静态链接
     * 到某一个具体模拟器 core。
     */
    struct FLibretroCoreApi
    {
        /** 注册 environment 回调，用于 core 选项、目录、硬件渲染等请求。 */
        void (*retro_set_environment)(retro_environment_t) = nullptr;

        /** 注册视频回调，用于软件帧和硬件帧交付。 */
        void (*retro_set_video_refresh)(retro_video_refresh_t) = nullptr;

        /** 注册单个双声道样本的音频回调。 */
        void (*retro_set_audio_sample)(retro_audio_sample_t) = nullptr;

        /** 注册批量双声道样本的音频回调。 */
        void (*retro_set_audio_sample_batch)(retro_audio_sample_batch_t) = nullptr;

        /** 注册 core 请求前端轮询输入的回调。 */
        void (*retro_set_input_poll)(retro_input_poll_t) = nullptr;

        /** 注册 core 查询当前输入状态的回调。 */
        void (*retro_set_input_state)(retro_input_state_t) = nullptr;

        /** 在注册回调后初始化 core 全局状态。 */
        void (*retro_init)() = nullptr;

        /** 在卸载 DLL 前释放 core 全局状态。 */
        void (*retro_deinit)() = nullptr;

        /** 返回 core 实现的 libretro ABI 版本。 */
        unsigned (*retro_api_version)() = nullptr;

        /** 返回 core 名称、版本等静态信息。 */
        void (*retro_get_system_info)(retro_system_info*) = nullptr;

        /** 返回加载内容后的视频尺寸、帧率和音频采样率。 */
        void (*retro_get_system_av_info)(retro_system_av_info*) = nullptr;

        /** 设置某个 libretro 控制器端口连接的输入设备类型。 */
        void (*retro_set_controller_port_device)(unsigned, unsigned) = nullptr;

        /** 重置当前加载的游戏/内容。 */
        void (*retro_reset)() = nullptr;

        /** 运行一帧模拟，并在过程中触发输入、音频、视频等回调。 */
        void (*retro_run)() = nullptr;

        /** 返回即时存档所需字节数；0 表示 core 不支持序列化。 */
        size_t (*retro_serialize_size)() = nullptr;

        /** 把当前模拟器即时状态写入调用方提供的缓冲区。 */
        bool (*retro_serialize)(void*, size_t) = nullptr;

        /** 从调用方提供的缓冲区恢复模拟器即时状态。 */
        bool (*retro_unserialize)(const void*, size_t) = nullptr;

        /** 清除当前 core 的全部作弊码。 */
        void (*retro_cheat_reset)() = nullptr;

        /** 按索引启用或禁用一条 core 作弊码。 */
        void (*retro_cheat_set)(unsigned, bool, const char*) = nullptr;

        /** 加载普通单文件游戏/内容。 */
        bool (*retro_load_game)(const retro_game_info*) = nullptr;

        /** 加载 core 支持的特殊多文件或子系统内容。 */
        bool (*retro_load_game_special)(unsigned, const retro_game_info*, size_t) = nullptr;

        /** 卸载当前游戏/内容。 */
        void (*retro_unload_game)() = nullptr;

        /** 返回 core 报告的区域制式，例如 NTSC 或 PAL。 */
        unsigned (*retro_get_region)() = nullptr;

        /** 返回 SRAM 等可变内存区域的指针。 */
        void* (*retro_get_memory_data)(unsigned) = nullptr;

        /** 返回 SRAM 等可变内存区域的字节大小。 */
        size_t (*retro_get_memory_size)(unsigned) = nullptr;
    };

    /** 校验启动路径，准备 UTF-8 状态、core 选项、输入状态和帧队列。 */
    bool PrepareLaunch(const FLibretroLaunchConfig& Config);

    /** 把项目相对路径或绝对路径规范化成绝对路径。 */
    FString ResolvePath(const FString& Path) const;

    /** 加载 core DLL，并解析所有必需的 retro_* 导出函数。 */
    bool LoadCore();

    /** 释放当前 core DLL，并清空函数表。 */
    void UnloadCore();

    /** 根据 core 的 need_fullpath 要求填充 retro_game_info 并调用 retro_load_game()。 */
    bool LoadGame(const FString& InRomPath, const retro_system_info& SystemInfo);

    /** 按 libretro 生命周期顺序在 Runner 线程中清理：保存 SRAM、卸载游戏、反初始化 core。 */
    void CleanupCoreOnRunnerThread();

    /** 如果 core 暴露了 SRAM，就写入 Saved/Libretro/Saves。 */
    void SaveSRAM();

    /** 返回当前 ROM 对应的单槽即时存档路径。 */
    FString GetQuickStatePath() const;

    /** 处理用户在游戏线程发来的即时存档、即时读档等运行时请求。 */
    void ProcessRuntimeRequests();

    /** 使用 retro_serialize 把当前模拟器完整状态写入单槽文件。 */
    bool SaveQuickState();

    /** 使用 retro_unserialize 从单槽文件恢复当前模拟器完整状态。 */
    bool LoadQuickState();

    /** 按固定速度档位设置当前模拟运行倍率。 */
    void SetSpeedIndex(int32 NewSpeedIndex);

    /** 记录错误日志，并把错误保存为 UI 状态文本。 */
    void SetError(const FString& Error);

    /** 保存非错误状态文本，供 UMG 显示。 */
    void SetStatus(const FString& Status);

    /** 从已加载的 core DLL 中解析一个导出符号。 */
    bool ExportSymbol(const ANSICHAR* Name, void*& OutPtr);

    /** 创建或重建接收 BGRA 帧的临时视频纹理。 */
    void InitializeVideoTexture(unsigned Width, unsigned Height);

    /** 创建并配置双声道程序化音频流。 */
    void InitializeAudioStream();

    /** 把 libretro 软件视频帧转换成 BGRA，并放入游戏线程帧队列。 */
    void QueueSoftwareVideoFrame(const void* Data, unsigned Width, unsigned Height, size_t Pitch);

    /** 读取 OpenGL 硬件帧缓冲，转换成 BGRA，并放入游戏线程帧队列。 */
    void QueueHardwareVideoFrame(unsigned Width, unsigned Height);

    /** 发布已经转换好的 BGRA 帧，并记录轻量诊断信息。 */
    void SubmitConvertedVideoFrame(TArray<uint8>&& Converted, unsigned Width, unsigned Height, const TCHAR* SourceName);

    /** 把交错排列的 int16 双声道样本写入 USoundWaveProcedural。 */
    void QueueAudio(const int16* Data, size_t Frames);

    /** 根据当前按键和触摸状态回答 libretro 输入查询。 */
    int16 QueryInput(unsigned Port, unsigned Device, unsigned Index, unsigned Id) const;

    /** 处理 core 发出的 RETRO_ENVIRONMENT_* 请求。 */
    bool HandleEnvironment(unsigned Cmd, void* Data);

    /** 配置 libretro OpenGL 硬件渲染回调和隐藏帧缓冲。 */
    bool ConfigureHardwareRendering(retro_hw_render_callback* Callback);

    /** 保存一个长期有效的 core 选项值，供 RETRO_ENVIRONMENT_GET_VARIABLE 返回。 */
    void StoreCoreOptionValue(const FString& Key, const FString& Value, bool bOverwriteExisting);

    /** 保存 core 通过 UTF-8 C 字符串声明的选项值。 */
    void StoreCoreOptionValueUtf8(const char* Key, const char* Value, bool bOverwriteExisting);

    /** 注册旧版 RETRO_ENVIRONMENT_SET_VARIABLES 选项，并提取默认值。 */
    void RegisterLegacyCoreOptions(const retro_variable* Variables);

    /** 注册 RETRO_ENVIRONMENT_SET_CORE_OPTIONS / INTL 选项，并提取默认值。 */
    void RegisterCoreOptionDefinitions(const retro_core_option_definition* Definitions);

    /** 注册 RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2 / V2_INTL 选项，并提取默认值。 */
    void RegisterCoreOptionsV2(const retro_core_options_v2* Options);

    /** 查找某个 core 选项的 UTF-8 值，供 RETRO_ENVIRONMENT_GET_VARIABLE 返回。 */
    const char* FindCoreOptionValue(const char* Key) const;

    /** libretro environment 静态回调入口，转发到当前活动 Runner。 */
    static bool RetroEnvironment(unsigned Cmd, void* Data);

    /** libretro 视频静态回调入口，负责区分软件帧和硬件帧。 */
    static void RetroVideoRefresh(const void* Data, unsigned Width, unsigned Height, size_t Pitch);

    /** libretro 单样本音频静态回调入口。 */
    static void RetroAudioSample(int16 Left, int16 Right);

    /** libretro 批量音频静态回调入口。 */
    static size_t RetroAudioSampleBatch(const int16* Data, size_t Frames);

    /** libretro 输入轮询静态回调；实际状态已由 UE 输入事件维护。 */
    static void RetroInputPoll();

    /** libretro 输入状态静态回调入口。 */
    static int16 RetroInputState(unsigned Port, unsigned Device, unsigned Index, unsigned Id);

    /** libretro 日志静态回调，把 core 日志映射到 UE_LOG。 */
    static void RetroLog(enum retro_log_level Level, const char* Fmt, ...);

    /** libretro OpenGL 回调，返回当前帧缓冲对象。 */
    static uintptr_t RetroGetCurrentFramebuffer();

    /** libretro OpenGL 回调，用于解析 GL/WGL 函数地址。 */
    static retro_proc_address_t RetroGetProcAddress(const char* Sym);

private:
    /** 当前 core 的 retro_* 函数指针表。 */
    FLibretroCoreApi CoreApi;

    /** FPlatformProcess::GetDllHandle 返回的平台 DLL 句柄。 */
    void* CoreHandle = nullptr;

    /** 拥有 libretro core 运行循环的工作线程。 */
    FRunnableThread* Thread = nullptr;

    /** 最近一次请求的启动配置。 */
    FLibretroLaunchConfig LaunchConfig;

    /** 当前 ROM/内容文件解析后的绝对路径。 */
    FString RomPath;

    /** 当前 libretro core DLL 解析后的绝对路径。 */
    FString CorePath;

    /** 提供给 RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY 的系统目录。 */
    FString SystemDir;

    /** 提供给 RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY 的存档目录。 */
    FString SaveDir;

    /** 即时存档单槽状态文件所在目录。 */
    FString StateDir;

    /** 当前 ROM/内容所在目录，提供给内容目录查询。 */
    FString ContentDir;

    /** 提供给 core assets 语义使用的目录，目前映射到 Saved/Libretro/Cache。 */
    FString CoreAssetsDir;

    /** 提供给 RETRO_ENVIRONMENT_GET_LIBRETRO_PATH 的 core 路径。 */
    FString LibretroPath;

    /** SystemDir 的长期有效 UTF-8 缓存。 */
    FTCHARToUTF8* SystemDirUtf8 = nullptr;

    /** SaveDir 的长期有效 UTF-8 缓存。 */
    FTCHARToUTF8* SaveDirUtf8 = nullptr;

    /** ContentDir 的长期有效 UTF-8 缓存。 */
    FTCHARToUTF8* ContentDirUtf8 = nullptr;

    /** CoreAssetsDir 的长期有效 UTF-8 缓存。 */
    FTCHARToUTF8* CoreAssetsDirUtf8 = nullptr;

    /** LibretroPath 的长期有效 UTF-8 缓存。 */
    FTCHARToUTF8* LibretroPathUtf8 = nullptr;

    /** core 选项值的长期有效 UTF-8 缓存。 */
    TMap<FString, TArray<ANSICHAR>> CoreOptionValueUtf8;

    /** 当前 core 通过 SET_PIXEL_FORMAT 请求的软件帧像素格式。 */
    retro_pixel_format PixelFormat = RETRO_PIXEL_FORMAT_XRGB8888;

    /** core 最近报告的视频尺寸、帧率和音频采样率。 */
    retro_system_av_info AvInfo = {};

    /** 某些 core 请求的逐帧时间回调。 */
    retro_frame_time_callback FrameTimeCallback = {};

    /** 游戏线程上传帧数据的临时 UE 视频纹理。 */
    UTexture2D* VideoTexture = nullptr;

    /** 接收 libretro 音频回调的程序化音频流。 */
    USoundWaveProcedural* SoundWave = nullptr;

    /** Windows OpenGL 上下文与帧缓冲桥接对象，用于硬件渲染 core。 */
    TUniquePtr<FLibretroOpenGLRenderContext> OpenGLRenderContext;

    /** 保护 LastError、StatusText、按键状态、触摸状态和速度设置。 */
    mutable FCriticalSection StateMutex;

    /** 最近一次用户可见错误。 */
    FString LastError;

    /** 当前用户可见状态文本。 */
    FString StatusText;

    /** 当前 RetroPad 按键状态，索引与 ELibretroButton 值一致。 */
    bool ButtonStates[static_cast<uint8>(ELibretroButton::Count)] = {};

    /** 归一化触摸/指针 X 坐标，0.0 为左侧，1.0 为右侧。 */
    float PointerX = 0.5f;

    /** 归一化触摸/指针 Y 坐标，0.0 为顶部，1.0 为底部。 */
    float PointerY = 0.75f;

    /** 当前触摸/指针是否按下。 */
    bool bPointerPressed = false;

    /** 保护跨线程视频帧队列。 */
    FCriticalSection FrameMutex;

    /** 等待游戏线程上传的最新 BGRA 帧。 */
    TArray<uint8> FrameBGRA;

    /** FrameBGRA 的像素宽度。 */
    unsigned FrameWidth = 0;

    /** FrameBGRA 的像素高度。 */
    unsigned FrameHeight = 0;

    /** 当前 UTexture2D 的像素宽度。 */
    unsigned TextureWidth = 0;

    /** 当前 UTexture2D 的像素高度。 */
    unsigned TextureHeight = 0;

    /** FrameBGRA 是否包含尚未上传到纹理的新帧。 */
    bool bNewFrame = false;

    /** 视频帧诊断计数器。 */
    uint64 VideoFrameCounter = 0;

    /** 防止同一会话重复输出首帧日志。 */
    bool bLoggedFirstFrame = false;

    /** 最近一个统计窗口内测得的 retro_run 循环 FPS。 */
    double ActualRunFps = 0.0;

    /** 最近一个统计窗口内 retro_run() 的平均耗时。 */
    double AverageRetroRunMs = 0.0;

    /** Runner 线程读取的停止请求标记。 */
    FThreadSafeBool bStopRequested = false;

    /** Runner 主循环是否处于活动状态。 */
    FThreadSafeBool bRunning = false;

    /** Runner 线程读取的软重置请求标记。 */
    FThreadSafeBool bResetRequested = false;

    /** Runner 线程读取的即时存档请求标记。 */
    FThreadSafeBool bQuickSaveRequested = false;

    /** Runner 线程读取的即时读档请求标记。 */
    FThreadSafeBool bQuickLoadRequested = false;

    /** retro_init() 成功后到 retro_deinit() 前为 true。 */
    bool bCoreInitialized = false;

    /** retro_load_game() 成功后到 retro_unload_game() 前为 true。 */
    bool bGameLoaded = false;

    /** 当前 core 报告的目标帧率。 */
    double TargetFps = 60.0;

    /** 当前 core 报告的目标音频采样率。 */
    double TargetSampleRate = 48000.0;

    /** 当前速度档位索引：0=0.5x，1=1.0x，2=1.5x，3=2.0x。 */
    int32 SpeedIndex = 1;

    /** 当前模拟速度倍率，影响 retro_run() 主循环节流间隔。 */
    double SpeedMultiplier = 1.0;
};
