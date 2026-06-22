#include "LibretroRunner.h"

#include "Async/Async.h"
#include "Engine/Texture2D.h"
#include "HAL/PlatformProcess.h"
#include "Logging/LogMacros.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeLock.h"
#include "RHITypes.h"
#include "Serialization/Archive.h"
#include "Sound/SoundWaveProcedural.h"

DEFINE_LOG_CATEGORY_STATIC(LogLibretroRunner, Log, All);

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/WindowsHWrapper.h"
#include <gl/GL.h>
#include "Windows/HideWindowsPlatformTypes.h"
#pragma comment(lib, "opengl32.lib")

// 这里直接声明少量 OpenGL/WGL 常量和函数指针，避免额外引入 OpenGL
// 加载库。Azahar 通过 libretro HW render 回调拿到的是 OpenGL 函数地址和
// 帧缓冲，因此本前端只需要创建隐藏上下文、提供帧缓冲、读回像素。
#ifndef WGL_CONTEXT_MAJOR_VERSION_ARB
#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#endif
#ifndef WGL_CONTEXT_MINOR_VERSION_ARB
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#endif
#ifndef WGL_CONTEXT_PROFILE_MASK_ARB
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#endif
#ifndef WGL_CONTEXT_CORE_PROFILE_BIT_ARB
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#endif
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#endif
#ifndef GL_RENDERBUFFER
#define GL_RENDERBUFFER 0x8D41
#endif
#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0 0x8CE0
#endif
#ifndef GL_DEPTH_STENCIL_ATTACHMENT
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A
#endif
#ifndef GL_DEPTH_ATTACHMENT
#define GL_DEPTH_ATTACHMENT 0x8D00
#endif
#ifndef GL_STENCIL_ATTACHMENT
#define GL_STENCIL_ATTACHMENT 0x8D20
#endif
#ifndef GL_DEPTH24_STENCIL8
#define GL_DEPTH24_STENCIL8 0x88F0
#endif
#ifndef GL_DEPTH_COMPONENT24
#define GL_DEPTH_COMPONENT24 0x81A6
#endif
#ifndef GL_TEXTURE_2D
#define GL_TEXTURE_2D 0x0DE1
#endif
#ifndef GL_TEXTURE_MIN_FILTER
#define GL_TEXTURE_MIN_FILTER 0x2801
#endif
#ifndef GL_TEXTURE_MAG_FILTER
#define GL_TEXTURE_MAG_FILTER 0x2800
#endif
#ifndef GL_TEXTURE_WRAP_S
#define GL_TEXTURE_WRAP_S 0x2802
#endif
#ifndef GL_TEXTURE_WRAP_T
#define GL_TEXTURE_WRAP_T 0x2803
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_RGBA8
#define GL_RGBA8 0x8058
#endif
#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif
#ifndef GL_UNSIGNED_INT_8_8_8_8_REV
#define GL_UNSIGNED_INT_8_8_8_8_REV 0x8367
#endif
#ifndef GL_READ_FRAMEBUFFER
#define GL_READ_FRAMEBUFFER 0x8CA8
#endif
#ifndef GL_DRAW_FRAMEBUFFER
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#endif
#ifndef GL_FRAMEBUFFER_COMPLETE
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#endif

typedef HGLRC(WINAPI* PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC, HGLRC, const int*);
typedef void (APIENTRY* PFNGLGENFRAMEBUFFERSPROC)(GLsizei, GLuint*);
typedef void (APIENTRY* PFNGLDELETEFRAMEBUFFERSPROC)(GLsizei, const GLuint*);
typedef void (APIENTRY* PFNGLBINDFRAMEBUFFERPROC)(GLenum, GLuint);
typedef void (APIENTRY* PFNGLFRAMEBUFFERTEXTURE2DPROC)(GLenum, GLenum, GLenum, GLuint, GLint);
typedef void (APIENTRY* PFNGLGENRENDERBUFFERSPROC)(GLsizei, GLuint*);
typedef void (APIENTRY* PFNGLDELETERENDERBUFFERSPROC)(GLsizei, const GLuint*);
typedef void (APIENTRY* PFNGLBINDRENDERBUFFERPROC)(GLenum, GLuint);
typedef void (APIENTRY* PFNGLRENDERBUFFERSTORAGEPROC)(GLenum, GLenum, GLsizei, GLsizei);
typedef void (APIENTRY* PFNGLFRAMEBUFFERRENDERBUFFERPROC)(GLenum, GLenum, GLenum, GLuint);
typedef GLenum(APIENTRY* PFNGLCHECKFRAMEBUFFERSTATUSPROC)(GLenum);
typedef void (APIENTRY* PFNGLGENVERTEXARRAYSPROC)(GLsizei, GLuint*);
typedef void (APIENTRY* PFNGLDELETEVERTEXARRAYSPROC)(GLsizei, const GLuint*);
typedef void (APIENTRY* PFNGLBINDVERTEXARRAYPROC)(GLuint);

/**
 * Windows OpenGL 硬件渲染桥接。
 *
 * libretro 硬件渲染 core 不会直接渲染到 UE 的 RHI 纹理；它只要求前端
 * 提供一个 OpenGL 上下文、当前帧缓冲和 GL 函数地址。这个类创建一个
 * 1x1 隐藏 Win32 窗口和 OpenGL context，再为 core 准备可调整尺寸的 FBO。
 * 每帧结束后 Runner 会把 FBO 像素读回为 BGRA，再上传到 UTexture2D。
 */
class FLibretroOpenGLRenderContext
{
public:
    bool Initialize(const retro_hw_render_callback& Callback, unsigned InitialWidth, unsigned InitialHeight)
    {
        RequestedCallback = Callback;
        InitialWidth = FMath::Max(InitialWidth, 1u);
        InitialHeight = FMath::Max(InitialHeight, 1u);

        if (RequestedCallback.context_type != RETRO_HW_CONTEXT_OPENGL &&
            RequestedCallback.context_type != RETRO_HW_CONTEXT_OPENGL_CORE)
        {
            UE_LOG(LogLibretroRunner, Warning, TEXT("Unsupported libretro HW context requested: %d"), static_cast<int32>(RequestedCallback.context_type));
            return false;
        }

        if (!CreateWindowAndContext())
        {
            return false;
        }

        if (!LoadFramebufferFunctions())
        {
            UE_LOG(LogLibretroRunner, Error, TEXT("OpenGL framebuffer entry points are unavailable"));
            Shutdown();
            return false;
        }

        Resize(InitialWidth, InitialHeight);
        if (!Framebuffer)
        {
            Shutdown();
            return false;
        }

        if (RequestedCallback.context_reset)
        {
            RequestedCallback.context_reset();
        }

        bValid = true;
        UE_LOG(LogLibretroRunner, Log, TEXT("Initialized libretro OpenGL HW context %ux%u"), InitialWidth, InitialHeight);
        return true;
    }

    void Shutdown()
    {
        if (!MakeCurrent())
        {
            UE_LOG(LogLibretroRunner, Verbose, TEXT("OpenGL context was not current during shutdown"));
        }

        if (bValid && RequestedCallback.context_destroy)
        {
            RequestedCallback.context_destroy();
        }

        if (glDeleteVertexArraysPtr && VertexArray)
        {
            glDeleteVertexArraysPtr(1, &VertexArray);
            VertexArray = 0;
        }
        if (glDeleteRenderbuffersPtr && DepthStencilRenderbuffer)
        {
            glDeleteRenderbuffersPtr(1, &DepthStencilRenderbuffer);
            DepthStencilRenderbuffer = 0;
        }
        if (glDeleteFramebuffersPtr && Framebuffer)
        {
            glDeleteFramebuffersPtr(1, &Framebuffer);
            Framebuffer = 0;
        }
        if (ColorTexture)
        {
            glDeleteTextures(1, &ColorTexture);
            ColorTexture = 0;
        }

        if (GLContext)
        {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(GLContext);
            GLContext = nullptr;
        }
        if (DeviceContext && WindowHandle)
        {
            ReleaseDC(WindowHandle, DeviceContext);
            DeviceContext = nullptr;
        }
        if (WindowHandle)
        {
            DestroyWindow(WindowHandle);
            WindowHandle = nullptr;
        }
        if (WindowClassAtom)
        {
            UnregisterClass(GetWindowClassName(), GetModuleHandle(nullptr));
            WindowClassAtom = 0;
        }

        bValid = false;
    }

    bool IsValid() const
    {
        return bValid;
    }

    bool MakeCurrent() const
    {
        return DeviceContext && GLContext && wglMakeCurrent(DeviceContext, GLContext);
    }

    uintptr_t GetCurrentFramebuffer()
    {
        if (!MakeCurrent())
        {
            return 0;
        }
        return static_cast<uintptr_t>(Framebuffer);
    }

    retro_proc_address_t GetProcAddress(const char* Sym) const
    {
        if (!Sym)
        {
            return nullptr;
        }

        PROC Proc = wglGetProcAddress(Sym);
        if (!Proc || Proc == reinterpret_cast<PROC>(1) || Proc == reinterpret_cast<PROC>(2) ||
            Proc == reinterpret_cast<PROC>(3) || Proc == reinterpret_cast<PROC>(-1))
        {
            HMODULE OpenGL = GetModuleHandle(TEXT("opengl32.dll"));
            Proc = OpenGL ? ::GetProcAddress(OpenGL, Sym) : nullptr;
        }

        return reinterpret_cast<retro_proc_address_t>(Proc);
    }

    void Resize(unsigned NewWidth, unsigned NewHeight)
    {
        NewWidth = FMath::Max(NewWidth, 1u);
        NewHeight = FMath::Max(NewHeight, 1u);
        if (!MakeCurrent())
        {
            return;
        }

        if (NewWidth == Width && NewHeight == Height && Framebuffer && ColorTexture)
        {
            glBindFramebufferPtr(GL_FRAMEBUFFER, Framebuffer);
            glViewport(0, 0, Width, Height);
            return;
        }

        Width = NewWidth;
        Height = NewHeight;

        // libretro core 可能在 SET_GEOMETRY 或视频回调中改变输出尺寸，
        // 因此 FBO、颜色纹理和深度/模板附件都按当前帧尺寸延迟重建。
        if (!Framebuffer)
        {
            glGenFramebuffersPtr(1, &Framebuffer);
        }
        if (!ColorTexture)
        {
            glGenTextures(1, &ColorTexture);
        }

        glBindTexture(GL_TEXTURE_2D, ColorTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, Width, Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        glBindFramebufferPtr(GL_FRAMEBUFFER, Framebuffer);
        glFramebufferTexture2DPtr(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ColorTexture, 0);

        if (RequestedCallback.depth || RequestedCallback.stencil)
        {
            if (!DepthStencilRenderbuffer)
            {
                glGenRenderbuffersPtr(1, &DepthStencilRenderbuffer);
            }
            glBindRenderbufferPtr(GL_RENDERBUFFER, DepthStencilRenderbuffer);
            const GLenum StorageFormat = (RequestedCallback.depth && RequestedCallback.stencil) ? GL_DEPTH24_STENCIL8 : GL_DEPTH_COMPONENT24;
            glRenderbufferStoragePtr(GL_RENDERBUFFER, StorageFormat, Width, Height);

            if (RequestedCallback.depth && RequestedCallback.stencil)
            {
                glFramebufferRenderbufferPtr(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, DepthStencilRenderbuffer);
            }
            else if (RequestedCallback.depth)
            {
                glFramebufferRenderbufferPtr(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, DepthStencilRenderbuffer);
            }
            else if (RequestedCallback.stencil)
            {
                glFramebufferRenderbufferPtr(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, DepthStencilRenderbuffer);
            }
        }

        const GLenum Status = glCheckFramebufferStatusPtr ? glCheckFramebufferStatusPtr(GL_FRAMEBUFFER) : 0;
        if (Status != GL_FRAMEBUFFER_COMPLETE)
        {
            UE_LOG(LogLibretroRunner, Error, TEXT("OpenGL framebuffer incomplete: 0x%04x"), Status);
            return;
        }

        if (glGenVertexArraysPtr && !VertexArray)
        {
            glGenVertexArraysPtr(1, &VertexArray);
        }
        if (glBindVertexArrayPtr && VertexArray)
        {
            glBindVertexArrayPtr(VertexArray);
        }

        glViewport(0, 0, Width, Height);
    }

    bool ReadFrame(TArray<uint8>& OutBGRA, unsigned ReadWidth, unsigned ReadHeight)
    {
        ReadWidth = FMath::Max(ReadWidth, 1u);
        ReadHeight = FMath::Max(ReadHeight, 1u);
        Resize(ReadWidth, ReadHeight);

        if (!Framebuffer || !MakeCurrent())
        {
            return false;
        }

        glBindFramebufferPtr(GL_FRAMEBUFFER, Framebuffer);
        OutBGRA.SetNumUninitialized(ReadWidth * ReadHeight * 4);

        TArray<uint8> RawBottomUp;
        RawBottomUp.SetNumUninitialized(OutBGRA.Num());
        glReadPixels(0, 0, ReadWidth, ReadHeight, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, RawBottomUp.GetData());

        // OpenGL 原点通常在左下角，而 UE 纹理上传按左上角行序处理。
        // bottom_left_origin 由 core 指明，因此这里按需翻转行顺序。
        const uint32 RowBytes = ReadWidth * 4;
        for (unsigned Y = 0; Y < ReadHeight; ++Y)
        {
            const unsigned SrcY = RequestedCallback.bottom_left_origin ? (ReadHeight - 1 - Y) : Y;
            FMemory::Memcpy(OutBGRA.GetData() + Y * RowBytes, RawBottomUp.GetData() + SrcY * RowBytes, RowBytes);
        }

        return true;
    }

private:
    static const TCHAR* GetWindowClassName()
    {
        return TEXT("UE_libretro_Hidden_GL_Context");
    }

    static LRESULT CALLBACK WindowProc(HWND Hwnd, UINT Message, WPARAM WParam, LPARAM LParam)
    {
        return DefWindowProc(Hwnd, Message, WParam, LParam);
    }

    bool CreateWindowAndContext()
    {
        HINSTANCE Instance = GetModuleHandle(nullptr);

        // WGL 需要一个窗口 DC 才能创建 OpenGL context；这个窗口不显示，只作为
        // core 渲染用的离屏上下文载体。
        WNDCLASS WindowClass = {};
        WindowClass.style = CS_OWNDC;
        WindowClass.lpfnWndProc = &FLibretroOpenGLRenderContext::WindowProc;
        WindowClass.hInstance = Instance;
        WindowClass.lpszClassName = GetWindowClassName();
        WindowClassAtom = RegisterClass(&WindowClass);
        if (!WindowClassAtom && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        {
            UE_LOG(LogLibretroRunner, Error, TEXT("Failed to register hidden OpenGL window class"));
            return false;
        }

        WindowHandle = CreateWindowEx(
            0,
            GetWindowClassName(),
            TEXT("UE libretro OpenGL"),
            WS_OVERLAPPEDWINDOW,
            0,
            0,
            1,
            1,
            nullptr,
            nullptr,
            Instance,
            nullptr);
        if (!WindowHandle)
        {
            UE_LOG(LogLibretroRunner, Error, TEXT("Failed to create hidden OpenGL window"));
            return false;
        }

        DeviceContext = GetDC(WindowHandle);
        if (!DeviceContext)
        {
            UE_LOG(LogLibretroRunner, Error, TEXT("Failed to acquire OpenGL device context"));
            return false;
        }

        PIXELFORMATDESCRIPTOR PixelFormatDesc = {};
        PixelFormatDesc.nSize = sizeof(PixelFormatDesc);
        PixelFormatDesc.nVersion = 1;
        PixelFormatDesc.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        PixelFormatDesc.iPixelType = PFD_TYPE_RGBA;
        PixelFormatDesc.cColorBits = 32;
        PixelFormatDesc.cDepthBits = RequestedCallback.depth ? 24 : 0;
        PixelFormatDesc.cStencilBits = RequestedCallback.stencil ? 8 : 0;
        PixelFormatDesc.iLayerType = PFD_MAIN_PLANE;

        const int PixelFormatIndex = ChoosePixelFormat(DeviceContext, &PixelFormatDesc);
        if (PixelFormatIndex == 0 || !SetPixelFormat(DeviceContext, PixelFormatIndex, &PixelFormatDesc))
        {
            UE_LOG(LogLibretroRunner, Error, TEXT("Failed to set OpenGL pixel format"));
            return false;
        }

        HGLRC TemporaryContext = wglCreateContext(DeviceContext);
        if (!TemporaryContext || !wglMakeCurrent(DeviceContext, TemporaryContext))
        {
            UE_LOG(LogLibretroRunner, Error, TEXT("Failed to create temporary OpenGL context"));
            return false;
        }

        PFNWGLCREATECONTEXTATTRIBSARBPROC CreateContextAttribs =
            reinterpret_cast<PFNWGLCREATECONTEXTATTRIBSARBPROC>(wglGetProcAddress("wglCreateContextAttribsARB"));

        // 先用临时 context 拿到 wglCreateContextAttribsARB，再尽量创建 core
        // 请求的 OpenGL 版本。若驱动不支持扩展，则退回临时 context。
        if (CreateContextAttribs)
        {
            const int Major = static_cast<int>(RequestedCallback.version_major ? RequestedCallback.version_major : 3);
            const int Minor = static_cast<int>(RequestedCallback.version_minor ? RequestedCallback.version_minor : 3);
            const bool bCore = RequestedCallback.context_type == RETRO_HW_CONTEXT_OPENGL_CORE;
            const int Attributes[] =
            {
                WGL_CONTEXT_MAJOR_VERSION_ARB, Major,
                WGL_CONTEXT_MINOR_VERSION_ARB, Minor,
                WGL_CONTEXT_PROFILE_MASK_ARB, bCore ? WGL_CONTEXT_CORE_PROFILE_BIT_ARB : 0,
                0
            };

            GLContext = CreateContextAttribs(DeviceContext, nullptr, Attributes);
        }

        if (!GLContext)
        {
            GLContext = TemporaryContext;
            TemporaryContext = nullptr;
        }

        wglMakeCurrent(nullptr, nullptr);
        if (TemporaryContext)
        {
            wglDeleteContext(TemporaryContext);
        }

        if (!wglMakeCurrent(DeviceContext, GLContext))
        {
            UE_LOG(LogLibretroRunner, Error, TEXT("Failed to activate OpenGL context"));
            return false;
        }

        return true;
    }

    template <typename T>
    bool LoadGlFunction(T& OutFunction, const char* Name)
    {
        OutFunction = reinterpret_cast<T>(GetProcAddress(Name));
        return OutFunction != nullptr;
    }

    bool LoadFramebufferFunctions()
    {
        return LoadGlFunction(glGenFramebuffersPtr, "glGenFramebuffers") &&
            LoadGlFunction(glDeleteFramebuffersPtr, "glDeleteFramebuffers") &&
            LoadGlFunction(glBindFramebufferPtr, "glBindFramebuffer") &&
            LoadGlFunction(glFramebufferTexture2DPtr, "glFramebufferTexture2D") &&
            LoadGlFunction(glGenRenderbuffersPtr, "glGenRenderbuffers") &&
            LoadGlFunction(glDeleteRenderbuffersPtr, "glDeleteRenderbuffers") &&
            LoadGlFunction(glBindRenderbufferPtr, "glBindRenderbuffer") &&
            LoadGlFunction(glRenderbufferStoragePtr, "glRenderbufferStorage") &&
            LoadGlFunction(glFramebufferRenderbufferPtr, "glFramebufferRenderbuffer") &&
            LoadGlFunction(glCheckFramebufferStatusPtr, "glCheckFramebufferStatus") &&
            LoadGlFunction(glGenVertexArraysPtr, "glGenVertexArrays") &&
            LoadGlFunction(glDeleteVertexArraysPtr, "glDeleteVertexArrays") &&
            LoadGlFunction(glBindVertexArrayPtr, "glBindVertexArray");
    }

private:
    retro_hw_render_callback RequestedCallback = {};
    ATOM WindowClassAtom = 0;
    HWND WindowHandle = nullptr;
    HDC DeviceContext = nullptr;
    HGLRC GLContext = nullptr;
    GLuint Framebuffer = 0;
    GLuint ColorTexture = 0;
    GLuint DepthStencilRenderbuffer = 0;
    GLuint VertexArray = 0;
    unsigned Width = 0;
    unsigned Height = 0;
    bool bValid = false;

    PFNGLGENFRAMEBUFFERSPROC glGenFramebuffersPtr = nullptr;
    PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffersPtr = nullptr;
    PFNGLBINDFRAMEBUFFERPROC glBindFramebufferPtr = nullptr;
    PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2DPtr = nullptr;
    PFNGLGENRENDERBUFFERSPROC glGenRenderbuffersPtr = nullptr;
    PFNGLDELETERENDERBUFFERSPROC glDeleteRenderbuffersPtr = nullptr;
    PFNGLBINDRENDERBUFFERPROC glBindRenderbufferPtr = nullptr;
    PFNGLRENDERBUFFERSTORAGEPROC glRenderbufferStoragePtr = nullptr;
    PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbufferPtr = nullptr;
    PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatusPtr = nullptr;
    PFNGLGENVERTEXARRAYSPROC glGenVertexArraysPtr = nullptr;
    PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArraysPtr = nullptr;
    PFNGLBINDVERTEXARRAYPROC glBindVertexArrayPtr = nullptr;
};
#endif

// libretro 的回调都是 C 函数指针，没有用户数据参数。本项目一次只运行一个
// core，因此用这个指针把静态回调转发到当前活动 Runner 实例。
static FLibretroRunner* GActiveLibretroRunner = nullptr;

namespace
{
    // 运行速度只提供几个固定档位，避免任意倍率造成音频采样率和帧节流难以判断。
    constexpr double LibretroSpeedLevels[] = { 0.5, 1.0, 1.5, 2.0 };
}

FLibretroRunner::FLibretroRunner()
{
    const FString ProjectDir = FPaths::ProjectDir();
    CorePath = FPaths::ConvertRelativePathToFull(ProjectDir / TEXT("ThirdParty/Libretro/Cores/Win64/fceumm_libretro.dll"));
    SystemDir = FPaths::ConvertRelativePathToFull(ProjectDir / TEXT("Saved/Libretro/System"));
    SaveDir = FPaths::ConvertRelativePathToFull(ProjectDir / TEXT("Saved/Libretro/Saves"));
    StateDir = FPaths::ConvertRelativePathToFull(ProjectDir / TEXT("Saved/Libretro/States"));
    CoreAssetsDir = FPaths::ConvertRelativePathToFull(ProjectDir / TEXT("Saved/Libretro/Cache"));
    LibretroPath = CorePath;

    IFileManager::Get().MakeDirectory(*SystemDir, true);
    IFileManager::Get().MakeDirectory(*SaveDir, true);
    IFileManager::Get().MakeDirectory(*StateDir, true);
    IFileManager::Get().MakeDirectory(*CoreAssetsDir, true);

    SystemDirUtf8 = new FTCHARToUTF8(*SystemDir);
    SaveDirUtf8 = new FTCHARToUTF8(*SaveDir);
    CoreAssetsDirUtf8 = new FTCHARToUTF8(*CoreAssetsDir);
    LibretroPathUtf8 = new FTCHARToUTF8(*LibretroPath);
}

FLibretroRunner::~FLibretroRunner()
{
    StopAndUnload();
    delete SystemDirUtf8;
    delete SaveDirUtf8;
    delete ContentDirUtf8;
    delete CoreAssetsDirUtf8;
    delete LibretroPathUtf8;
}

bool FLibretroRunner::Start(const FString& InRomPath)
{
    FLibretroLaunchConfig Config;
    Config.RomPath = InRomPath;
    Config.CorePath = FPaths::ProjectDir() / TEXT("ThirdParty/Libretro/Cores/Win64/fceumm_libretro.dll");
    Config.DisplayName = TEXT("重装机兵1");
    Config.SystemType = ELibretroSystemType::NES;
    return Start(Config);
}

bool FLibretroRunner::Start(const FLibretroLaunchConfig& Config)
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
    bQuickSaveRequested = false;
    bQuickLoadRequested = false;
    bLoggedFirstFrame = false;
    VideoFrameCounter = 0;
    ActualRunFps = 0.0;
    AverageRetroRunMs = 0.0;
    FrameTimeCallback = {};
    SetStatus(FString::Printf(TEXT("正在启动 %s..."), *LaunchConfig.DisplayName));

    // libretro core 的生命周期和 retro_run() 放到独立线程，避免阻塞 UE 游戏线程。
    // 纹理和音频 UObject 仍通过 AsyncTask/Tick 回到游戏线程处理。
    Thread = FRunnableThread::Create(this, TEXT("LibretroRunner"), 0, TPri_Normal);
    if (!Thread)
    {
        SetError(TEXT("无法创建 libretro 运行线程"));
        return false;
    }

    return true;
}

bool FLibretroRunner::PrepareLaunch(const FLibretroLaunchConfig& Config)
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

    // libretro 要求返回 const char*，所以 core 选项值必须拥有稳定生命周期，
    // 不能直接返回临时 FTCHARToUTF8 缓冲区。
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

    OpenGLRenderContext.Reset();

    return true;
}

FString FLibretroRunner::ResolvePath(const FString& Path) const
{
    if (FPaths::IsRelative(Path))
    {
        return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / Path);
    }

    return FPaths::ConvertRelativePathToFull(Path);
}

void FLibretroRunner::StopAndUnload()
{
    bStopRequested = true;

    if (Thread)
    {
        Thread->WaitForCompletion();
        delete Thread;
        Thread = nullptr;
    }

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

void FLibretroRunner::Reset()
{
    bResetRequested = true;
}

void FLibretroRunner::RequestQuickSave()
{
    if (!Thread && !bRunning)
    {
        SetStatus(TEXT("当前没有运行中的 ROM，无法即时存档"));
        return;
    }

    bQuickSaveRequested = true;
    SetStatus(TEXT("正在请求即时存档..."));
}

void FLibretroRunner::RequestQuickLoad()
{
    if (!Thread && !bRunning)
    {
        SetStatus(TEXT("当前没有运行中的 ROM，无法即时读档"));
        return;
    }

    bQuickLoadRequested = true;
    SetStatus(TEXT("正在请求即时读档..."));
}

void FLibretroRunner::DecreaseSpeed()
{
    const int32 NewSpeedIndex = [this]()
    {
        FScopeLock Lock(&StateMutex);
        return SpeedIndex - 1;
    }();
    SetSpeedIndex(NewSpeedIndex);
}

void FLibretroRunner::IncreaseSpeed()
{
    const int32 NewSpeedIndex = [this]()
    {
        FScopeLock Lock(&StateMutex);
        return SpeedIndex + 1;
    }();
    SetSpeedIndex(NewSpeedIndex);
}

double FLibretroRunner::GetSpeedMultiplier() const
{
    FScopeLock Lock(&StateMutex);
    return SpeedMultiplier;
}

void FLibretroRunner::Stop()
{
    bStopRequested = true;
}

uint32 FLibretroRunner::Run()
{
    GActiveLibretroRunner = this;

    // libretro 标准生命周期：
    // 1. 加载 DLL 并绑定 retro_* 符号
    // 2. 注册 environment/video/audio/input 回调
    // 3. retro_init()
    // 4. retro_load_game()
    // 5. 循环 retro_run()
    // 6. retro_unload_game()、retro_deinit()、卸载 DLL
    if (!LoadCore())
    {
        CleanupCoreOnRunnerThread();
        bRunning = false;
        return 1;
    }

    CoreApi.retro_set_environment(&FLibretroRunner::RetroEnvironment);
    CoreApi.retro_set_video_refresh(&FLibretroRunner::RetroVideoRefresh);
    CoreApi.retro_set_audio_sample(&FLibretroRunner::RetroAudioSample);
    CoreApi.retro_set_audio_sample_batch(&FLibretroRunner::RetroAudioSampleBatch);
    CoreApi.retro_set_input_poll(&FLibretroRunner::RetroInputPoll);
    CoreApi.retro_set_input_state(&FLibretroRunner::RetroInputState);

    CoreApi.retro_init();
    bCoreInitialized = true;

    retro_system_info SystemInfo = {};
    CoreApi.retro_get_system_info(&SystemInfo);
    const FString CoreName = UTF8_TO_TCHAR(SystemInfo.library_name ? SystemInfo.library_name : "");
    const FString CoreVersion = UTF8_TO_TCHAR(SystemInfo.library_version ? SystemInfo.library_version : "");
    UE_LOG(LogLibretroRunner, Log, TEXT("Loaded core: %s %s"),
        *CoreName,
        *CoreVersion);

    if (!LoadGame(RomPath))
    {
        CleanupCoreOnRunnerThread();
        bRunning = false;
        return 1;
    }

    CoreApi.retro_get_system_av_info(&AvInfo);
    TargetFps = AvInfo.timing.fps > 1.0 ? AvInfo.timing.fps : 60.0;
    TargetSampleRate = AvInfo.timing.sample_rate > 1.0 ? AvInfo.timing.sample_rate : 48000.0;
    const double InitialSpeed = GetSpeedMultiplier();
    UE_LOG(LogLibretroRunner, Log, TEXT("Loaded ROM: %s"), *RomPath);
    UE_LOG(LogLibretroRunner, Log, TEXT("AV info: base=%ux%u max=%ux%u aspect=%.4f fps=%.6f sample_rate=%.1f"),
        AvInfo.geometry.base_width,
        AvInfo.geometry.base_height,
        AvInfo.geometry.max_width,
        AvInfo.geometry.max_height,
        AvInfo.geometry.aspect_ratio,
        TargetFps,
        TargetSampleRate);

    const unsigned DisplayWidth = AvInfo.geometry.base_width > 0 ? AvInfo.geometry.base_width : AvInfo.geometry.max_width;
    const unsigned DisplayHeight = AvInfo.geometry.base_height > 0 ? AvInfo.geometry.base_height : AvInfo.geometry.max_height;
    AsyncTask(ENamedThreads::GameThread, [this, DisplayWidth, DisplayHeight]()
    {
        InitializeVideoTexture(DisplayWidth > 0 ? DisplayWidth : 256, DisplayHeight > 0 ? DisplayHeight : 240);
        InitializeAudioStream();
    });

    bRunning = true;
    SetStatus(FString::Printf(TEXT("运行中：%s  速度 %.1fx  %.3f FPS  %.0f Hz"), *FPaths::GetCleanFilename(RomPath), InitialSpeed, TargetFps, TargetSampleRate * InitialSpeed));

    const double BaseFrameTime = 1.0 / TargetFps;
    double NextFrameTime = FPlatformTime::Seconds();
    double LastFrameTime = NextFrameTime;
    double StatsStartTime = NextFrameTime;
    uint32 FramesSinceStats = 0;
    double RunMsAccumulator = 0.0;

    while (!bStopRequested)
    {
        if (bResetRequested)
        {
            if (CoreApi.retro_reset)
            {
                CoreApi.retro_reset();
            }
            bResetRequested = false;
        }

        ProcessRuntimeRequests();

        if (FrameTimeCallback.callback)
        {
            // 一些 core 需要前端每帧告知真实经过时间，以便内部动态调速。
            const double BeforeFrameCallbackTime = FPlatformTime::Seconds();
            const retro_usec_t DeltaUsec = static_cast<retro_usec_t>(FMath::Clamp((BeforeFrameCallbackTime - LastFrameTime) * 1000000.0, 0.0, 1000000.0));
            FrameTimeCallback.callback(DeltaUsec > 0 ? DeltaUsec : FrameTimeCallback.reference);
            LastFrameTime = BeforeFrameCallbackTime;
        }

        if (OpenGLRenderContext)
        {
            if (!OpenGLRenderContext->MakeCurrent())
            {
                UE_LOG(LogLibretroRunner, Warning, TEXT("无法激活 OpenGL context，当前帧可能无法正确渲染"));
            }
        }

        const double RunStartTime = FPlatformTime::Seconds();
        CoreApi.retro_run();
        const double RunEndTime = FPlatformTime::Seconds();

        RunMsAccumulator += (RunEndTime - RunStartTime) * 1000.0;
        ++FramesSinceStats;
        const double StatsElapsed = RunEndTime - StatsStartTime;
        const double CurrentSpeed = GetSpeedMultiplier();
        const double TargetRunFps = TargetFps * CurrentSpeed;
        if (StatsElapsed >= 1.0)
        {
            ActualRunFps = FramesSinceStats / StatsElapsed;
            AverageRetroRunMs = RunMsAccumulator / FramesSinceStats;
            SetStatus(FString::Printf(
                TEXT("运行中：%s  速度 %.1fx  实测 %.1f/%.1f FPS  retro_run %.2f ms"),
                *FPaths::GetCleanFilename(RomPath),
                CurrentSpeed,
                ActualRunFps,
                TargetRunFps,
                AverageRetroRunMs));
            UE_LOG(LogLibretroRunner, Log, TEXT("Runtime stats: actual_fps=%.2f target_fps=%.2f speed=%.2f retro_run_avg_ms=%.2f hw_render=%s"),
                ActualRunFps,
                TargetRunFps,
                CurrentSpeed,
                AverageRetroRunMs,
                OpenGLRenderContext && OpenGLRenderContext->IsValid() ? TEXT("yes") : TEXT("no"));
            StatsStartTime = RunEndTime;
            FramesSinceStats = 0;
            RunMsAccumulator = 0.0;
        }

        const double Now = FPlatformTime::Seconds();
        const double FrameTime = BaseFrameTime / FMath::Max(CurrentSpeed, 0.1);
        NextFrameTime += FrameTime;
        const double SleepTime = NextFrameTime - Now;
        // 主动按 core 报告的 FPS 节流，避免 retro_run() 空转占满 CPU。
        if (SleepTime > 0.001)
        {
            FPlatformProcess::Sleep(static_cast<float>(SleepTime));
        }
        else if (SleepTime < -FrameTime)
        {
            NextFrameTime = Now;
        }
    }

    CleanupCoreOnRunnerThread();
    bRunning = false;
    return 0;
}

bool FLibretroRunner::LoadCore()
{
    CoreHandle = FPlatformProcess::GetDllHandle(*CorePath);
    if (!CoreHandle)
    {
        SetError(FString::Printf(TEXT("无法加载 libretro core：%s"), *CorePath));
        return false;
    }

#define LOAD_RETRO_SYMBOL(Name) \
    if (!ExportSymbol(#Name, reinterpret_cast<void*&>(CoreApi.Name))) \
    { \
        return false; \
    }

    // 这里按 libretro.h 的必需导出逐个绑定；如果 core 缺少任何基础函数，
    // 后续生命周期调用就不可靠，因此直接启动失败。
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

    if (CoreApi.retro_api_version() != RETRO_API_VERSION)
    {
        SetError(FString::Printf(TEXT("libretro API 版本不匹配。core=%u 前端=%u"), CoreApi.retro_api_version(), RETRO_API_VERSION));
        return false;
    }

    return true;
}

void FLibretroRunner::UnloadCore()
{
    if (CoreHandle)
    {
        FPlatformProcess::FreeDllHandle(CoreHandle);
        CoreHandle = nullptr;
    }
    CoreApi = FLibretroCoreApi();
}

void FLibretroRunner::CleanupCoreOnRunnerThread()
{
    if (OpenGLRenderContext)
    {
        if (!OpenGLRenderContext->MakeCurrent())
        {
            UE_LOG(LogLibretroRunner, Verbose, TEXT("清理 core 时 OpenGL context 未处于激活状态"));
        }
    }

    SaveSRAM();

    if (bGameLoaded && CoreApi.retro_unload_game)
    {
        CoreApi.retro_unload_game();
        bGameLoaded = false;
    }

    if (bCoreInitialized && CoreApi.retro_deinit)
    {
        CoreApi.retro_deinit();
        bCoreInitialized = false;
    }

    OpenGLRenderContext.Reset();
    UnloadCore();
}

bool FLibretroRunner::LoadGame(const FString& InRomPath)
{
    FTCHARToUTF8 RomPathUtf8(*InRomPath);
    const retro_game_info Game =
    {
        RomPathUtf8.Get(),
        nullptr,
        0,
        nullptr
    };

    if (!CoreApi.retro_load_game(&Game))
    {
        SetError(FString::Printf(TEXT("core 无法加载 ROM：%s"), *InRomPath));
        return false;
    }

    bGameLoaded = true;
    if (CoreApi.retro_set_controller_port_device)
    {
        CoreApi.retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
    }

    return true;
}

void FLibretroRunner::SaveSRAM()
{
    if (!bGameLoaded || !CoreApi.retro_get_memory_data || !CoreApi.retro_get_memory_size)
    {
        return;
    }

    void* SaveData = CoreApi.retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
    const size_t SaveSize = CoreApi.retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
    if (!SaveData || SaveSize == 0)
    {
        return;
    }

    const FString SavePath = SaveDir / (FPaths::GetBaseFilename(RomPath) + TEXT(".srm"));
    if (!FFileHelper::SaveArrayToFile(TArrayView64<const uint8>(static_cast<const uint8*>(SaveData), static_cast<int64>(SaveSize)), *SavePath))
    {
        UE_LOG(LogLibretroRunner, Warning, TEXT("SRAM 保存失败：%s"), *SavePath);
    }
}

FString FLibretroRunner::GetQuickStatePath() const
{
    const FString StateFileName = FPaths::GetBaseFilename(RomPath) + TEXT(".state");
    return StateDir / StateFileName;
}

void FLibretroRunner::ProcessRuntimeRequests()
{
    if (bQuickSaveRequested)
    {
        bQuickSaveRequested = false;
        SaveQuickState();
    }

    if (bQuickLoadRequested)
    {
        bQuickLoadRequested = false;
        LoadQuickState();
    }
}

bool FLibretroRunner::SaveQuickState()
{
    if (!bGameLoaded || !CoreApi.retro_serialize_size || !CoreApi.retro_serialize)
    {
        SetError(TEXT("当前 core 不支持即时存档"));
        return false;
    }

    const size_t StateSize = CoreApi.retro_serialize_size();
    if (StateSize == 0)
    {
        SetError(TEXT("当前 core 不支持即时存档：状态大小为 0"));
        return false;
    }

    if (StateSize > static_cast<size_t>(MAX_int32))
    {
        SetError(FString::Printf(TEXT("即时存档失败：状态数据过大（%llu 字节）"), static_cast<unsigned long long>(StateSize)));
        return false;
    }

    TArray<uint8> StateData;
    StateData.SetNumUninitialized(static_cast<int32>(StateSize));
    if (!CoreApi.retro_serialize(StateData.GetData(), StateSize))
    {
        SetError(TEXT("即时存档失败：core 无法序列化当前状态"));
        return false;
    }

    const FString StatePath = GetQuickStatePath();
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(StatePath), true);
    if (!FFileHelper::SaveArrayToFile(TArrayView64<const uint8>(StateData.GetData(), StateData.Num()), *StatePath))
    {
        SetError(FString::Printf(TEXT("即时存档失败：无法写入 %s"), *StatePath));
        return false;
    }

    SetStatus(FString::Printf(TEXT("即时存档成功：%s"), *FPaths::GetCleanFilename(StatePath)));
    UE_LOG(LogLibretroRunner, Log, TEXT("Saved quick state: %s (%llu bytes)"), *StatePath, static_cast<unsigned long long>(StateSize));
    return true;
}

bool FLibretroRunner::LoadQuickState()
{
    if (!bGameLoaded || !CoreApi.retro_unserialize)
    {
        SetError(TEXT("当前 core 不支持即时读档"));
        return false;
    }

    const FString StatePath = GetQuickStatePath();
    IFileManager& FileManager = IFileManager::Get();
    const int64 FileSize = FileManager.FileSize(*StatePath);
    if (FileSize <= 0)
    {
        SetError(FString::Printf(TEXT("即时读档失败：状态文件不存在或为空：%s"), *StatePath));
        return false;
    }

    if (FileSize > MAX_int32)
    {
        SetError(FString::Printf(TEXT("即时读档失败：状态文件过大（%lld 字节）"), FileSize));
        return false;
    }

    TUniquePtr<FArchive> Reader(FileManager.CreateFileReader(*StatePath));
    if (!Reader)
    {
        SetError(FString::Printf(TEXT("即时读档失败：无法打开 %s"), *StatePath));
        return false;
    }

    TArray<uint8> StateData;
    StateData.SetNumUninitialized(static_cast<int32>(FileSize));
    Reader->Serialize(StateData.GetData(), StateData.Num());
    if (Reader->IsError())
    {
        SetError(FString::Printf(TEXT("即时读档失败：读取 %s 时发生错误"), *StatePath));
        return false;
    }

    if (!CoreApi.retro_unserialize(StateData.GetData(), StateData.Num()))
    {
        SetError(TEXT("即时读档失败：core 拒绝恢复该状态文件"));
        return false;
    }

    SetStatus(FString::Printf(TEXT("即时读档成功：%s"), *FPaths::GetCleanFilename(StatePath)));
    UE_LOG(LogLibretroRunner, Log, TEXT("Loaded quick state: %s (%lld bytes)"), *StatePath, FileSize);
    return true;
}

void FLibretroRunner::SetSpeedIndex(int32 NewSpeedIndex)
{
    double NewSpeedMultiplier = 1.0;
    {
        FScopeLock Lock(&StateMutex);
        const int32 ClampedSpeedIndex = FMath::Clamp(NewSpeedIndex, 0, UE_ARRAY_COUNT(LibretroSpeedLevels) - 1);
        SpeedIndex = ClampedSpeedIndex;
        NewSpeedMultiplier = LibretroSpeedLevels[SpeedIndex];
        SpeedMultiplier = NewSpeedMultiplier;
    }

    SetStatus(FString::Printf(TEXT("当前速度：%.1fx"), NewSpeedMultiplier));
    UE_LOG(LogLibretroRunner, Log, TEXT("Set libretro speed multiplier: %.2f"), NewSpeedMultiplier);
}

bool FLibretroRunner::ExportSymbol(const ANSICHAR* Name, void*& OutPtr)
{
    OutPtr = FPlatformProcess::GetDllExport(CoreHandle, ANSI_TO_TCHAR(Name));
    if (!OutPtr)
    {
        const FString SymbolName = ANSI_TO_TCHAR(Name);
        SetError(FString::Printf(TEXT("core 缺少导出函数：%s"), *SymbolName));
        return false;
    }
    return true;
}

void FLibretroRunner::InitializeVideoTexture(unsigned Width, unsigned Height)
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
}

void FLibretroRunner::InitializeAudioStream()
{
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

bool FLibretroRunner::ConsumeFrameForTextureUpdate()
{
    if (!VideoTexture)
    {
        return false;
    }

    struct FPendingFrameSnapshot
    {
        TArray<uint8> BGRA;
        unsigned Width = 0;
        unsigned Height = 0;
    };

    FPendingFrameSnapshot Snapshot;
    {
        // 视频回调发生在 Runner 线程，纹理上传发生在游戏线程。
        // 这里只复制最新一帧，允许高帧率情况下丢弃旧帧以保持 UI 低延迟。
        FScopeLock Lock(&FrameMutex);
        if (!bNewFrame || FrameBGRA.Num() == 0)
        {
            return false;
        }

        Snapshot.BGRA = FrameBGRA;
        Snapshot.Width = FrameWidth;
        Snapshot.Height = FrameHeight;
        bNewFrame = false;
    }

    if (Snapshot.Width == 0 || Snapshot.Height == 0)
    {
        return false;
    }

    if (Snapshot.Width != TextureWidth || Snapshot.Height != TextureHeight)
    {
        InitializeVideoTexture(Snapshot.Width, Snapshot.Height);
        if (!VideoTexture)
        {
            return false;
        }
    }

    FUpdateTextureRegion2D* HeapRegion = new FUpdateTextureRegion2D(0, 0, 0, 0, Snapshot.Width, Snapshot.Height);
    uint8* HeapData = static_cast<uint8*>(FMemory::Malloc(Snapshot.BGRA.Num()));
    FMemory::Memcpy(HeapData, Snapshot.BGRA.GetData(), Snapshot.BGRA.Num());

    VideoTexture->UpdateTextureRegions(
        0,
        1,
        HeapRegion,
        Snapshot.Width * 4,
        4,
        HeapData,
        [](uint8* Data, const FUpdateTextureRegion2D* Regions)
        {
            FMemory::Free(Data);
            delete Regions;
        });

    return true;
}

void FLibretroRunner::SubmitConvertedVideoFrame(TArray<uint8>&& Converted, unsigned Width, unsigned Height, const TCHAR* SourceName)
{
    if (Converted.Num() == 0 || Width == 0 || Height == 0)
    {
        return;
    }

    uint64 BrightnessSum = 0;
    uint32 NonBlackPixels = 0;
    const uint8* Pixels = Converted.GetData();
    const int32 PixelCount = static_cast<int32>(Width * Height);
    for (int32 Index = 0; Index < PixelCount; ++Index)
    {
        const uint8 B = Pixels[Index * 4 + 0];
        const uint8 G = Pixels[Index * 4 + 1];
        const uint8 R = Pixels[Index * 4 + 2];
        BrightnessSum += R + G + B;
        NonBlackPixels += (R | G | B) ? 1u : 0u;
    }

    // 记录前几帧的非黑像素和亮度总和，自动验证时可以判断是否真的读到了画面。
    ++VideoFrameCounter;
    if (VideoFrameCounter == 1 || VideoFrameCounter == 60 || VideoFrameCounter == 180 || VideoFrameCounter == 360)
    {
        UE_LOG(LogLibretroRunner, Log, TEXT("Frame %llu stats (%s): nonblack=%u brightness=%llu"),
            static_cast<unsigned long long>(VideoFrameCounter),
            SourceName ? SourceName : TEXT("video"),
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

void FLibretroRunner::QueueHardwareVideoFrame(unsigned Width, unsigned Height)
{
#if PLATFORM_WINDOWS
    if (!OpenGLRenderContext || !OpenGLRenderContext->IsValid())
    {
        return;
    }

    if (Width == 0 || Height == 0)
    {
        Width = AvInfo.geometry.base_width > 0 ? AvInfo.geometry.base_width : AvInfo.geometry.max_width;
        Height = AvInfo.geometry.base_height > 0 ? AvInfo.geometry.base_height : AvInfo.geometry.max_height;
    }

    TArray<uint8> Converted;
    if (OpenGLRenderContext->ReadFrame(Converted, Width, Height))
    {
        if (!bLoggedFirstFrame)
        {
            bLoggedFirstFrame = true;
            UE_LOG(LogLibretroRunner, Log, TEXT("First hardware video frame read: %ux%u"), Width, Height);
        }
        SubmitConvertedVideoFrame(MoveTemp(Converted), Width, Height, TEXT("opengl"));
    }
#else
    UE_UNUSED(Width);
    UE_UNUSED(Height);
#endif
}

void FLibretroRunner::QueueSoftwareVideoFrame(const void* Data, unsigned Width, unsigned Height, size_t Pitch)
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

    const uint8* SrcBytes = static_cast<const uint8*>(Data);
    for (unsigned Y = 0; Y < Height; ++Y)
    {
        const uint8* SrcRow = SrcBytes + Y * Pitch;
        uint8* DstRow = Converted.GetData() + Y * Width * 4;

        if (PixelFormat == RETRO_PIXEL_FORMAT_RGB565)
        {
            // RGB565 是许多 2D core 的常见输出格式，需要扩展到 8-bit BGRA。
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
            }
        }
        else if (PixelFormat == RETRO_PIXEL_FORMAT_0RGB1555)
        {
            // 0RGB1555 也是 libretro 支持的软件帧格式，最高位忽略。
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
            }
        }
        else
        {
            // 默认路径按 XRGB8888/ARGB8888 内存布局读取，再写成 UE 需要的 BGRA。
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
            }
        }
    }

    SubmitConvertedVideoFrame(MoveTemp(Converted), Width, Height, TEXT("software"));
}

void FLibretroRunner::QueueAudio(const int16* Data, size_t Frames)
{
    if (!Data || Frames == 0 || !SoundWave)
    {
        return;
    }

    SoundWave->QueueAudio(reinterpret_cast<const uint8*>(Data), static_cast<int32>(Frames * 2 * sizeof(int16)));
}

void FLibretroRunner::SetButtonState(ELibretroButton Button, bool bPressed)
{
    const uint8 Index = static_cast<uint8>(Button);
    if (Index >= static_cast<uint8>(ELibretroButton::Count))
    {
        return;
    }

    FScopeLock Lock(&StateMutex);
    ButtonStates[Index] = bPressed;
}

void FLibretroRunner::SetPointerState(float NormalizedX, float NormalizedY, bool bPressed)
{
    FScopeLock Lock(&StateMutex);
    PointerX = FMath::Clamp(NormalizedX, 0.0f, 1.0f);
    PointerY = FMath::Clamp(NormalizedY, 0.0f, 1.0f);
    bPointerPressed = bPressed;
}

int16 FLibretroRunner::QueryInput(unsigned Port, unsigned Device, unsigned Index, unsigned Id) const
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
        // 目前没有真正的模拟摇杆输入，先把 WASD/方向键映射为左摇杆满量程。
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
        // NDS/3DS 触摸屏通过 libretro pointer 设备表达，坐标范围是 -32767..32767。
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

bool FLibretroRunner::HandleEnvironment(unsigned Cmd, void* Data)
{
    // environment 是 libretro core 向前端查询能力、目录、core 选项、
    // 硬件渲染和日志接口的主通道。这里支持当前三个 core 运行所需的最小集合。
    switch (Cmd)
    {
    case RETRO_ENVIRONMENT_SET_HW_RENDER:
        return ConfigureHardwareRendering(static_cast<retro_hw_render_callback*>(Data));

    case RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER:
        if (Data)
        {
            *static_cast<retro_hw_context_type*>(Data) = RETRO_HW_CONTEXT_OPENGL_CORE;
        }
        return true;

    case RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK:
        if (Data)
        {
            FrameTimeCallback = *static_cast<retro_frame_time_callback*>(Data);
            return true;
        }
        FrameTimeCallback = {};
        return false;

    case RETRO_ENVIRONMENT_SET_HW_SHARED_CONTEXT:
        return true;

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
        return true;
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
        if (const retro_game_geometry* Geometry = static_cast<const retro_game_geometry*>(Data))
        {
            AvInfo.geometry = *Geometry;
        }
        return true;
    }

    case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
    {
        if (const retro_system_av_info* Info = static_cast<const retro_system_av_info*>(Data))
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
        if (retro_log_callback* Log = static_cast<retro_log_callback*>(Data))
        {
            Log->log = &FLibretroRunner::RetroLog;
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

bool FLibretroRunner::ConfigureHardwareRendering(retro_hw_render_callback* Callback)
{
    if (!Callback)
    {
        return false;
    }

    if (LaunchConfig.SystemType != ELibretroSystemType::ThreeDS)
    {
        UE_LOG(LogLibretroRunner, Warning, TEXT("Core requested hardware rendering for non-3DS content; accepting OpenGL path anyway"));
    }

    if (Callback->context_type != RETRO_HW_CONTEXT_OPENGL && Callback->context_type != RETRO_HW_CONTEXT_OPENGL_CORE)
    {
        UE_LOG(LogLibretroRunner, Warning, TEXT("libretro core requested unsupported HW context: %d"), static_cast<int32>(Callback->context_type));
        return false;
    }

    Callback->get_current_framebuffer = &FLibretroRunner::RetroGetCurrentFramebuffer;
    Callback->get_proc_address = &FLibretroRunner::RetroGetProcAddress;
    Callback->cache_context = true;

    // Azahar 会通过 get_current_framebuffer 获取 FBO，再通过 get_proc_address
    // 获取 OpenGL 函数。初始尺寸优先使用 core 已报告的最大尺寸。
    const unsigned InitialWidth = AvInfo.geometry.max_width > 0 ? AvInfo.geometry.max_width : 400;
    const unsigned InitialHeight = AvInfo.geometry.max_height > 0 ? AvInfo.geometry.max_height : 480;

#if PLATFORM_WINDOWS
    OpenGLRenderContext = MakeUnique<FLibretroOpenGLRenderContext>();
    if (!OpenGLRenderContext->Initialize(*Callback, InitialWidth, InitialHeight))
    {
        OpenGLRenderContext.Reset();
        return false;
    }
    return true;
#else
    UE_LOG(LogLibretroRunner, Error, TEXT("当前前端只在 Windows 上实现了 libretro 硬件渲染 core"));
    return false;
#endif
}

const char* FLibretroRunner::FindCoreOptionValue(const char* Key) const
{
    const FString KeyString = UTF8_TO_TCHAR(Key);
    if (const TArray<ANSICHAR>* Value = CoreOptionValueUtf8.Find(KeyString))
    {
        return Value->GetData();
    }
    return nullptr;
}

void FLibretroRunner::SetError(const FString& Error)
{
    UE_LOG(LogLibretroRunner, Error, TEXT("%s"), *Error);
    FScopeLock Lock(&StateMutex);
    LastError = Error;
    StatusText = Error;
}

void FLibretroRunner::SetStatus(const FString& Status)
{
    FScopeLock Lock(&StateMutex);
    StatusText = Status;
}

FString FLibretroRunner::GetLastError() const
{
    FScopeLock Lock(&StateMutex);
    return LastError;
}

FString FLibretroRunner::GetStatusText() const
{
    FScopeLock Lock(&StateMutex);
    return StatusText;
}

FString FLibretroRunner::GetLoadedRomPath() const
{
    FScopeLock Lock(&StateMutex);
    return RomPath;
}

FString FLibretroRunner::GetLoadedCorePath() const
{
    FScopeLock Lock(&StateMutex);
    return CorePath;
}

bool FLibretroRunner::RetroEnvironment(unsigned Cmd, void* Data)
{
    return GActiveLibretroRunner ? GActiveLibretroRunner->HandleEnvironment(Cmd, Data) : false;
}

void FLibretroRunner::RetroVideoRefresh(const void* Data, unsigned Width, unsigned Height, size_t Pitch)
{
    if (!GActiveLibretroRunner)
    {
        return;
    }

    if (Data == RETRO_HW_FRAME_BUFFER_VALID)
    {
        GActiveLibretroRunner->QueueHardwareVideoFrame(Width, Height);
    }
    else if (Data)
    {
        GActiveLibretroRunner->QueueSoftwareVideoFrame(Data, Width, Height, Pitch);
    }
}

void FLibretroRunner::RetroAudioSample(int16 Left, int16 Right)
{
    int16 Samples[2] = { Left, Right };
    if (GActiveLibretroRunner)
    {
        GActiveLibretroRunner->QueueAudio(Samples, 1);
    }
}

size_t FLibretroRunner::RetroAudioSampleBatch(const int16* Data, size_t Frames)
{
    if (GActiveLibretroRunner)
    {
        GActiveLibretroRunner->QueueAudio(Data, Frames);
    }
    return Frames;
}

void FLibretroRunner::RetroInputPoll()
{
}

int16 FLibretroRunner::RetroInputState(unsigned Port, unsigned Device, unsigned Index, unsigned Id)
{
    return GActiveLibretroRunner ? GActiveLibretroRunner->QueryInput(Port, Device, Index, Id) : 0;
}

uintptr_t FLibretroRunner::RetroGetCurrentFramebuffer()
{
#if PLATFORM_WINDOWS
    return GActiveLibretroRunner && GActiveLibretroRunner->OpenGLRenderContext
        ? GActiveLibretroRunner->OpenGLRenderContext->GetCurrentFramebuffer()
        : 0;
#else
    return 0;
#endif
}

retro_proc_address_t FLibretroRunner::RetroGetProcAddress(const char* Sym)
{
#if PLATFORM_WINDOWS
    return GActiveLibretroRunner && GActiveLibretroRunner->OpenGLRenderContext
        ? GActiveLibretroRunner->OpenGLRenderContext->GetProcAddress(Sym)
        : nullptr;
#else
    UE_UNUSED(Sym);
    return nullptr;
#endif
}

void FLibretroRunner::RetroLog(enum retro_log_level Level, const char* Fmt, ...)
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
