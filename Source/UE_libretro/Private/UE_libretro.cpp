#include "UE_libretro.h"
#include "Modules/ModuleManager.h"

// 注册 UE 主游戏模块，模块名必须与 .uproject 中的 Runtime 模块名称一致。
IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, UE_libretro, "UE_libretro");
