#include "LibretroPawn.h"

#include "Blueprint/UserWidget.h"
#include "Components/AudioComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "LibretroWidget.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Sound/SoundWaveProcedural.h"
#include "UnrealClient.h"

ALibretroPawn::ALibretroPawn()
{
    PrimaryActorTick.bCanEverTick = true;
    AutoPossessPlayer = EAutoReceiveInput::Player0;
}

void ALibretroPawn::BeginPlay()
{
    Super::BeginPlay();

    Runner = MakeUnique<FLibretroNESRunner>();

    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        PC->bShowMouseCursor = true;
        PC->SetInputMode(FInputModeGameAndUI());
    }

    Widget = CreateWidget<ULibretroWidget>(GetWorld(), ULibretroWidget::StaticClass());
    if (Widget)
    {
        Widget->SetOwningPawn(this);
        Widget->AddToViewport();
    }

    bAutoScreenshot = FParse::Param(FCommandLine::Get(), TEXT("AutoScreenshot"));

    const EAutoRomTarget AutoTarget = ParseAutoRomTarget();
    if (FParse::Param(FCommandLine::Get(), TEXT("AutoStartROM")) || bAutoScreenshot || AutoTarget != EAutoRomTarget::None)
    {
        switch (AutoTarget)
        {
        case EAutoRomTarget::MM2R:
            StartMetalMax2R();
            break;
        case EAutoRomTarget::MM3:
            StartMetalMax3();
            break;
        case EAutoRomTarget::NES:
        case EAutoRomTarget::None:
        default:
            StartNesRom();
            break;
        }
    }
}

void ALibretroPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (Runner)
    {
        const bool bUpdatedTexture = Runner->ConsumeFrameForTextureUpdate();

        if (!AudioComponent && Runner->GetSoundWave())
        {
            AudioComponent = NewObject<UAudioComponent>(this);
            AudioComponent->bAutoActivate = false;
            AudioComponent->SetSound(Runner->GetSoundWave());
            AudioComponent->RegisterComponent();
            AudioComponent->Play();
        }

        if (bAutoScreenshot && bUpdatedTexture && !bScreenshotRequested)
        {
            bScreenshotRequested = true;
            ScreenshotDelay = 8.0f;
        }
    }

    if (bScreenshotRequested && ScreenshotDelay > 0.0f)
    {
        ScreenshotDelay -= DeltaSeconds;
        if (ScreenshotDelay <= 0.0f)
        {
            const FString ScreenshotPath = FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/%s_Auto.png"), *GetScreenshotStem());
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(ScreenshotPath), true);
            FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
            ScreenshotDelay = -2.0f;
        }
    }
    else if (bAutoScreenshot && bScreenshotRequested && ScreenshotDelay < 0.0f)
    {
        ScreenshotDelay += DeltaSeconds;
        if (ScreenshotDelay >= 0.0f)
        {
            FPlatformMisc::RequestExit(false, TEXT("AutoScreenshot completed"));
        }
    }

    if (Widget)
    {
        Widget->RefreshFromRunner();
    }
}

void ALibretroPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (Runner)
    {
        Runner->StopAndUnload();
        Runner.Reset();
    }

    Super::EndPlay(EndPlayReason);
}

void ALibretroPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    PlayerInputComponent->BindKey(EKeys::W, IE_Pressed, this, &ALibretroPawn::PressUp);
    PlayerInputComponent->BindKey(EKeys::W, IE_Released, this, &ALibretroPawn::ReleaseUp);
    PlayerInputComponent->BindKey(EKeys::S, IE_Pressed, this, &ALibretroPawn::PressDown);
    PlayerInputComponent->BindKey(EKeys::S, IE_Released, this, &ALibretroPawn::ReleaseDown);
    PlayerInputComponent->BindKey(EKeys::A, IE_Pressed, this, &ALibretroPawn::PressLeft);
    PlayerInputComponent->BindKey(EKeys::A, IE_Released, this, &ALibretroPawn::ReleaseLeft);
    PlayerInputComponent->BindKey(EKeys::D, IE_Pressed, this, &ALibretroPawn::PressRight);
    PlayerInputComponent->BindKey(EKeys::D, IE_Released, this, &ALibretroPawn::ReleaseRight);

    PlayerInputComponent->BindKey(EKeys::J, IE_Pressed, this, &ALibretroPawn::PressConfirm);
    PlayerInputComponent->BindKey(EKeys::J, IE_Released, this, &ALibretroPawn::ReleaseConfirm);
    PlayerInputComponent->BindKey(EKeys::K, IE_Pressed, this, &ALibretroPawn::PressCancel);
    PlayerInputComponent->BindKey(EKeys::K, IE_Released, this, &ALibretroPawn::ReleaseCancel);
    PlayerInputComponent->BindKey(EKeys::U, IE_Pressed, this, &ALibretroPawn::PressX);
    PlayerInputComponent->BindKey(EKeys::U, IE_Released, this, &ALibretroPawn::ReleaseX);
    PlayerInputComponent->BindKey(EKeys::I, IE_Pressed, this, &ALibretroPawn::PressY);
    PlayerInputComponent->BindKey(EKeys::I, IE_Released, this, &ALibretroPawn::ReleaseY);
    PlayerInputComponent->BindKey(EKeys::Q, IE_Pressed, this, &ALibretroPawn::PressL);
    PlayerInputComponent->BindKey(EKeys::Q, IE_Released, this, &ALibretroPawn::ReleaseL);
    PlayerInputComponent->BindKey(EKeys::E, IE_Pressed, this, &ALibretroPawn::PressR);
    PlayerInputComponent->BindKey(EKeys::E, IE_Released, this, &ALibretroPawn::ReleaseR);
    PlayerInputComponent->BindKey(EKeys::BackSpace, IE_Pressed, this, &ALibretroPawn::PressSelect);
    PlayerInputComponent->BindKey(EKeys::BackSpace, IE_Released, this, &ALibretroPawn::ReleaseSelect);
    PlayerInputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &ALibretroPawn::PressStart);
    PlayerInputComponent->BindKey(EKeys::Enter, IE_Released, this, &ALibretroPawn::ReleaseStart);
    PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ALibretroPawn::PressTouch);
    PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Released, this, &ALibretroPawn::ReleaseTouch);
}

void ALibretroPawn::StartNesRom()
{
    StartRom(MakeNesConfig());
}

void ALibretroPawn::StartMetalMax2R()
{
    StartRom(MakeNdsConfig(TEXT("MetalMax2R_Ver0.94.nds"), TEXT("Metal Max 2 Reloaded")));
}

void ALibretroPawn::StartMetalMax3()
{
    StartRom(MakeNdsConfig(TEXT("Metal_Max_3_chs_v1.0-Union_of_MM3.nds"), TEXT("Metal Max 3")));
}

void ALibretroPawn::StartRom(const FLibretroLaunchConfig& Config)
{
    if (!Runner)
    {
        Runner = MakeUnique<FLibretroNESRunner>();
    }

    if (AudioComponent)
    {
        AudioComponent->Stop();
        AudioComponent->DestroyComponent();
        AudioComponent = nullptr;
    }

    ActiveDisplayName = Config.DisplayName;
    ActiveScreenshotStem = ActiveDisplayName.Replace(TEXT(" "), TEXT("_")).Replace(TEXT(":"), TEXT("_"));
    bScreenshotRequested = false;
    ScreenshotDelay = 0.0f;
    Runner->Start(Config);
}

FLibretroLaunchConfig ALibretroPawn::MakeNesConfig() const
{
    FLibretroLaunchConfig Config;
    Config.DisplayName = TEXT("重装机兵1");
    Config.SystemType = ELibretroSystemType::NES;
    Config.CorePath = FPaths::ProjectDir() / TEXT("ThirdParty/Libretro/Cores/Win64/fceumm_libretro.dll");
    Config.RomPath = FindRomByFileName(TEXT("E:/Z_Game/VirtuaNESex-FC模拟器/游戏"), TEXT("重装机兵1.nes"));
    return Config;
}

FLibretroLaunchConfig ALibretroPawn::MakeNdsConfig(const FString& RomFileName, const FString& DisplayName) const
{
    FLibretroLaunchConfig Config;
    Config.DisplayName = DisplayName;
    Config.SystemType = ELibretroSystemType::NDS;
    Config.CorePath = FPaths::ProjectDir() / TEXT("ThirdParty/Libretro/Cores/Win64/desmume_libretro.dll");
    Config.RomPath = FindRomByFileName(TEXT("E:/Z_Game/MM3&2R"), RomFileName);
    Config.CoreOptions.Add(TEXT("desmume_screens_layout"), TEXT("top/bottom"));
    Config.CoreOptions.Add(TEXT("desmume_internal_resolution"), TEXT("256x192"));
    Config.CoreOptions.Add(TEXT("desmume_opengl_mode"), TEXT("disabled"));
    Config.CoreOptions.Add(TEXT("desmume_color_depth"), TEXT("32-bit"));
    Config.CoreOptions.Add(TEXT("desmume_cpu_mode"), TEXT("jit"));
    Config.CoreOptions.Add(TEXT("desmume_jit_block_size"), TEXT("12"));
    Config.CoreOptions.Add(TEXT("desmume_load_to_memory"), TEXT("enabled"));
    Config.CoreOptions.Add(TEXT("desmume_pointer_type"), TEXT("touch"));
    Config.CoreOptions.Add(TEXT("desmume_pointer_mouse"), TEXT("enabled"));
    Config.CoreOptions.Add(TEXT("desmume_firmware_language"), TEXT("English"));
    return Config;
}

FString ALibretroPawn::FindRomByFileName(const FString& Directory, const FString& FileName) const
{
    TArray<FString> Matches;
    IFileManager::Get().FindFilesRecursive(Matches, *Directory, *FileName, true, false, false);
    return Matches.Num() > 0 ? Matches[0] : Directory / FileName;
}

ALibretroPawn::EAutoRomTarget ALibretroPawn::ParseAutoRomTarget() const
{
    FString AutoRom;
    if (!FParse::Value(FCommandLine::Get(), TEXT("AutoRom="), AutoRom))
    {
        return EAutoRomTarget::None;
    }

    if (AutoRom.Equals(TEXT("MM2R"), ESearchCase::IgnoreCase) || AutoRom.Equals(TEXT("MetalMax2R"), ESearchCase::IgnoreCase))
    {
        return EAutoRomTarget::MM2R;
    }

    if (AutoRom.Equals(TEXT("MM3"), ESearchCase::IgnoreCase) || AutoRom.Equals(TEXT("MetalMax3"), ESearchCase::IgnoreCase))
    {
        return EAutoRomTarget::MM3;
    }

    if (AutoRom.Equals(TEXT("NES"), ESearchCase::IgnoreCase) || AutoRom.Equals(TEXT("FC"), ESearchCase::IgnoreCase))
    {
        return EAutoRomTarget::NES;
    }

    return EAutoRomTarget::None;
}

FString ALibretroPawn::GetScreenshotStem() const
{
    return ActiveScreenshotStem.IsEmpty() ? TEXT("Libretro") : ActiveScreenshotStem;
}

void ALibretroPawn::SetButton(ELibretroButton Button, bool bPressed)
{
    if (Runner)
    {
        Runner->SetButtonState(Button, bPressed);
    }
}

void ALibretroPawn::PressUp() { SetButton(ELibretroButton::Up, true); }
void ALibretroPawn::ReleaseUp() { SetButton(ELibretroButton::Up, false); }
void ALibretroPawn::PressDown() { SetButton(ELibretroButton::Down, true); }
void ALibretroPawn::ReleaseDown() { SetButton(ELibretroButton::Down, false); }
void ALibretroPawn::PressLeft() { SetButton(ELibretroButton::Left, true); }
void ALibretroPawn::ReleaseLeft() { SetButton(ELibretroButton::Left, false); }
void ALibretroPawn::PressRight() { SetButton(ELibretroButton::Right, true); }
void ALibretroPawn::ReleaseRight() { SetButton(ELibretroButton::Right, false); }
void ALibretroPawn::PressConfirm() { SetButton(ELibretroButton::A, true); }
void ALibretroPawn::ReleaseConfirm() { SetButton(ELibretroButton::A, false); }
void ALibretroPawn::PressCancel() { SetButton(ELibretroButton::B, true); }
void ALibretroPawn::ReleaseCancel() { SetButton(ELibretroButton::B, false); }
void ALibretroPawn::PressX() { SetButton(ELibretroButton::X, true); }
void ALibretroPawn::ReleaseX() { SetButton(ELibretroButton::X, false); }
void ALibretroPawn::PressY() { SetButton(ELibretroButton::Y, true); }
void ALibretroPawn::ReleaseY() { SetButton(ELibretroButton::Y, false); }
void ALibretroPawn::PressL() { SetButton(ELibretroButton::L, true); }
void ALibretroPawn::ReleaseL() { SetButton(ELibretroButton::L, false); }
void ALibretroPawn::PressR() { SetButton(ELibretroButton::R, true); }
void ALibretroPawn::ReleaseR() { SetButton(ELibretroButton::R, false); }
void ALibretroPawn::PressSelect() { SetButton(ELibretroButton::Select, true); }
void ALibretroPawn::ReleaseSelect() { SetButton(ELibretroButton::Select, false); }
void ALibretroPawn::PressStart() { SetButton(ELibretroButton::Start, true); }
void ALibretroPawn::ReleaseStart() { SetButton(ELibretroButton::Start, false); }
void ALibretroPawn::PressTouch()
{
    if (Runner)
    {
        Runner->SetPointerState(0.5f, 0.75f, true);
    }
}
void ALibretroPawn::ReleaseTouch()
{
    if (Runner)
    {
        Runner->SetPointerState(0.5f, 0.75f, false);
    }
}
