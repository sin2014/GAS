#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LibretroGameMode.generated.h"

/**
 * 示例工程的默认 GameMode。
 *
 * 当前只负责把默认 Pawn 指定为 ALibretroPawn，让项目启动后直接进入
 * libretro 运行界面。
 */
UCLASS()
class UE_LIBRETRO_API ALibretroGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    /** 设置 DefaultPawnClass。 */
    ALibretroGameMode();
};
