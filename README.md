# UE_libretro 多平台 ROM 运行示例

这个 UE5.8 C++ 工程演示在 UE 游戏中作为 libretro 前端运行 FC/NES、NDS 与 3DS ROM。

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

## 已实现功能

- 在 UE5.8 中实现了一个最小 libretro 前端，可以动态加载 Win64 libretro core DLL。
- 已接入 FC/NES、NDS、3DS 三类 ROM 启动入口。
- 已接入 FCEUmm、DeSmuME、Azahar 三个 core。
- 已实现 libretro core 生命周期：加载 DLL、绑定 `retro_*` 函数、注册回调、`retro_init`、`retro_load_game`、循环 `retro_run`、`retro_unload_game`、`retro_deinit`、卸载 DLL。
- 已实现 libretro environment 回调，支持系统目录、存档目录、内容目录、core 路径、core 选项、像素格式、输入能力、日志接口、AV 信息更新、OpenGL 硬件渲染请求等。
- 已实现软件视频帧转换：支持 `XRGB8888`、`RGB565`、`0RGB1555` 转成 UE 可上传的 BGRA 数据。
- 已实现硬件视频帧读回：Azahar/3DS 通过 `RETRO_ENVIRONMENT_SET_HW_RENDER` 获取 Windows OpenGL context 和帧缓冲，再读回到 UE `UTexture2D`。
- 已实现 UMG 视频显示：把 libretro 输出帧上传到临时 `UTexture2D`，再绑定到 C++ 构建的 UMG `Image`。
- 已实现程序化音频播放：libretro stereo int16 音频样本写入 `USoundWaveProcedural`，由 `UAudioComponent` 播放。
- 已实现基础输入：键盘映射到 RetroPad、左模拟摇杆方向和 pointer/touch 设备。
- 已实现基础触摸入口：`Space` 固定按下合成下屏中心点。
- 已实现 core 选项注入：不同 core 使用各自选项 key 配置画面布局、JIT、硬件渲染、触摸等行为。
- 已实现 SRAM 保存：core 暴露 `RETRO_MEMORY_SAVE_RAM` 时会写入 `Saved/Libretro/Saves/*.srm`。
- 已实现状态与错误显示：Runner 状态会显示到 UMG 底部文本。
- 已实现自动验证参数：`-AutoRom=...`、`-AutoScreenshot` 可以自动启动 ROM、等待出帧、截图并退出。
- 已实现运行诊断日志：记录 core 名称、AV 信息、实际 FPS、`retro_run` 平均耗时、硬件渲染状态和关键帧非黑像素统计。

## 后续可扩展功能

libretro 前端的能力不只限于当前三个 core。只要 core 能在 Win64 下运行，并且所需 environment 回调已经实现或可以补齐，UE 中还可以继续扩展这些方向：

- 更多主机/掌机 core：例如 SNES、Game Boy / Game Boy Color、Game Boy Advance、Genesis / Mega Drive、PC Engine、Neo Geo Pocket、WonderSwan、PlayStation、PSP 等。
- 更多街机 core：例如 FinalBurn Neo、MAME 系列 core，用于街机 ROM 或合集前端。
- 更多 Nintendo 相关平台：例如 SNES、N64、GameCube/Wii 等，但 3D 平台通常需要更完整的硬件渲染、输入和性能调优。
- 更多 Sega / Sony / NEC 等平台 core：按 core 要求补齐 BIOS、系统目录、core 选项、输入映射和硬件渲染支持。
- 即时存档/读档：当前已绑定 `retro_serialize`、`retro_unserialize`、`retro_serialize_size` 函数指针，但还没有 UI 和文件格式封装。
- 作弊码：当前已绑定 `retro_cheat_reset`、`retro_cheat_set`，后续可以做作弊码列表、开关和导入。
- 更完整的触摸和鼠标输入：把 UMG 鼠标坐标换算到 NDS/3DS 下屏，支持点击、拖拽、多点触摸或光标显示。
- 手柄输入：接入 UE Enhanced Input 或 XInput，把真实手柄映射到 RetroPad、模拟摇杆、L2/R2、触摸快捷键。
- 多玩家输入：当前只支持 port 0，后续可以扩展到多个 libretro input port。
- Core 选项界面：把 core 暴露的选项做成设置界面，支持运行前配置或部分运行时刷新。
- BIOS/system 文件管理：为需要 BIOS 的 core 提供 UI 提示、目录检查和文件完整性提示。
- 存档管理：展示 SRAM、即时存档、截图、配置文件，支持导入、备份、恢复。
- 渲染优化：把 OpenGL 帧缓冲读回改为更低延迟的共享纹理路径，或按平台扩展 D3D/Vulkan 硬件渲染桥接。
- 画面处理：整数缩放、保持宽高比、滤镜、扫描线、shader、双屏布局切换、上下屏单独缩放。
- 音频增强：音量控制、静音、缓冲大小配置、欠载统计、音频延迟调优。
- 性能工具：在 UI 中显示 FPS、`retro_run` 耗时、音频缓冲、帧队列状态和硬件渲染状态。
- 录像/截图：扩展手动截图、连续截图、录制视频或导出运行日志。
- 前端内容库：扫描 ROM 目录、生成游戏列表、按平台分组、记录最近运行和封面图。

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

触摸目前先做成基础入口：`Space` 会在合成双屏画面的下屏中心按下触摸点。后续如果要做鼠标点击/拖拽下屏，可以在 `FLibretroRunner::SetPointerState` 和 UI 鼠标事件上继续扩展。

## 关键源码

```text
Source\UE_libretro\Public\LibretroRunner.h
Source\UE_libretro\Private\LibretroRunner.cpp
Source\UE_libretro\Public\LibretroPawn.h
Source\UE_libretro\Private\LibretroPawn.cpp
Source\UE_libretro\Public\LibretroWidget.h
Source\UE_libretro\Private\LibretroWidget.cpp
```

说明：

- `FLibretroRunner` 是实际 libretro 前端，负责动态加载 core、绑定 `retro_*` 符号、提供 environment/video/audio/input 回调。
- 3DS/Azahar 通过 `RETRO_ENVIRONMENT_SET_HW_RENDER` 接入 Windows OpenGL 3.3+ 硬件渲染，再把硬件 framebuffer 读回到 UE `UTexture2D` 显示。
- `FLibretroLaunchConfig` 描述一次启动需要的 core、ROM、系统类型和 core 选项。
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
