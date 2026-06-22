#include "LibretroNESRunner.h"

#include "Async/Async.h"
#include "Engine/Texture2D.h"
#include "HAL/PlatformProcess.h"
#include "Logging/LogMacros.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeLock.h"
#include "Sound/SoundWaveProcedural.h"

DEFINE_LOG_CATEGORY_STATIC(LogLibretroRunner, Log, All);

static FLibretroNESRunner* GActiveLibretroRunner = nullptr;

FLibretroNESRunner::FLibretroNESRunner()
{
    const FString ProjectDir = FPaths::ProjectDir();
    CorePath = FPaths::ConvertRelativePathToFull(ProjectDir / TEXT("ThirdParty/Libretro/Cores/Win64/fceumm_libretro.dll"));
    SystemDir = FPaths::ConvertRelativePathToFull(ProjectDir / TEXT("Saved/Libretro/System"));
    SaveDir = FPaths::ConvertRelativePathToFull(ProjectDir / TEXT("Saved/Libretro/Saves"));
    CoreAssetsDir = FPaths::ConvertRelativePathToFull(ProjectDir / TEXT("Saved/Libretro/Cache"));
    LibretroPath = CorePath;

    IFileManager::Get().MakeDirectory(*SystemDir, true);
    IFileManager::Get().MakeDirectory(*SaveDir, true);
    IFileManager::Get().MakeDirectory(*CoreAssetsDir, true);

    SystemDirUtf8 = new FTCHARToUTF8(*SystemDir);
    SaveDirUtf8 = new FTCHARToUTF8(*SaveDir);
    CoreAssetsDirUtf8 = new FTCHARToUTF8(*CoreAssetsDir);
    LibretroPathUtf8 = new FTCHARToUTF8(*LibretroPath);
}

FLibretroNESRunner::~FLibretroNESRunner()
{
    StopAndUnload();
    delete SystemDirUtf8;
    delete SaveDirUtf8;
    delete ContentDirUtf8;
    delete CoreAssetsDirUtf8;
    delete LibretroPathUtf8;
}

bool FLibretroNESRunner::Start(const FString& InRomPath)
{
    FLibretroLaunchConfig Config;
    Config.RomPath = InRomPath;
    Config.CorePath = FPaths::ProjectDir() / TEXT("ThirdParty/Libretro/Cores/Win64/fceumm_libretro.dll");
    Config.DisplayName = TEXT("重装机兵1");
    Config.SystemType = ELibretroSystemType::NES;
    return Start(Config);
}

bool FLibretroNESRunner::Start(const FLibretroLaunchConfig& Config)
{
    if (Thread || bRunning)
    {
        StopAndUnload();
    }

    if (!PrepareLaunch(Config))
    {
        return false;
    }

    bStopRequested = false;
    bResetRequested = false;
    bLoggedFirstFrame = false;
    VideoFrameCounter = 0;
    SetStatus(FString::Printf(TEXT("正在启动 %s..."), *LaunchConfig.DisplayName));

    Thread = FRunnableThread::Create(this, TEXT("LibretroRunner"), 0, TPri_Normal);
    if (!Thread)
    {
        SetError(TEXT("无法创建 libretro 运行线程"));
        return false;
    }

    return true;
}

bool FLibretroNESRunner::PrepareLaunch(const FLibretroLaunchConfig& Config)
{
    LaunchConfig = Config;
    RomPath = ResolvePath(Config.RomPath);
    CorePath = ResolvePath(Config.CorePath);
    LibretroPath = CorePath;
    ContentDir = FPaths::GetPath(RomPath);

    if (LaunchConfig.DisplayName.IsEmpty())
    {
        LaunchConfig.DisplayName = FPaths::GetBaseFilename(RomPath);
    }

    delete ContentDirUtf8;
    delete LibretroPathUtf8;
    ContentDirUtf8 = new FTCHARToUTF8(*ContentDir);
    LibretroPathUtf8 = new FTCHARToUTF8(*LibretroPath);

    CoreOptionValueUtf8.Reset();
    for (const TPair<FString, FString>& Pair : LaunchConfig.CoreOptions)
    {
        FTCHARToUTF8 Converted(*Pair.Value);
        TArray<ANSICHAR>& Buffer = CoreOptionValueUtf8.Add(Pair.Key);
        Buffer.Append(Converted.Get(), Converted.Length());
        Buffer.Add('\0');
    }

    if (!FPaths::FileExists(RomPath))
    {
        SetError(FString::Printf(TEXT("ROM 不存在：%s"), *RomPath));
        return false;
    }

    if (!FPaths::FileExists(CorePath))
    {
        SetError(FString::Printf(TEXT("libretro core 不存在：%s"), *CorePath));
        return false;
    }

    {
        FScopeLock Lock(&StateMutex);
        FMemory::Memzero(ButtonStates, sizeof(ButtonStates));
        PointerX = 0.5f;
        PointerY = 0.75f;
        bPointerPressed = false;
        LastError.Reset();
    }

    {
        FScopeLock Lock(&FrameMutex);
        FrameBGRA.Reset();
        FrameWidth = 0;
        FrameHeight = 0;
        bNewFrame = false;
    }

    return true;
}

FString FLibretroNESRunner::ResolvePath(const FString& Path) const
{
    if (FPaths::IsRelative(Path))
    {
        return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / Path);
    }

    return FPaths::ConvertRelativePathToFull(Path);
}

void FLibretroNESRunner::StopAndUnload()
{
    bStopRequested = true;

    if (Thread)
    {
        Thread->WaitForCompletion();
        delete Thread;
        Thread = nullptr;
    }

    SaveSRAM();

    if (bGameLoaded && Api.retro_unload_game)
    {
        Api.retro_unload_game();
        bGameLoaded = false;
    }

    if (bCoreInitialized && Api.retro_deinit)
    {
        Api.retro_deinit();
        bCoreInitialized = false;
    }

    UnloadCore();

    if (GActiveLibretroRunner == this)
    {
        GActiveLibretroRunner = nullptr;
    }

    bRunning = false;
    if (!RomPath.IsEmpty())
    {
        SetStatus(TEXT("已停止"));
    }
}

void FLibretroNESRunner::Reset()
{
    bResetRequested = true;
}

void FLibretroNESRunner::Stop()
{
    bStopRequested = true;
}

uint32 FLibretroNESRunner::Run()
{
    GActiveLibretroRunner = this;

    if (!LoadCore())
    {
        bRunning = false;
        return 1;
    }

    Api.retro_set_environment(&FLibretroNESRunner::RetroEnvironment);
    Api.retro_set_video_refresh(&FLibretroNESRunner::RetroVideoRefresh);
    Api.retro_set_audio_sample(&FLibretroNESRunner::RetroAudioSample);
    Api.retro_set_audio_sample_batch(&FLibretroNESRunner::RetroAudioSampleBatch);
    Api.retro_set_input_poll(&FLibretroNESRunner::RetroInputPoll);
    Api.retro_set_input_state(&FLibretroNESRunner::RetroInputState);

    Api.retro_init();
    bCoreInitialized = true;

    retro_system_info SystemInfo = {};
    Api.retro_get_system_info(&SystemInfo);
    UE_LOG(LogLibretroRunner, Log, TEXT("Loaded core: %s %s"),
        UTF8_TO_TCHAR(SystemInfo.library_name ? SystemInfo.library_name : ""),
        UTF8_TO_TCHAR(SystemInfo.library_version ? SystemInfo.library_version : ""));

    if (!LoadGame(RomPath))
    {
        bRunning = false;
        return 1;
    }

    Api.retro_get_system_av_info(&AvInfo);
    TargetFps = AvInfo.timing.fps > 1.0 ? AvInfo.timing.fps : 60.0;
    TargetSampleRate = AvInfo.timing.sample_rate > 1.0 ? AvInfo.timing.sample_rate : 48000.0;
    UE_LOG(LogLibretroRunner, Log, TEXT("Loaded ROM: %s"), *RomPath);
    UE_LOG(LogLibretroRunner, Log, TEXT("AV info: base=%ux%u max=%ux%u aspect=%.4f fps=%.6f sample_rate=%.1f"),
        AvInfo.geometry.base_width,
        AvInfo.geometry.base_height,
        AvInfo.geometry.max_width,
        AvInfo.geometry.max_height,
        AvInfo.geometry.aspect_ratio,
        TargetFps,
        TargetSampleRate);

    const unsigned MaxWidth = FMath::Max(AvInfo.geometry.max_width, AvInfo.geometry.base_width);
    const unsigned MaxHeight = FMath::Max(AvInfo.geometry.max_height, AvInfo.geometry.base_height);
    AsyncTask(ENamedThreads::GameThread, [this, MaxWidth, MaxHeight]()
    {
        InitializeTexture(MaxWidth > 0 ? MaxWidth : 256, MaxHeight > 0 ? MaxHeight : 240);
    });

    bRunning = true;
    SetStatus(FString::Printf(TEXT("运行中：%s  %.3f FPS  %.0f Hz"), *FPaths::GetCleanFilename(RomPath), TargetFps, TargetSampleRate));

    const double FrameTime = 1.0 / TargetFps;
    double NextFrameTime = FPlatformTime::Seconds();

    while (!bStopRequested)
    {
        if (bResetRequested)
        {
            if (Api.retro_reset)
            {
                Api.retro_reset();
            }
            bResetRequested = false;
        }

        Api.retro_run();

        const double Now = FPlatformTime::Seconds();
        NextFrameTime += FrameTime;
        const double SleepTime = NextFrameTime - Now;
        if (SleepTime > 0.001)
        {
            FPlatformProcess::Sleep(static_cast<float>(SleepTime));
        }
        else if (SleepTime < -FrameTime)
        {
            NextFrameTime = Now;
        }
    }

    bRunning = false;
    return 0;
}

bool FLibretroNESRunner::LoadCore()
{
    CoreHandle = FPlatformProcess::GetDllHandle(*CorePath);
    if (!CoreHandle)
    {
        SetError(FString::Printf(TEXT("无法加载 libretro core：%s"), *CorePath));
        return false;
    }

#define LOAD_RETRO_SYMBOL(Name) \
    if (!ExportSymbol(#Name, reinterpret_cast<void*&>(Api.Name))) \
    { \
        return false; \
    }

    LOAD_RETRO_SYMBOL(retro_set_environment);
    LOAD_RETRO_SYMBOL(retro_set_video_refresh);
    LOAD_RETRO_SYMBOL(retro_set_audio_sample);
    LOAD_RETRO_SYMBOL(retro_set_audio_sample_batch);
    LOAD_RETRO_SYMBOL(retro_set_input_poll);
    LOAD_RETRO_SYMBOL(retro_set_input_state);
    LOAD_RETRO_SYMBOL(retro_init);
    LOAD_RETRO_SYMBOL(retro_deinit);
    LOAD_RETRO_SYMBOL(retro_api_version);
    LOAD_RETRO_SYMBOL(retro_get_system_info);
    LOAD_RETRO_SYMBOL(retro_get_system_av_info);
    LOAD_RETRO_SYMBOL(retro_set_controller_port_device);
    LOAD_RETRO_SYMBOL(retro_reset);
    LOAD_RETRO_SYMBOL(retro_run);
    LOAD_RETRO_SYMBOL(retro_serialize_size);
    LOAD_RETRO_SYMBOL(retro_serialize);
    LOAD_RETRO_SYMBOL(retro_unserialize);
    LOAD_RETRO_SYMBOL(retro_cheat_reset);
    LOAD_RETRO_SYMBOL(retro_cheat_set);
    LOAD_RETRO_SYMBOL(retro_load_game);
    LOAD_RETRO_SYMBOL(retro_load_game_special);
    LOAD_RETRO_SYMBOL(retro_unload_game);
    LOAD_RETRO_SYMBOL(retro_get_region);
    LOAD_RETRO_SYMBOL(retro_get_memory_data);
    LOAD_RETRO_SYMBOL(retro_get_memory_size);

#undef LOAD_RETRO_SYMBOL

    if (Api.retro_api_version() != RETRO_API_VERSION)
    {
        SetError(FString::Printf(TEXT("libretro API 版本不匹配。core=%u frontend=%u"), Api.retro_api_version(), RETRO_API_VERSION));
        return false;
    }

    return true;
}

void FLibretroNESRunner::UnloadCore()
{
    if (CoreHandle)
    {
        FPlatformProcess::FreeDllHandle(CoreHandle);
        CoreHandle = nullptr;
    }
    Api = FApi();
}

bool FLibretroNESRunner::LoadGame(const FString& InRomPath)
{
    FTCHARToUTF8 RomPathUtf8(*InRomPath);
    retro_game_info Game = {};
    Game.path = RomPathUtf8.Get();
    Game.data = nullptr;
    Game.size = 0;
    Game.meta = nullptr;

    if (!Api.retro_load_game(&Game))
    {
        SetError(FString::Printf(TEXT("core 无法加载 ROM：%s"), *InRomPath));
        return false;
    }

    bGameLoaded = true;
    if (Api.retro_set_controller_port_device)
    {
        Api.retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
    }

    return true;
}

void FLibretroNESRunner::SaveSRAM()
{
    if (!bGameLoaded || !Api.retro_get_memory_data || !Api.retro_get_memory_size)
    {
        return;
    }

    void* SaveData = Api.retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
    const size_t SaveSize = Api.retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
    if (!SaveData || SaveSize == 0)
    {
        return;
    }

    const FString SavePath = SaveDir / (FPaths::GetBaseFilename(RomPath) + TEXT(".srm"));
    FFileHelper::SaveArrayToFile(TArrayView64<const uint8>(static_cast<const uint8*>(SaveData), static_cast<int64>(SaveSize)), *SavePath);
}

bool FLibretroNESRunner::ExportSymbol(const ANSICHAR* Name, void*& OutPtr)
{
    OutPtr = FPlatformProcess::GetDllExport(CoreHandle, ANSI_TO_TCHAR(Name));
    if (!OutPtr)
    {
        SetError(FString::Printf(TEXT("core 缺少导出函数：%s"), ANSI_TO_TCHAR(Name)));
        return false;
    }
    return true;
}

void FLibretroNESRunner::InitializeTexture(unsigned Width, unsigned Height)
{
    TextureWidth = Width;
    TextureHeight = Height;

    if (VideoTexture)
    {
        VideoTexture->RemoveFromRoot();
        VideoTexture = nullptr;
    }

    VideoTexture = UTexture2D::CreateTransient(TextureWidth, TextureHeight, PF_B8G8R8A8);
    VideoTexture->Filter = TF_Nearest;
    VideoTexture->NeverStream = true;
    VideoTexture->UpdateResource();
    VideoTexture->AddToRoot();

    if (!SoundWave)
    {
        SoundWave = NewObject<USoundWaveProcedural>();
        SoundWave->NumChannels = 2;
        SoundWave->Duration = INDEFINITELY_LOOPING_DURATION;
        SoundWave->SoundGroup = SOUNDGROUP_Default;
        SoundWave->bLooping = false;
        SoundWave->AddToRoot();
    }

    SoundWave->SetSampleRate(static_cast<int32>(TargetSampleRate));
}

bool FLibretroNESRunner::ConsumeFrameForTextureUpdate()
{
    if (!VideoTexture)
    {
        return false;
    }

    TArray<uint8> LocalFrame;
    unsigned LocalWidth = 0;
    unsigned LocalHeight = 0;
    {
        FScopeLock Lock(&FrameMutex);
        if (!bNewFrame || FrameBGRA.Num() == 0)
        {
            return false;
        }

        LocalFrame = FrameBGRA;
        LocalWidth = FrameWidth;
        LocalHeight = FrameHeight;
        bNewFrame = false;
    }

    if (LocalWidth == 0 || LocalHeight == 0)
    {
        return false;
    }

    FUpdateTextureRegion2D* HeapRegion = new FUpdateTextureRegion2D(0, 0, 0, 0, LocalWidth, LocalHeight);
    uint8* HeapData = static_cast<uint8*>(FMemory::Malloc(LocalFrame.Num()));
    FMemory::Memcpy(HeapData, LocalFrame.GetData(), LocalFrame.Num());

    VideoTexture->UpdateTextureRegions(
        0,
        1,
        HeapRegion,
        LocalWidth * 4,
        4,
        HeapData,
        [](uint8* Data, const FUpdateTextureRegion2D* Regions)
        {
            FMemory::Free(Data);
            delete Regions;
        });

    return true;
}

void FLibretroNESRunner::CopyVideoFrame(const void* Data, unsigned Width, unsigned Height, size_t Pitch)
{
    if (!Data || Width == 0 || Height == 0)
    {
        return;
    }

    if (!bLoggedFirstFrame)
    {
        bLoggedFirstFrame = true;
        UE_LOG(LogLibretroRunner, Log, TEXT("First video frame received: %ux%u pitch=%llu pixel_format=%d"),
            Width,
            Height,
            static_cast<unsigned long long>(Pitch),
            static_cast<int32>(PixelFormat));
    }

    TArray<uint8> Converted;
    Converted.SetNumUninitialized(Width * Height * 4);
    uint64 BrightnessSum = 0;
    uint32 NonBlackPixels = 0;

    const uint8* SrcBytes = static_cast<const uint8*>(Data);
    for (unsigned Y = 0; Y < Height; ++Y)
    {
        const uint8* SrcRow = SrcBytes + Y * Pitch;
        uint8* DstRow = Converted.GetData() + Y * Width * 4;

        if (PixelFormat == RETRO_PIXEL_FORMAT_RGB565)
        {
            const uint16* Src = reinterpret_cast<const uint16*>(SrcRow);
            for (unsigned X = 0; X < Width; ++X)
            {
                const uint16 P = Src[X];
                const uint8 R = static_cast<uint8>(((P >> 11) & 0x1F) * 255 / 31);
                const uint8 G = static_cast<uint8>(((P >> 5) & 0x3F) * 255 / 63);
                const uint8 B = static_cast<uint8>((P & 0x1F) * 255 / 31);
                DstRow[X * 4 + 0] = B;
                DstRow[X * 4 + 1] = G;
                DstRow[X * 4 + 2] = R;
                DstRow[X * 4 + 3] = 255;
                BrightnessSum += R + G + B;
                NonBlackPixels += (R | G | B) ? 1u : 0u;
            }
        }
        else if (PixelFormat == RETRO_PIXEL_FORMAT_0RGB1555)
        {
            const uint16* Src = reinterpret_cast<const uint16*>(SrcRow);
            for (unsigned X = 0; X < Width; ++X)
            {
                const uint16 P = Src[X];
                const uint8 R = static_cast<uint8>(((P >> 10) & 0x1F) * 255 / 31);
                const uint8 G = static_cast<uint8>(((P >> 5) & 0x1F) * 255 / 31);
                const uint8 B = static_cast<uint8>((P & 0x1F) * 255 / 31);
                DstRow[X * 4 + 0] = B;
                DstRow[X * 4 + 1] = G;
                DstRow[X * 4 + 2] = R;
                DstRow[X * 4 + 3] = 255;
                BrightnessSum += R + G + B;
                NonBlackPixels += (R | G | B) ? 1u : 0u;
            }
        }
        else
        {
            const uint32* Src = reinterpret_cast<const uint32*>(SrcRow);
            for (unsigned X = 0; X < Width; ++X)
            {
                const uint32 P = Src[X];
                const uint8 R = static_cast<uint8>((P >> 16) & 0xFF);
                const uint8 G = static_cast<uint8>((P >> 8) & 0xFF);
                const uint8 B = static_cast<uint8>(P & 0xFF);
                DstRow[X * 4 + 0] = B;
                DstRow[X * 4 + 1] = G;
                DstRow[X * 4 + 2] = R;
                DstRow[X * 4 + 3] = 255;
                BrightnessSum += R + G + B;
                NonBlackPixels += (R | G | B) ? 1u : 0u;
            }
        }
    }

    ++VideoFrameCounter;
    if (VideoFrameCounter == 1 || VideoFrameCounter == 60 || VideoFrameCounter == 180 || VideoFrameCounter == 360)
    {
        UE_LOG(LogLibretroRunner, Log, TEXT("Frame %llu stats: nonblack=%u brightness=%llu"),
            static_cast<unsigned long long>(VideoFrameCounter),
            NonBlackPixels,
            static_cast<unsigned long long>(BrightnessSum));
    }

    {
        FScopeLock Lock(&FrameMutex);
        FrameBGRA = MoveTemp(Converted);
        FrameWidth = Width;
        FrameHeight = Height;
        bNewFrame = true;
    }
}

void FLibretroNESRunner::QueueAudio(const int16* Data, size_t Frames)
{
    if (!Data || Frames == 0 || !SoundWave)
    {
        return;
    }

    SoundWave->QueueAudio(reinterpret_cast<const uint8*>(Data), static_cast<int32>(Frames * 2 * sizeof(int16)));
}

void FLibretroNESRunner::SetButtonState(ELibretroButton Button, bool bPressed)
{
    const uint8 Index = static_cast<uint8>(Button);
    if (Index >= static_cast<uint8>(ELibretroButton::Count))
    {
        return;
    }

    FScopeLock Lock(&StateMutex);
    ButtonStates[Index] = bPressed;
}

void FLibretroNESRunner::SetPointerState(float NormalizedX, float NormalizedY, bool bPressed)
{
    FScopeLock Lock(&StateMutex);
    PointerX = FMath::Clamp(NormalizedX, 0.0f, 1.0f);
    PointerY = FMath::Clamp(NormalizedY, 0.0f, 1.0f);
    bPointerPressed = bPressed;
}

int16 FLibretroNESRunner::QueryInput(unsigned Port, unsigned Device, unsigned Index, unsigned Id) const
{
    if (Port != 0)
    {
        return 0;
    }

    const unsigned BaseDevice = Device & RETRO_DEVICE_MASK;
    FScopeLock Lock(&StateMutex);

    if (BaseDevice == RETRO_DEVICE_JOYPAD)
    {
        if (Id == RETRO_DEVICE_ID_JOYPAD_MASK)
        {
            uint16 Mask = 0;
            for (uint8 ButtonIndex = 0; ButtonIndex < static_cast<uint8>(ELibretroButton::Count); ++ButtonIndex)
            {
                if (ButtonStates[ButtonIndex])
                {
                    Mask |= (1u << ButtonIndex);
                }
            }
            return static_cast<int16>(Mask);
        }

        if (Id < static_cast<unsigned>(ELibretroButton::Count))
        {
            return ButtonStates[Id] ? 1 : 0;
        }
        return 0;
    }

    if (BaseDevice == RETRO_DEVICE_ANALOG)
    {
        const bool bLeftStick = Index == RETRO_DEVICE_INDEX_ANALOG_LEFT;
        if (bLeftStick && Id == RETRO_DEVICE_ID_ANALOG_X)
        {
            if (ButtonStates[static_cast<uint8>(ELibretroButton::Left)])
            {
                return -0x7fff;
            }
            if (ButtonStates[static_cast<uint8>(ELibretroButton::Right)])
            {
                return 0x7fff;
            }
        }
        if (bLeftStick && Id == RETRO_DEVICE_ID_ANALOG_Y)
        {
            if (ButtonStates[static_cast<uint8>(ELibretroButton::Up)])
            {
                return -0x7fff;
            }
            if (ButtonStates[static_cast<uint8>(ELibretroButton::Down)])
            {
                return 0x7fff;
            }
        }
    }

    if (BaseDevice == RETRO_DEVICE_POINTER)
    {
        if (Index > 0)
        {
            return 0;
        }

        switch (Id)
        {
        case RETRO_DEVICE_ID_POINTER_X:
            return static_cast<int16>(FMath::RoundToInt(FMath::Lerp(-32767.0f, 32767.0f, PointerX)));
        case RETRO_DEVICE_ID_POINTER_Y:
            return static_cast<int16>(FMath::RoundToInt(FMath::Lerp(-32767.0f, 32767.0f, PointerY)));
        case RETRO_DEVICE_ID_POINTER_PRESSED:
            return bPointerPressed ? 1 : 0;
        case RETRO_DEVICE_ID_POINTER_IS_OFFSCREEN:
            return 0;
        default:
            return 0;
        }
    }

    return 0;
}

bool FLibretroNESRunner::HandleEnvironment(unsigned Cmd, void* Data)
{
    switch (Cmd)
    {
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
        PixelFormat = *static_cast<retro_pixel_format*>(Data);
        return PixelFormat == RETRO_PIXEL_FORMAT_XRGB8888 ||
            PixelFormat == RETRO_PIXEL_FORMAT_RGB565 ||
            PixelFormat == RETRO_PIXEL_FORMAT_0RGB1555;

    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        *static_cast<const char**>(Data) = SystemDirUtf8 ? SystemDirUtf8->Get() : nullptr;
        return true;

    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        *static_cast<const char**>(Data) = SaveDirUtf8 ? SaveDirUtf8->Get() : nullptr;
        return true;

    case RETRO_ENVIRONMENT_GET_CONTENT_DIRECTORY:
        *static_cast<const char**>(Data) = ContentDirUtf8 ? ContentDirUtf8->Get() : nullptr;
        return true;

    case RETRO_ENVIRONMENT_GET_LIBRETRO_PATH:
        *static_cast<const char**>(Data) = LibretroPathUtf8 ? LibretroPathUtf8->Get() : nullptr;
        return true;

    case RETRO_ENVIRONMENT_SET_VARIABLES:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK:
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
    case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
    case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
    case RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS:
        return true;

    case RETRO_ENVIRONMENT_GET_VARIABLE:
    {
        retro_variable* Variable = static_cast<retro_variable*>(Data);
        if (!Variable || !Variable->key)
        {
            return false;
        }

        Variable->value = FindCoreOptionValue(Variable->key);
        return Variable->value != nullptr;
    }

    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
        *static_cast<bool*>(Data) = false;
        return true;

    case RETRO_ENVIRONMENT_GET_OVERSCAN:
        *static_cast<bool*>(Data) = false;
        return true;

    case RETRO_ENVIRONMENT_GET_CAN_DUPE:
        *static_cast<bool*>(Data) = true;
        return true;

    case RETRO_ENVIRONMENT_SET_GEOMETRY:
    {
        const retro_game_geometry* Geometry = static_cast<const retro_game_geometry*>(Data);
        if (Geometry)
        {
            AvInfo.geometry = *Geometry;
        }
        return true;
    }

    case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
    {
        const retro_system_av_info* Info = static_cast<const retro_system_av_info*>(Data);
        if (Info)
        {
            AvInfo = *Info;
            TargetFps = Info->timing.fps > 1.0 ? Info->timing.fps : TargetFps;
            TargetSampleRate = Info->timing.sample_rate > 1.0 ? Info->timing.sample_rate : TargetSampleRate;
        }
        return true;
    }

    case RETRO_ENVIRONMENT_GET_INPUT_DEVICE_CAPABILITIES:
        *static_cast<uint64*>(Data) =
            (1ULL << RETRO_DEVICE_JOYPAD) |
            (1ULL << RETRO_DEVICE_ANALOG) |
            (1ULL << RETRO_DEVICE_POINTER) |
            (1ULL << RETRO_DEVICE_KEYBOARD);
        return true;

    case RETRO_ENVIRONMENT_GET_INPUT_MAX_USERS:
        *static_cast<unsigned*>(Data) = 1;
        return true;

    case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
        return true;

    case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
        *static_cast<unsigned*>(Data) = 2;
        return true;

    case RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION:
        *static_cast<unsigned*>(Data) = 1;
        return true;

    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
    {
        retro_log_callback* Log = static_cast<retro_log_callback*>(Data);
        if (Log)
        {
            Log->log = &FLibretroNESRunner::RetroLog;
            return true;
        }
        return false;
    }

    case RETRO_ENVIRONMENT_SET_MESSAGE:
    case RETRO_ENVIRONMENT_SET_MESSAGE_EXT:
        return true;

    case RETRO_ENVIRONMENT_GET_LANGUAGE:
        *static_cast<unsigned*>(Data) = RETRO_LANGUAGE_ENGLISH;
        return true;

    case RETRO_ENVIRONMENT_GET_JIT_CAPABLE:
        *static_cast<bool*>(Data) = true;
        return true;

    case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE:
        *static_cast<int*>(Data) = 3;
        return true;

    default:
        UE_LOG(LogLibretroRunner, Verbose, TEXT("Unhandled libretro environment cmd: %u"), Cmd);
        return false;
    }
}

const char* FLibretroNESRunner::FindCoreOptionValue(const char* Key) const
{
    const FString KeyString = UTF8_TO_TCHAR(Key);
    if (const TArray<ANSICHAR>* Value = CoreOptionValueUtf8.Find(KeyString))
    {
        return Value->GetData();
    }
    return nullptr;
}

void FLibretroNESRunner::SetError(const FString& Error)
{
    UE_LOG(LogLibretroRunner, Error, TEXT("%s"), *Error);
    FScopeLock Lock(&StateMutex);
    LastError = Error;
    StatusText = Error;
}

void FLibretroNESRunner::SetStatus(const FString& Status)
{
    FScopeLock Lock(&StateMutex);
    StatusText = Status;
}

FString FLibretroNESRunner::GetLastError() const
{
    FScopeLock Lock(&StateMutex);
    return LastError;
}

FString FLibretroNESRunner::GetStatusText() const
{
    FScopeLock Lock(&StateMutex);
    return StatusText;
}

FString FLibretroNESRunner::GetLoadedRomPath() const
{
    FScopeLock Lock(&StateMutex);
    return RomPath;
}

FString FLibretroNESRunner::GetLoadedCorePath() const
{
    FScopeLock Lock(&StateMutex);
    return CorePath;
}

bool FLibretroNESRunner::RetroEnvironment(unsigned Cmd, void* Data)
{
    return GActiveLibretroRunner ? GActiveLibretroRunner->HandleEnvironment(Cmd, Data) : false;
}

void FLibretroNESRunner::RetroVideoRefresh(const void* Data, unsigned Width, unsigned Height, size_t Pitch)
{
    if (GActiveLibretroRunner && Data)
    {
        GActiveLibretroRunner->CopyVideoFrame(Data, Width, Height, Pitch);
    }
}

void FLibretroNESRunner::RetroAudioSample(int16 Left, int16 Right)
{
    int16 Samples[2] = { Left, Right };
    if (GActiveLibretroRunner)
    {
        GActiveLibretroRunner->QueueAudio(Samples, 1);
    }
}

size_t FLibretroNESRunner::RetroAudioSampleBatch(const int16* Data, size_t Frames)
{
    if (GActiveLibretroRunner)
    {
        GActiveLibretroRunner->QueueAudio(Data, Frames);
    }
    return Frames;
}

void FLibretroNESRunner::RetroInputPoll()
{
}

int16 FLibretroNESRunner::RetroInputState(unsigned Port, unsigned Device, unsigned Index, unsigned Id)
{
    return GActiveLibretroRunner ? GActiveLibretroRunner->QueryInput(Port, Device, Index, Id) : 0;
}

void FLibretroNESRunner::RetroLog(enum retro_log_level Level, const char* Fmt, ...)
{
    ANSICHAR Buffer[2048];
    va_list Args;
    va_start(Args, Fmt);
    FCStringAnsi::GetVarArgs(Buffer, UE_ARRAY_COUNT(Buffer), Fmt, Args);
    va_end(Args);

    const TCHAR* Message = UTF8_TO_TCHAR(Buffer);
    switch (Level)
    {
    case RETRO_LOG_ERROR:
        UE_LOG(LogLibretroRunner, Error, TEXT("%s"), Message);
        break;
    case RETRO_LOG_WARN:
        UE_LOG(LogLibretroRunner, Warning, TEXT("%s"), Message);
        break;
    case RETRO_LOG_DEBUG:
        UE_LOG(LogLibretroRunner, Verbose, TEXT("%s"), Message);
        break;
    default:
        UE_LOG(LogLibretroRunner, Log, TEXT("%s"), Message);
        break;
    }
}
