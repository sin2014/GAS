#include "LibretroGameMode.h"
#include "LibretroPawn.h"

ALibretroGameMode::ALibretroGameMode()
{
    DefaultPawnClass = ALibretroPawn::StaticClass();
}
