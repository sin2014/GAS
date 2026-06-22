#include "LibretroWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "LibretroNESRunner.h"
#include "LibretroPawn.h"
#include "Styling/CoreStyle.h"

DEFINE_LOG_CATEGORY_STATIC(LogLibretroWidget, Log, All);

namespace
{
    constexpr float VideoBoxWidth = 768.0f;
    constexpr float VideoBoxHeight = 720.0f;

    FText DefaultStatusText()
    {
        return FText::FromString(TEXT("选择一个 ROM 启动。WASD 方向，J=A/确认，K=B/取消，U=X，I=Y，Q=L，E=R，Enter=Start，Backspace=Select，Space=触摸下屏中心。"));
    }
}

void ULibretroWidget::SetOwningPawn(ALibretroPawn* InPawn)
{
    OwningPawn = InPawn;
}

TSharedRef<SWidget> ULibretroWidget::RebuildWidget()
{
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    UOverlay* Panel = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Panel"));
    UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
    PanelSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
    PanelSlot->SetOffsets(FMargin(0.0f));

    UImage* Background = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Background"));
    Background->SetColorAndOpacity(FLinearColor(0.015f, 0.018f, 0.025f, 1.0f));
    Panel->AddChildToOverlay(Background);

    USizeBox* VideoSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("VideoSize"));
    VideoSize->SetWidthOverride(VideoBoxWidth);
    VideoSize->SetHeightOverride(VideoBoxHeight);
    UOverlaySlot* VideoSizeSlot = Panel->AddChildToOverlay(VideoSize);
    VideoSizeSlot->SetHorizontalAlignment(HAlign_Center);
    VideoSizeSlot->SetVerticalAlignment(VAlign_Center);
    VideoSizeSlot->SetPadding(FMargin(0.0f, 42.0f, 0.0f, 0.0f));

    UScaleBox* VideoScale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("VideoScale"));
    VideoScale->SetStretch(EStretch::ScaleToFit);
    VideoSize->AddChild(VideoScale);

    VideoImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("VideoImage"));
    VideoImage->SetColorAndOpacity(FLinearColor::Black);
    VideoScale->AddChild(VideoImage);

    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
    Title->SetText(FText::FromString(TEXT("UE5.8 libretro ROM 运行器")));
    Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.94f, 0.96f, 1.0f, 1.0f)));
    Title->SetJustification(ETextJustify::Center);
    Title->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 30));
    UOverlaySlot* TitleSlot = Panel->AddChildToOverlay(Title);
    TitleSlot->SetHorizontalAlignment(HAlign_Center);
    TitleSlot->SetVerticalAlignment(VAlign_Top);
    TitleSlot->SetPadding(FMargin(0.0f, 28.0f, 0.0f, 0.0f));

    StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
    StatusText->SetText(DefaultStatusText());
    StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.78f, 0.84f, 0.90f, 1.0f)));
    StatusText->SetJustification(ETextJustify::Center);
    StatusText->SetAutoWrapText(true);
    StatusText->SetWrapTextAt(1120.0f);
    StatusText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 17));
    UOverlaySlot* StatusSlot = Panel->AddChildToOverlay(StatusText);
    StatusSlot->SetHorizontalAlignment(HAlign_Center);
    StatusSlot->SetVerticalAlignment(VAlign_Bottom);
    StatusSlot->SetPadding(FMargin(36.0f, 0.0f, 36.0f, 30.0f));

    UHorizontalBox* ButtonRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ButtonRow"));
    UOverlaySlot* ButtonSlot = Panel->AddChildToOverlay(ButtonRow);
    ButtonSlot->SetHorizontalAlignment(HAlign_Center);
    ButtonSlot->SetVerticalAlignment(VAlign_Bottom);
    ButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 86.0f));

    StartNesButton = CreateLauncherButton(ButtonRow, TEXT("StartNesButton"), TEXT("重装机兵1 FC"));
    StartMM2RButton = CreateLauncherButton(ButtonRow, TEXT("StartMM2RButton"), TEXT("Metal Max 2R NDS"));
    StartMM3Button = CreateLauncherButton(ButtonRow, TEXT("StartMM3Button"), TEXT("Metal Max 3 NDS"));

    return Super::RebuildWidget();
}

UButton* ULibretroWidget::CreateLauncherButton(UPanelWidget* Parent, const FName& Name, const FString& Text)
{
    UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
    UHorizontalBoxSlot* ButtonSlot = Cast<UHorizontalBoxSlot>(Parent->AddChild(Button));
    if (ButtonSlot)
    {
        ButtonSlot->SetPadding(FMargin(8.0f, 0.0f));
        ButtonSlot->SetHorizontalAlignment(HAlign_Center);
        ButtonSlot->SetVerticalAlignment(VAlign_Center);
    }

    USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName(*(Name.ToString() + TEXT("_Size"))));
    SizeBox->SetWidthOverride(220.0f);
    SizeBox->SetHeightOverride(48.0f);

    UTextBlock* ButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(*(Name.ToString() + TEXT("_Text"))));
    ButtonText->SetText(FText::FromString(Text));
    ButtonText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 18));
    ButtonText->SetColorAndOpacity(FSlateColor(FLinearColor(0.05f, 0.07f, 0.10f, 1.0f)));
    ButtonText->SetJustification(ETextJustify::Center);
    ButtonText->SetAutoWrapText(false);

    SizeBox->AddChild(ButtonText);
    Button->AddChild(SizeBox);
    return Button;
}

void ULibretroWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (StartNesButton)
    {
        StartNesButton->OnClicked.AddDynamic(this, &ULibretroWidget::HandleStartNesClicked);
    }
    if (StartMM2RButton)
    {
        StartMM2RButton->OnClicked.AddDynamic(this, &ULibretroWidget::HandleStartMM2RClicked);
    }
    if (StartMM3Button)
    {
        StartMM3Button->OnClicked.AddDynamic(this, &ULibretroWidget::HandleStartMM3Clicked);
    }
}

void ULibretroWidget::HandleStartNesClicked()
{
    if (ALibretroPawn* Pawn = OwningPawn.Get())
    {
        Pawn->StartNesRom();
    }
}

void ULibretroWidget::HandleStartMM2RClicked()
{
    if (ALibretroPawn* Pawn = OwningPawn.Get())
    {
        Pawn->StartMetalMax2R();
    }
}

void ULibretroWidget::HandleStartMM3Clicked()
{
    if (ALibretroPawn* Pawn = OwningPawn.Get())
    {
        Pawn->StartMetalMax3();
    }
}

void ULibretroWidget::RefreshFromRunner()
{
    ALibretroPawn* Pawn = OwningPawn.Get();
    FLibretroNESRunner* Runner = Pawn ? Pawn->GetRunner() : nullptr;
    if (!Runner)
    {
        return;
    }

    if (VideoImage && Runner->GetVideoTexture())
    {
        UTexture2D* Texture = Runner->GetVideoTexture();
        if (CurrentVideoTexture != Texture)
        {
            CurrentVideoTexture = Texture;
            VideoImage->SetBrushFromTexture(Texture, false);
            VideoImage->SetDesiredSizeOverride(FVector2D(256.0f, 384.0f));
            VideoImage->SetColorAndOpacity(FLinearColor::White);
            UE_LOG(LogLibretroWidget, Log, TEXT("Bound video texture to UMG image: %s"), *GetNameSafe(Texture));
        }
    }

    if (StatusText)
    {
        const FString RunnerStatus = Runner->GetStatusText();
        StatusText->SetText(RunnerStatus.IsEmpty() ? DefaultStatusText() : FText::FromString(RunnerStatus));
    }
}
