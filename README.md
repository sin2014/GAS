# UE_libretro 多平台 ROM 运行示例

这个 UE5.8 C++ 工程演示在 UE 游戏中作为 libretro frontend 运行 FC/NES、NDS 与 3DS ROM。

## 已接入的 ROM

```text
E:\Z_Game\VirtuaNESex-FC模拟器\游戏\重装机兵1.nes
E:\Z_Game\MM3&2R\MetalMax2R_Ver0.94.nds
E:\Z_Game\MM3&2R\Metal_Max_3_chs_v1.0-Union_of_MM3.nds
E:\Z_Game\MM4\重装机兵4月光歌姬515.cci
```

## 已接入的 libretro core

```text
ThirdParty\Libretro\Cores\Win64\fceumm_libretro.dll
ThirdParty\Libretro\Cores\Win64\desmume_libretro.dll
ThirdParty\Libretro\Cores\Win64\azahar_libretro.dll
ThirdParty\Libretro\Include\libretro.h
```

- FC/NES 使用 FCEUmm。
- NDS 使用 DeSmuME libretro core。
- 3DS 当前按钮使用 Azahar libretro core，配置为 OpenGL 硬件渲染、硬件 shader、CPU JIT、原生 400x240 上屏 + 320x240 下屏的 Top-Bottom 布局。

## UE 游戏入口

运行游戏后，界面底部有四个按钮：

- `重装机兵1 FC`
- `Metal Max 2R NDS`
- `Metal Max 3 NDS`
- `重装机兵4 3DS`

点击对应按钮会停止当前 core，加载对应 core 和 ROM，然后把 libretro 视频帧显示到 UMG `Image`，音频送入 `USoundWaveProcedural` 播放。

## 键盘映射

### 通用方向

| PC 按键 | libretro / 3DS |
| --- | --- |
| `W` | 上 / Circle Pad 上 |
| `S` | 下 / Circle Pad 下 |
| `A` | 左 / Circle Pad 左 |
| `D` | 右 / Circle Pad 右 |

### FC/NES

| PC 按键 | FC |
| --- | --- |
| `J` | A / 确认 |
| `K` | B / 取消 |
| `Enter` | Start |

### NDS / 3DS 临时映射

| PC 按键 | NDS / 3DS |
| --- | --- |
| `J` | A / 常用确认 |
| `K` | B / 常用取消 |
| `U` | X |
| `I` | Y |
| `Q` | L |
| `E` | R |
| `Z` | ZL / L2 |
| `C` | ZR / R2 |
| `Enter` | Start |
| `Backspace` | Select |
| `Space` | 触摸下屏中心点 |

触摸目前先做成基础入口：`Space` 会在合成双屏画面的下屏中心按下触摸点。后续如果要做鼠标点击/拖拽下屏，可以在 `FLibretroNESRunner::SetPointerState` 和 UI 鼠标事件上继续扩展。

## 关键源码

```text
Source\UE_libretro\Public\LibretroNESRunner.h
Source\UE_libretro\Private\LibretroNESRunner.cpp
Source\UE_libretro\Public\LibretroPawn.h
Source\UE_libretro\Private\LibretroPawn.cpp
Source\UE_libretro\Public\LibretroWidget.h
Source\UE_libretro\Private\LibretroWidget.cpp
```

说明：

- `FLibretroNESRunner` 是实际 libretro frontend，负责动态加载 core、绑定 `retro_*` 符号、提供 environment/video/audio/input 回调。
- 3DS/Azahar 通过 `RETRO_ENVIRONMENT_SET_HW_RENDER` 接入 Windows OpenGL 3.3+ 硬件渲染，再把硬件 framebuffer 读回到 UE `UTexture2D` 显示。
- `FLibretroLaunchConfig` 描述一次启动需要的 core、ROM、系统类型和 core options。
- `ALibretroPawn` 负责创建 runner、UMG、音频组件、按钮入口和键盘映射。
- `ULibretroWidget` 是纯 C++ UMG，提供四个 ROM 启动按钮和视频画面显示。

## 打开工程

工程位置：

```text
D:\GameDev\插件\UE_libretro\UE_libretro.uproject
```

UE5.8 安装位置：

```text
D:\GameEngine\Epic Games\UE_5.8
```

由于本机 UE/编译器链路对中文路径的 Intermediate 目录处理会出现乱码，已创建一个目录联接用于编译和命令行运行：

```text
D:\GameDev\UE_libretro_build -> D:\GameDev\插件\UE_libretro
```

建议命令行使用联接路径：

```powershell
& "D:\GameEngine\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "D:\GameDev\UE_libretro_build\UE_libretro.uproject" -game
```

也可以直接双击原始路径下的 `.uproject` 打开编辑器。

## 编译命令

```powershell
& "D:\GameEngine\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" UE_libretroEditor Win64 Development -Project="D:\GameDev\UE_libretro_build\UE_libretro.uproject" -WaitMutex -NoHotReloadFromIDE
```

## 自动验证命令

自动启动 MM2R：

```powershell
& "D:\GameEngine\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "D:\GameDev\UE_libretro_build\UE_libretro.uproject" -game -AutoScreenshot -AutoRom=MM2R -log
```

自动启动 MM3：

```powershell
& "D:\GameEngine\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "D:\GameDev\UE_libretro_build\UE_libretro.uproject" -game -AutoScreenshot -AutoRom=MM3 -log
```

自动启动 MM4：

```powershell
& "D:\GameEngine\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "D:\GameDev\UE_libretro_build\UE_libretro.uproject" -game -AutoScreenshot -AutoRom=MM4 -log
```

截图输出：

```text
Saved\Screenshots\Metal_Max_2_Reloaded_Auto.png
Saved\Screenshots\Metal_Max_3_Auto.png
Saved\Screenshots\重装机兵4_月光歌姬_Auto.png
```

## 当前验证结果

- `UE_libretroEditor Win64 Development` 编译通过。
- `desmume_libretro.dll` 成功加载，日志显示 `Loaded core: DeSmuME git ae0f7f5`。
- `MetalMax2R_Ver0.94.nds` 成功加载并出帧。
- `Metal_Max_3_chs_v1.0-Union_of_MM3.nds` 成功加载并出帧。
- `azahar_libretro.dll` 成功加载，日志显示 `Loaded core: Azahar 9701a3d`。
- `重装机兵4月光歌姬515.cci` 成功加载，日志确认：
  - `Initialized libretro OpenGL HW context 400x480`
  - `Loaded ROM: E:/Z_Game/MM4/重装机兵4月光歌姬515.cci`
  - `AV info: base=400x480 max=7200x4800 aspect=0.8333 fps=60.000000 sample_rate=32728.0`
  - `First hardware video frame read: 400x480`
  - `Runtime stats: actual_fps=60.00 target_fps=60.00 retro_run_avg_ms=2.31 hw_render=yes`
  - `Frame 360 stats (opengl)` 非黑像素正常。
- 自动截图已确认能看到 UE 中的 3DS 双屏画面，且自动验证退出无崩溃。
