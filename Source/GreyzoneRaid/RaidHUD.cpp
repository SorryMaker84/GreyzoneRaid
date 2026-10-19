#include "RaidHUD.h"
#include "RaidTreatmentComponent.h"

#include "RaidExtractionZone.h"
#include "RaidGameMode.h"
#include "RaidLoot.h"
#include "RaidPlayer.h"
#include "RaidInteractionComponent.h"
#include "RaidInventoryComponent.h"
#include "RaidPlayerController.h"
#include "Camera/CameraComponent.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogRaidHUD, Log, All);

namespace RaidUI
{
    const FLinearColor White(0.93f, 0.95f, 0.9f, 1.f);
    const FLinearColor Muted(0.62f, 0.68f, 0.66f, 0.95f);
    const FLinearColor Green(0.57f, 0.87f, 0.57f, 1.f);
    const FLinearColor Amber(0.95f, 0.72f, 0.38f, 1.f);
    const FLinearColor Red(0.98f, 0.3f, 0.24f, 1.f);
    const FLinearColor Border(0.35f, 0.4f, 0.36f, 0.7f);

    // Frontier's generated level uses these exact centimetre bounds and routes.
    // +X is east and +Y is north; mission and exit markers use live actor positions.
    struct FFrontierRegion
    {
        const TCHAR* Name;
        FVector2D Min;
        FVector2D Max;
        FVector2D Label;
        FLinearColor Color;
    };
    const FFrontierRegion FrontierRegions[] = {
        { TEXT("通讯台地区"), { 0.f, 3000.f }, { 8000.f, 7000.f }, { 4000.f, 6500.f },
            FLinearColor(0.55f, 0.43f, 0.19f, 0.28f) },
        { TEXT("燃料与排水区"), { 3000.f, -7000.f }, { 8000.f, 3000.f }, { 5650.f, 1200.f },
            FLinearColor(0.32f, 0.45f, 0.39f, 0.28f) },
        { TEXT("中央仓储区"), { -3000.f, -4200.f }, { 3000.f, 3000.f }, { -250.f, 1600.f },
            FLinearColor(0.39f, 0.43f, 0.46f, 0.28f) },
        { TEXT("后勤检查站"), { -8000.f, -7000.f }, { -3000.f, 1000.f }, { -5350.f, -1000.f },
            FLinearColor(0.38f, 0.39f, 0.25f, 0.28f) },
        { TEXT("营房医疗区"), { -8000.f, 1000.f }, { 0.f, 7000.f }, { -4800.f, 5600.f },
            FLinearColor(0.22f, 0.44f, 0.42f, 0.28f) }
    };

    // Measuring the actual Canvas item keeps kerning, font fallback and DPI
    // identical to rendering, including the runtime Chinese font face.
    class FMeasuredTextItem : public FCanvasTextItem
    {
    public:
        using FCanvasTextItem::FCanvasTextItem;
        FVector2D Measure(float DPIScale) const { return GetTextSize(DPIScale); }
    };

    void ConfigureText(FCanvasTextItem& Item, const UFont* Font,
        float FontScale, float UIScale, float DPIScale)
    {
        if (Font->FontCacheType == EFontCacheType::Runtime)
        {
            FSlateFontInfo FontInfo = Font->GetLegacySlateFontInfo();
            // Rasterize at the displayed size. Scaling a small atlas glyph up
            // makes even a runtime font look like the old enlarged bitmap font.
            FontInfo.Size = FMath::Clamp(FMath::RoundToInt(
                16.f * FontScale * UIScale / DPIScale), 1, 256);
            Item.SlateFontInfo = MoveTemp(FontInfo);
            Item.Scale = FVector2D(1.f, 1.f);
        }
        else
        {
            Item.Scale = FVector2D(FontScale * UIScale, FontScale * UIScale);
        }
    }
}

ARaidHUD::ARaidHUD()
{
    static ConstructorHelpers::FObjectFinder<UFont> ChineseFont(
        TEXT("/Game/UI/Fonts/F_RaidChinese.F_RaidChinese"));
    HUDFont = ChineseFont.Object;
    if (!HUDFont)
    {
        UE_LOG(LogRaidHUD, Error, TEXT("Missing cooked HUD font /Game/UI/Fonts/F_RaidChinese.F_RaidChinese; engine font fallback will be used."));
    }
}

void ARaidHUD::BeginPlay()
{
    Super::BeginPlay();
    SetObservedPlayer(GetOwningPlayerController()
        ? Cast<ARaidPlayer>(GetOwningPlayerController()->GetPawn()) : nullptr);
    ObserveMissionMode(Cast<ARaidGameMode>(UGameplayStatics::GetGameMode(this)));
}

void ARaidHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bEndingPlay = true;
    ObserveMissionMode(nullptr);
    UnbindObservedPlayer();
    Super::EndPlay(EndPlayReason);
}

void ARaidHUD::SetObservedPlayer(ARaidPlayer* Player)
{
    if (bEndingPlay) return;
    if (!Player)
    {
        UnbindObservedPlayer();
        return;
    }
    if (ObservedPlayer.Get() == Player) return;
    UnbindObservedPlayer();
    if (!IsValid(Player)) return;
    ObservedPlayer = Player;
    ObservedBackpack = Player->GetInventoryComponent();
    ObservedInteraction = Player->GetInteractionComponent();
    if (URaidInventoryComponent* Backpack = ObservedBackpack.Get())
    {
        BackpackChangedHandle = Backpack->OnInventoryChanged.AddUObject(this, &ARaidHUD::HandleInventoryChanged);
        BackpackEndingHandle = Backpack->OnInventoryEndingPlay.AddUObject(this, &ARaidHUD::HandleInventoryEndingPlay);
    }
    if (URaidInteractionComponent* Interaction = ObservedInteraction.Get())
    {
        InteractionChangedHandle = Interaction->OnInteractionChanged.AddUObject(this, &ARaidHUD::HandleInteractionChanged);
        InteractionEndingHandle = Interaction->OnInteractionEndingPlay.AddUObject(this, &ARaidHUD::HandleInteractionEndingPlay);
    }
    RefreshBackpackView();
    HandleInteractionChanged(ObservedInteraction.Get());
}

ARaidPlayer* ARaidHUD::GetObservedPlayer() const
{
    return ObservedPlayer.Get();
}

void ARaidHUD::UnbindObservedPlayer()
{
    CloseTacticalMap();
    InvalidateInventoryButtons();
    BindContainer(nullptr);
    if (URaidInventoryComponent* Backpack = ObservedBackpack.Get())
    {
        Backpack->OnInventoryChanged.Remove(BackpackChangedHandle);
        Backpack->OnInventoryEndingPlay.Remove(BackpackEndingHandle);
    }
    if (URaidInteractionComponent* Interaction = ObservedInteraction.Get())
    {
        Interaction->OnInteractionChanged.Remove(InteractionChangedHandle);
        Interaction->OnInteractionEndingPlay.Remove(InteractionEndingHandle);
    }
    BackpackChangedHandle.Reset();
    BackpackEndingHandle.Reset();
    InteractionChangedHandle.Reset();
    InteractionEndingHandle.Reset();
    ObservedPlayer.Reset();
    ObservedBackpack.Reset();
    ObservedInteraction.Reset();
    RenderedPlayer.Reset();
    RenderedOpenLoot.Reset();
    RenderedContext = FRaidInventoryCommandContext();
    InventoryView.Reset();
    InventoryViewRevision = INDEX_NONE;
    InteractionViewRevision = INDEX_NONE;
    InventoryItemCount = 0;
    InventoryTotalValue = 0;
    InventoryCapacity = 0;
    InventoryMedkitCount = 0;
    bInventoryViewOpen = false;
    bViewSearching = false;
    SearchEntryId = INDEX_NONE;
    SearchRowIndex = INDEX_NONE;
    ContainerPage = 0;
    InventoryPage = 0;
    LastInventoryFeedbackItemId = NAME_None;
    LastInventoryFeedbackLabel.Reset();
    LastInventoryFeedbackUnitValue = INDEX_NONE;
}

void ARaidHUD::BindContainer(ARaidLoot* Container)
{
    if (URaidInventoryComponent* Previous = ObservedContainerInventory.Get())
    {
        Previous->OnInventoryChanged.Remove(ContainerChangedHandle);
        Previous->OnInventoryEndingPlay.Remove(ContainerEndingHandle);
    }
    ContainerChangedHandle.Reset();
    ContainerEndingHandle.Reset();
    ObservedContainer = Container;
    ObservedContainerInventory = Container ? Container->GetInventoryComponent() : nullptr;
    if (URaidInventoryComponent* Inventory = ObservedContainerInventory.Get())
    {
        ContainerChangedHandle = Inventory->OnInventoryChanged.AddUObject(this, &ARaidHUD::HandleInventoryChanged);
        ContainerEndingHandle = Inventory->OnInventoryEndingPlay.AddUObject(this, &ARaidHUD::HandleInventoryEndingPlay);
    }
    RefreshContainerView();
}

void ARaidHUD::HandleInventoryChanged(URaidInventoryComponent* Inventory, int64 Revision)
{
    // Events invalidate yesterday's hitboxes immediately, even before another draw.
    InvalidateInventoryButtons();
    if (Inventory == ObservedBackpack.Get()) RefreshBackpackView();
    else if (Inventory == ObservedContainerInventory.Get()) RefreshContainerView();
}

void ARaidHUD::HandleInventoryEndingPlay(URaidInventoryComponent* Inventory)
{
    if (Inventory == ObservedBackpack.Get()) UnbindObservedPlayer();
    else if (Inventory == ObservedContainerInventory.Get())
    {
        InvalidateInventoryButtons();
        BindContainer(nullptr);
    }
}

void ARaidHUD::HandleInteractionChanged(URaidInteractionComponent* Interaction)
{
    if (Interaction != ObservedInteraction.Get()) return;
    InvalidateInventoryButtons();
    const bool bWasOpen = bInventoryViewOpen;
    bInventoryViewOpen = Interaction && Interaction->IsInventoryOpen();
    if (bInventoryViewOpen) CloseTacticalMap();
    bViewSearching = Interaction && Interaction->IsSearching();
    SearchEntryId = Interaction ? Interaction->GetSearchEntryId() : INDEX_NONE;
    InteractionViewRevision = Interaction ? Interaction->GetSessionRevision() : INDEX_NONE;
    ARaidLoot* Container = Interaction ? Interaction->GetOpenLoot() : nullptr;
    if (ObservedContainer.Get() != Container || bWasOpen != bInventoryViewOpen)
    {
        ContainerPage = 0;
        InventoryPage = 0;
        LastInventoryFeedbackItemId = NAME_None;
        LastInventoryFeedbackLabel.Reset();
        LastInventoryFeedbackUnitValue = INDEX_NONE;
        BindContainer(Container);
    }
    SearchRowIndex = ContainerView.IndexOfByPredicate([this](const FRaidInventoryEntry& Entry)
    {
        return Entry.EntryId == SearchEntryId;
    });
}

void ARaidHUD::HandleInteractionEndingPlay(URaidInteractionComponent* Interaction)
{
    if (Interaction == ObservedInteraction.Get()) UnbindObservedPlayer();
}

void ARaidHUD::RefreshBackpackView()
{
    InventoryView.Reset();
    InventoryItemCount = 0;
    InventoryTotalValue = 0;
    InventoryCapacity = 0;
    InventoryMedkitCount = 0;
    InventoryViewRevision = INDEX_NONE;
    if (const URaidInventoryComponent* Backpack = ObservedBackpack.Get())
    {
        InventoryView = Backpack->GetEntries();
        InventoryViewRevision = Backpack->GetRevision();
        InventoryItemCount = Backpack->GetItemCount();
        InventoryTotalValue = Backpack->GetTotalValue();
        InventoryCapacity = Backpack->GetCapacity();
        for (const FRaidInventoryEntry& Entry : InventoryView)
        {
            if (Entry.Definition.UseEffect == ERaidItemUseEffect::Heal) InventoryMedkitCount += Entry.Quantity;
        }
    }
    ++InventoryViewRefreshCount;
}

void ARaidHUD::RefreshContainerView()
{
    ContainerView.Reset();
    ContainerRows.Reset();
    RevealedRows = 0;
    UnsearchedRows = 0;
    TakenRows = 0;
    ContainerViewRevision = INDEX_NONE;
    ContainerName.Reset();
    if (const ARaidLoot* Container = ObservedContainer.Get()) ContainerName = Container->GetContainerName();
    if (const URaidInventoryComponent* Inventory = ObservedContainerInventory.Get())
    {
        ContainerView = Inventory->GetEntries();
        ContainerViewRevision = Inventory->GetRevision();
        for (int32 Index = 0; Index < ContainerView.Num(); ++Index)
        {
            const FRaidInventoryEntry& Entry = ContainerView[Index];
            if (Entry.Quantity > 0)
            {
                ContainerRows.Add(Index);
                if (Entry.bRevealed) ++RevealedRows;
                else ++UnsearchedRows;
            }
            else if (Entry.bRevealed)
            {
                ContainerRows.Add(Index);
                ++TakenRows;
            }
        }
        if (RevealedRows + UnsearchedRows == 0) ContainerRows.Reset();
    }
}

void ARaidHUD::RegisterInventoryCommand(FName Action, ERaidInventoryCommand Command,
    const FRaidInventoryEntry* Entry)
{
    if (!ActiveInventoryButtons.Contains(Action)) return;
    FRaidHUDInventoryAction& Target = InventoryCommands.Add(Action);
    Target.Command = Command;
    if (Entry)
    {
        Target.EntryId = Entry->EntryId;
        Target.FeedbackDefinition = Entry->Definition;
    }
}

void ARaidHUD::ObserveMissionMode(ARaidGameMode* GameMode)
{
    if (ObservedMissionMode.Get() == GameMode) return;
    if (ARaidGameMode* Previous = ObservedMissionMode.Get())
    {
        Previous->OnMissionChanged.Remove(MissionChangedHandle);
    }
    MissionChangedHandle.Reset();
    ObservedMissionMode = GameMode;
    MissionView = GameMode ? GameMode->GetMissionSnapshot() : FRaidMissionSnapshot();
    LastMissionCompletedTime = -100.f;
    LastMissionInterruptedTime = -100.f;
    CloseTacticalMap();
    if (GameMode)
    {
        MissionChangedHandle = GameMode->OnMissionChanged.AddUObject(
            this, &ARaidHUD::HandleMissionChanged);
    }
}

void ARaidHUD::HandleMissionChanged(ARaidGameMode* GameMode)
{
    if (bEndingPlay || !GameMode || ObservedMissionMode.Get() != GameMode) return;
    const FRaidMissionSnapshot Next = GameMode->GetMissionSnapshot();
    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    if (Next.bCompleted && !MissionView.bCompleted) LastMissionCompletedTime = Now;
    if (MissionView.bInProgress && Next.bPresent && !Next.bInProgress && !Next.bCompleted)
    {
        LastMissionInterruptedTime = Now;
    }
    if (Next.bInProgress) LastMissionInterruptedTime = -100.f;
    MissionView = Next;
    if (!MissionView.bPresent) CloseTacticalMap();
}

void ARaidHUD::ToggleTacticalMap()
{
    if (bTacticalMapOpen)
    {
        CloseTacticalMap();
        return;
    }
    const ARaidGameMode* GameMode = ObservedMissionMode.Get();
    const ARaidPlayer* Player = ObservedPlayer.Get();
    if (!bEndingPlay && GameMode && GameMode->IsRaidActive() && Player && Player->CanAct()
        && !bInventoryViewOpen && GameMode->GetMissionSnapshot().bPresent)
    {
        bTacticalMapOpen = true;
        if (ARaidPlayerController* PC=Cast<ARaidPlayerController>(GetOwningPlayerController()))
            PC->PrepareTacticalMapInput();
    }
}

void ARaidHUD::DrawHUD()
{
    Super::DrawHUD();
    ActiveMenuButtons.Reset();
    ActiveInventoryButtons.Reset();
    InventoryCommands.Reset();
    bInventorySnapshotValid = false;
    if (!Canvas || !GEngine)
    {
        return;
    }

    if (!HUDFont)
    {
        HUDFont = LoadObject<UFont>(nullptr, TEXT("/Game/UI/Fonts/F_RaidChinese.F_RaidChinese"));
        if (!HUDFont)
        {
            UE_LOG(LogRaidHUD, Warning, TEXT("Chinese HUD font could not be loaded; falling back to the engine medium font."));
            HUDFont = GEngine->GetMediumFont();
        }
        if (!HUDFont)
        {
            return;
        }
    }

    // All layout values are in reference pixels. The uniform scale preserves
    // spacing at different viewport resolutions and aspect ratios.
    UIScale = FMath::Max(0.1f, FMath::Min(Canvas->ClipX / 1280.f, Canvas->ClipY / 720.f));
    ScreenWidth = Canvas->ClipX / UIScale;
    ScreenHeight = Canvas->ClipY / UIScale;

    ARaidGameMode* GameMode = Cast<ARaidGameMode>(UGameplayStatics::GetGameMode(this));
    ObserveMissionMode(GameMode);
    // State changes are event driven. Only the active six-second hold needs a
    // fresh time-derived progress value while drawing.
    if (GameMode && MissionView.bInProgress) MissionView = GameMode->GetMissionSnapshot();
    if (GameMode)
    {
        const uint8 CurrentState = static_cast<uint8>(GameMode->State);
        if (CurrentState != LastMenuState)
        {
            CloseTacticalMap();
            HoveredButton = NAME_None;
            LastMenuState = CurrentState;
        }
        switch (GameMode->State)
        {
        case ERaidState::MainMenu:
            DrawMainMenu();
            return;
        case ERaidState::Instructions:
            DrawInstructions();
            return;
        case ERaidState::Extracted:
        case ERaidState::Dead:
            DrawResult(GameMode);
            return;
        default:
            break;
        }
    }

    const ARaidPlayer* Player = GetOwningPlayerController()
        ? Cast<ARaidPlayer>(GetOwningPlayerController()->GetPawn()) : nullptr;
    if (!Player || !GameMode || Player != ObservedPlayer.Get())
    {
        DrawLabel(TEXT("正在准备行动…"), ScreenWidth * 0.5f, 48.f, RaidUI::White, 1.f, true);
        return;
    }

    if (GameMode->IsPauseMenuOpen())
    {
        DrawPauseMenu(GameMode);
        return;
    }
    DrawDamageVignette(Player);
    DrawDamageDirection(Player);
    DrawRunningHUD(Player, GameMode);
    if (bInventoryViewOpen)
    {
        DrawInventory(Player);
    }
    else
    {
        RenderedOpenLoot.Reset();
        RenderedPlayer.Reset();
        ContainerPage = 0;
        InventoryPage = 0;
        if (bTacticalMapOpen && MissionView.bPresent)
        {
            DrawTacticalMap(Player);
            // Damage remains visible while consulting a map that does not pause.
            DrawDamageVignette(Player);
            DrawDamageDirection(Player);
        }
        else DrawCrosshair(Player);
    }
}

void ARaidHUD::NotifyHitBoxClick(FName BoxName)
{
    Super::NotifyHitBoxClick(BoxName);
    ARaidPlayerController* PC = Cast<ARaidPlayerController>(GetOwningPlayerController());
    ARaidGameMode* GameMode = Cast<ARaidGameMode>(UGameplayStatics::GetGameMode(this));
    if (ActiveInventoryButtons.Contains(BoxName))
    {
        if (!bInventorySnapshotValid || !PC || !PC->IsInventoryContextCurrent(RenderedContext))
        {
            InvalidateInventoryButtons();
            return;
        }
        // Delegates can refresh the view during submission; copy before consuming
        // this frame's targets. Row names identify widgets, never inventory entries.
        const FRaidInventoryCommandContext Context = RenderedContext;
        const FRaidHUDInventoryAction* Found = InventoryCommands.Find(BoxName);
        const bool bHasCommand = Found != nullptr;
        const FRaidHUDInventoryAction Action = Found ? *Found : FRaidHUDInventoryAction();
        InvalidateInventoryButtons();
        if (BoxName == TEXT("ContainerPrev")) --ContainerPage;
        else if (BoxName == TEXT("ContainerNext")) ++ContainerPage;
        else if (BoxName == TEXT("InventoryPrev")) --InventoryPage;
        else if (BoxName == TEXT("InventoryNext")) ++InventoryPage;
        else if (bHasCommand)
        {
            const ERaidInventoryResultCode Result = PC->SubmitInventoryCommand(Action.Command, Action.EntryId, Context);
            const URaidInteractionComponent* CurrentInteraction = ObservedInteraction.Get();
            if (Result == ERaidInventoryResultCode::Success && Action.EntryId != INDEX_NONE &&
                ObservedPlayer.Get() == Context.Player.Get() &&
                ObservedContainer.Get() == Context.Container.Get() && CurrentInteraction &&
                CurrentInteraction->GetSessionRevision() == Context.SessionRevision)
            {
                LastInventoryFeedbackItemId = Action.FeedbackDefinition.ItemId;
                LastInventoryFeedbackLabel = Action.FeedbackDefinition.DisplayName;
                LastInventoryFeedbackUnitValue = Action.FeedbackDefinition.UnitValue;
                LastInventoryFeedbackTime = GetWorld()->GetTimeSeconds();
                bLastFeedbackInventoryOnly = Action.Command == ERaidInventoryCommand::Use;
            }
        }
        return;
    }
    if (!ActiveMenuButtons.Contains(BoxName) || !GameMode || !PC) return;
    const bool bMainAction = GameMode->State == ERaidState::MainMenu &&
        (BoxName == TEXT("Start") || BoxName == TEXT("Instructions") || BoxName == TEXT("Quit"));
    const bool bBackAction = GameMode->State == ERaidState::Instructions && BoxName == TEXT("Back");
    const bool bResultAction = (GameMode->State == ERaidState::Extracted || GameMode->State == ERaidState::Dead) &&
        (BoxName == TEXT("Restart") || BoxName == TEXT("Menu"));
    const bool bPauseAction = GameMode->IsPauseMenuOpen() &&
        (BoxName == TEXT("Resume") || BoxName == TEXT("Abandon") ||
         BoxName == TEXT("CancelAbandon") || BoxName == TEXT("ConfirmAbandon"));
    if (bMainAction || bBackAction || bResultAction || bPauseAction)
    {
        ActiveMenuButtons.Reset();
        HoveredButton = NAME_None;
        PC->HandleMenuAction(BoxName);
    }
}

void ARaidHUD::NotifyHitBoxBeginCursorOver(FName BoxName)
{
    Super::NotifyHitBoxBeginCursorOver(BoxName);
    if (ActiveMenuButtons.Contains(BoxName) || ActiveInventoryButtons.Contains(BoxName))
    {
        HoveredButton = BoxName;
    }
}

void ARaidHUD::NotifyHitBoxEndCursorOver(FName BoxName)
{
    Super::NotifyHitBoxEndCursorOver(BoxName);
    if (HoveredButton == BoxName)
    {
        HoveredButton = NAME_None;
    }
}

void ARaidHUD::DrawMainMenu()
{
    const bool bFrontier = MissionView.bPresent;
    // The right side stays open to the real depot briefing camera. A solid left
    // panel and a narrow fade keep the menu readable over any time of day.
    DrawRect(FLinearColor(0.008f, 0.014f, 0.012f, 0.96f), 0.f, 0.f,
        526.f * UIScale, Canvas->ClipY);
    constexpr int32 FadeStrips = 32;
    for (int32 Strip = 0; Strip < FadeStrips; ++Strip)
    {
        const float Fade = 1.f - static_cast<float>(Strip) / FadeStrips;
        DrawRect(FLinearColor(0.008f, 0.014f, 0.012f, 0.9f * Fade * Fade),
            (526.f + Strip * 5.f) * UIScale, 0.f, 5.f * UIScale, Canvas->ClipY);
    }
    const float Left = 52.f;
    DrawUILine(Left, 52.f, Left + 62.f, 52.f, RaidUI::Amber, 3.f);
    DrawLabel(TEXT("单人行动  //  搜集与撤离"), Left, 67.f, RaidUI::Amber, 0.88f);
    DrawLabel(TEXT("灰域"), Left - 3.f, 94.f, RaidUI::White, 3.4f);
    DrawLabel(TEXT("突围"), Left - 1.f, 172.f, RaidUI::White, 2.45f);
    DrawLabel(bFrontier ? TEXT("前线边境  /  五区探索行动") : TEXT("仓储区  /  交战区域"),
        Left, 245.f, RaidUI::Muted, 0.95f);
    DrawLabel(TEXT("打开容器，搜索物资，取舍生存。"), Left, 277.f, RaidUI::White, 0.9f);
    DrawLabel(bFrontier ? TEXT("深入通讯台地，下载情报解锁北侧近路。") :
        TEXT("逐件拿取战利品，活着抵达撤离点。"), Left, 304.f, RaidUI::Muted, 0.9f);

    DrawMenuButton(TEXT("Start"), TEXT("开始行动"), Left, 353.f, 396.f, 64.f, true);
    DrawMenuButton(TEXT("Instructions"), TEXT("操作说明"), Left, 433.f, 396.f, 58.f);
    DrawMenuButton(TEXT("Quit"), TEXT("退出游戏"), Left, 507.f, 396.f, 58.f);
    DrawLabel(TEXT("独自行动 · 背包 8 件 / 12 kg · 安全位置治疗"), Left, 606.f, RaidUI::Muted, 0.86f);
    DrawLabel(TEXT("每次重新开始都会重置战场。"), Left, 633.f, RaidUI::Muted, 0.86f);
    DrawLabel(TEXT("离线模式  /  本地行动"), Left, ScreenHeight - 31.f, RaidUI::Amber, 0.82f);

    const float BriefX = ScreenWidth - 444.f;
    DrawPanel(BriefX, 52.f, 392.f, 240.f, RaidUI::Amber);
    DrawLabel(TEXT("区域简报"), BriefX + 22.f, 71.f, RaidUI::Amber, 0.86f);
    DrawLabel(bFrontier ? TEXT("前线边境") : TEXT("仓储区"), BriefX + 22.f, 99.f, RaidUI::White, 1.7f);
    DrawUILine(BriefX + 22.f, 145.f, BriefX + 370.f, 145.f, RaidUI::Border);
    DrawLabel(TEXT("潜入方位    西南"), BriefX + 22.f, 162.f, RaidUI::Muted, 0.9f);
    DrawLabel(bFrontier ? TEXT("撤离路线    南侧 / 北侧近路") : TEXT("撤离方位    东北"),
        BriefX + 22.f, 192.f, RaidUI::Green, 0.9f);
    DrawLabel(TEXT("敌情情报    武装巡逻队"), BriefX + 22.f, 222.f, RaidUI::Muted, 0.9f);
    DrawLabel(bFrontier ? TEXT("可选目标    长按 E 下载情报") : TEXT("行动目标    搜索、取舍、撤离"),
        BriefX + 22.f, 252.f, RaidUI::White, 0.9f);

    const float ObjectiveY = ScreenHeight - 176.f;
    DrawSoftBackdrop(BriefX, ObjectiveY, 392.f, 132.f, 0.9f);
    DrawUILine(BriefX, ObjectiveY, BriefX, ObjectiveY + 132.f, RaidUI::Green, 3.f);
    DrawLabel(TEXT("撤离须知"), BriefX + 22.f, ObjectiveY + 17.f, RaidUI::Green, 0.9f);
    DrawFittedLabel(TEXT("携带至少 1 件物资进入绿色区域。"), BriefX + 22.f,
        ObjectiveY + 47.f, 348.f, RaidUI::White, 0.86f);
    DrawLabel(TEXT("在区域内存活 8 秒即可撤离。"), BriefX + 22.f,
        ObjectiveY + 76.f, RaidUI::Muted, 0.86f);
    DrawFittedLabel(TEXT("离开区域或受伤，倒计时重置。"), BriefX + 22.f,
        ObjectiveY + 101.f, 348.f, RaidUI::Muted, 0.82f);
}

void ARaidHUD::DrawInstructions()
{
    const bool bFrontier = MissionView.bPresent;
    DrawRect(FLinearColor(0.006f, 0.012f, 0.01f, 0.76f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
    const float Width = 1080.f;
    const float Height = 626.f;
    const float X = (ScreenWidth - Width) * 0.5f;
    const float Y = (ScreenHeight - Height) * 0.5f;
    DrawPanel(X, Y, Width, Height, RaidUI::Amber);
    DrawLabel(bFrontier ? TEXT("战地手册  //  前线边境") : TEXT("战地手册  //  仓储区行动"),
        X + 28.f, Y + 20.f, RaidUI::Amber, 0.86f);
    DrawLabel(TEXT("操作说明"), X + 28.f, Y + 49.f, RaidUI::White, 1.65f);
    DrawUILine(X + 28.f, Y + 95.f, X + Width - 28.f, Y + 95.f, RaidUI::Border);
    DrawLabel(TEXT("操作按键"), X + 28.f, Y + 111.f, RaidUI::Amber, 0.96f);
    DrawLabel(TEXT("行动流程"), X + 565.f, Y + 111.f, RaidUI::Amber, 0.96f);
    DrawUILine(X + 526.f, Y + 111.f, X + 526.f, Y + 517.f, RaidUI::Border);

    struct FControlRow { const TCHAR* Key; const TCHAR* Description; };
    const FControlRow Controls[] = {
        { TEXT("WASD"), TEXT("移动") },
        { TEXT("鼠标"), TEXT("转动视角") },
        { TEXT("空格"), TEXT("跳跃") },
        { TEXT("左 SHIFT"), TEXT("冲刺") },
        { TEXT("鼠标左键"), TEXT("按住连射；空匣有备弹自动换弹") },
        { TEXT("鼠标右键"), TEXT("按住瞄准") },
        { bFrontier ? TEXT("R / M") : TEXT("R"), bFrontier ? TEXT("换弹 / 查看战术地图") : TEXT("换弹") },
        { TEXT("E"), bFrontier ? TEXT("容器按 E；通讯终端长按 E 6 秒") : TEXT("打开容器，在界面搜索与拿取") },
        { TEXT("TAB"), TEXT("打开 / 关闭背包，战斗继续") },
        { TEXT("Q"), TEXT("3 秒治疗，最多 +45；再按取消") },
        { TEXT("ESC"), TEXT("先关面板；无面板时暂停行动") }
    };
    for (int32 Row = 0; Row < static_cast<int32>(UE_ARRAY_COUNT(Controls)); ++Row)
    {
        const float RowY = Y + 150.f + Row * 31.f;
        if (Row % 2 == 0)
        {
            DrawRect(FLinearColor(0.35f, 0.4f, 0.32f, 0.06f), (X + 22.f) * UIScale,
                (RowY - 5.f) * UIScale, 482.f * UIScale, 29.f * UIScale);
        }
        DrawFittedLabel(Controls[Row].Key, X + 28.f, RowY, 126.f, RaidUI::White, 0.86f);
        DrawFittedLabel(Controls[Row].Description, X + 168.f, RowY, 332.f, RaidUI::Muted, 0.86f);
    }
    DrawLabel(TEXT("背包 8 件 / 12 kg；放回减重，治疗受击中断"), X + 28.f, Y + 500.f, RaidUI::Green, 0.9f);

    const float MissionX = X + 565.f;
    DrawLabel(TEXT("01  搜集物资"), MissionX, Y + 150.f, RaidUI::White, 1.05f);
    DrawLabel(TEXT("靠近容器按 E 打开，自动逐件搜索。"), MissionX, Y + 180.f, RaidUI::Muted, 0.86f);
    DrawLabel(TEXT("逐件揭示后手动拿取；满包也能继续搜索。"), MissionX, Y + 206.f, RaidUI::Muted, 0.86f);
    DrawLabel(bFrontier ? TEXT("02  可选：深入下载情报") : TEXT("02  设法生存"),
        MissionX, Y + 252.f, RaidUI::White, 1.05f);
    DrawLabel(bFrontier ? TEXT("通讯台地上层，长按 E 6 秒解锁北侧撤离。") :
        TEXT("武装巡逻队会发现、追击并向你开火。"), MissionX, Y + 282.f, RaidUI::Muted, 0.86f);
    DrawLabel(bFrontier ? TEXT("松手、远离、失去视线或受伤会中断下载。") :
        TEXT("背包中可放回物资、使用医疗包和补充弹药。"), MissionX, Y + 308.f, RaidUI::Muted, 0.86f);
    DrawLabel(TEXT("03  成功撤离"), MissionX, Y + 354.f, RaidUI::Green, 1.05f);
    DrawLabel(bFrontier ? TEXT("带至少 1 件物资，前往开放的南侧或北侧。") :
        TEXT("带至少 1 件物资，前往东北绿色区域。"), MissionX, Y + 384.f, RaidUI::Muted, 0.86f);
    DrawLabel(TEXT("在区域内存活 8 秒，带走所获物资。"), MissionX, Y + 410.f, RaidUI::Muted, 0.86f);
    DrawLabel(TEXT("离开区域或受伤都会重置倒计时。"), MissionX, Y + 436.f, RaidUI::Amber, 0.86f);
    DrawLabel(TEXT("阵亡或放弃行动，携带的物资全部丢失。"), MissionX, Y + 484.f, RaidUI::Red, 0.86f);
    DrawUILine(X + 28.f, Y + 535.f, X + Width - 28.f, Y + 535.f, RaidUI::Border);
    DrawMenuButton(TEXT("Back"), TEXT("返回主菜单"), X + 28.f, Y + 552.f, 290.f, 54.f);
    DrawRightLabel(TEXT("点击按钮返回主菜单"), X + Width - 28.f,
        Y + 571.f, RaidUI::Muted, 0.86f);
}

void ARaidHUD::DrawPauseMenu(const ARaidGameMode* GameMode)
{
    const bool bConfirm = GameMode->IsAbandonConfirmationOpen();
    const float X = (ScreenWidth - 600.f) * .5f;
    const float Y = (ScreenHeight - 360.f) * .5f;
    DrawRect(FLinearColor(.006f, .012f, .01f, .78f), 0, 0, Canvas->ClipX, Canvas->ClipY);
    DrawPanel(X, Y, 600.f, 360.f, bConfirm ? RaidUI::Red : RaidUI::Amber);
    DrawLabel(bConfirm ? TEXT("确认放弃本次行动？") : TEXT("行动已暂停"), X + 30, Y + 28,
        RaidUI::White, 1.55f);
    DrawLabel(TEXT("战场与倒计时已暂停。"), X + 30, Y + 86, RaidUI::Muted, .9f);
    DrawFittedLabel(bConfirm ? FString::Printf(TEXT("将损失 %d 件物资 / $%lld；无法恢复本次行动。"),
        InventoryItemCount, InventoryTotalValue) : TEXT("继续行动后，生命、物资和换弹进度保持不变。"),
        X + 30, Y + 121, 540, bConfirm ? RaidUI::Red : RaidUI::Muted, .87f);
    DrawMenuButton(bConfirm ? TEXT("CancelAbandon") : TEXT("Resume"),
        bConfirm ? TEXT("返回暂停菜单") : TEXT("继续行动  [ESC / ENTER]"),
        X + 30, Y + 179, 540, 58, true);
    DrawMenuButton(bConfirm ? TEXT("ConfirmAbandon") : TEXT("Abandon"),
        bConfirm ? TEXT("确认放弃，结算损失") : TEXT("放弃本次行动…"), X + 30, Y + 251, 540, 54);
    DrawLabel(bConfirm ? TEXT("[ESC] 返回   [ENTER] 直接继续行动") : TEXT("操作提示：R 换弹 · Q 治疗 · M 地图"),
        X + 30, Y + 325, RaidUI::Muted, .8f);
}

void ARaidHUD::DrawMenuButton(FName Action, const FString& Label, float X, float Y,
    float Width, float Height, bool bPrimary)
{
    const bool bHovered = HoveredButton == Action;
    const FLinearColor Accent = bPrimary ? RaidUI::Amber : RaidUI::Green;
    const FLinearColor Fill = bHovered ? FLinearColor(0.13f, 0.18f, 0.12f, 0.98f) :
        (bPrimary ? FLinearColor(0.12f, 0.105f, 0.06f, 0.98f) : FLinearColor(0.035f, 0.05f, 0.039f, 0.98f));
    const FLinearColor Outline = bHovered ? Accent : (bPrimary ? RaidUI::Amber : RaidUI::Border);
    DrawRect(Fill, X * UIScale, Y * UIScale, Width * UIScale, Height * UIScale);
    DrawUILine(X, Y, X + Width, Y, Outline, bHovered ? 2.f : 1.f);
    DrawUILine(X, Y + Height, X + Width, Y + Height, Outline);
    DrawUILine(X, Y, X, Y + Height, Accent, bHovered ? 4.f : 2.f);
    DrawUILine(X + Width, Y, X + Width, Y + Height, Outline);
    constexpr float ButtonFontScale = 1.1f;
    const float TextY = Y + (Height - MeasureLabel(Label, ButtonFontScale).Y) * 0.5f;
    DrawFittedLabel(Label, X + 23.f, TextY, Width - 72.f, RaidUI::White, ButtonFontScale);
    DrawRightLabel(TEXT(">"), X + Width - 19.f, TextY, bHovered ? Accent : RaidUI::Muted, ButtonFontScale);
    ActiveMenuButtons.Add(Action);
    // AHUD works in physical Canvas pixels; use the very same scale as the art.
    AddHitBox(FVector2D(X * UIScale, Y * UIScale),
        FVector2D(Width * UIScale, Height * UIScale), Action, true, 100);
}

void ARaidHUD::DrawRunningHUD(const ARaidPlayer* Player, const ARaidGameMode* GameMode)
{
    const float Margin = 32.f;
    const float CenterX = ScreenWidth * 0.5f;
    ARaidExtractionZone* Extraction = FindExtraction(Player);
    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    const bool bFrontier = MissionView.bPresent;
    const bool bAtExtraction = Extraction && Extraction->IsPlayerInside();
    const bool bExtracting = bAtExtraction && (!bFrontier || Extraction->IsAvailable());
    if (bWasExtracting && !bExtracting)
    {
        LastExtractionResetTime = Now;
    }
    bWasExtracting = bExtracting && InventoryItemCount > 0;

    // The normal HUD uses small transparent strips instead of enclosing the view.
    if (bFrontier)
    {
        DrawFrontierBrief(Player);
    }
    else
    {
        DrawUILine(Margin, 27.f, Margin, 69.f, RaidUI::Amber, 2.f);
        DrawLabel(TEXT("灰域  //  战地行动"), Margin + 12.f, 25.f, RaidUI::Amber, 0.82f);
        DrawFittedLabel(TEXT("搜集物资 · 设法生存 · 成功撤离"), Margin + 12.f, 47.f, 342.f, RaidUI::White, 0.87f);
        if (!bInventoryViewOpen)
        {
            DrawFittedLabel(InventoryItemCount > 0 ? TEXT("活着撤离，才能带走战利品") :
                TEXT("按 [E] 打开容器，再搜索并拿取物资"), Margin + 12.f, 72.f, 330.f,
                RaidUI::Muted, 0.82f);
        }
    }
    DrawCompass(Player, bFrontier ? nullptr : Extraction);
    if (bFrontier && !bInventoryViewOpen)
    {
        if (Extraction)
        {
            DrawNavigationMarker(Player, Extraction->GetActorLocation(), Extraction->GetDisplayName(),
                Extraction->IsAvailable() ? RaidUI::Green : RaidUI::Amber, 85.f);
        }
        if (!MissionView.bCompleted)
        {
            DrawNavigationMarker(Player, MissionView.WorldLocation, TEXT("情报"), RaidUI::Amber, 121.f);
        }
    }
    DrawRightLabel(TEXT("行动  ") + FormatTime(GameMode->RaidElapsed), ScreenWidth - Margin,
        26.f, RaidUI::White, 0.95f);
    DrawRightLabel(FString::Printf(TEXT("击毙  %02d"), GameMode->EnemiesKilled),
        ScreenWidth - Margin, 53.f, RaidUI::Muted, 0.82f);

    if (!bInventoryViewOpen)
    {
        const float Kg = Player->GetInventoryComponent()->GetWeightGrams()/1000.f;
        DrawRightLabel(FString::Printf(TEXT("负重 %.1f / %.0f kg · %s"), Kg, Player->GetInventoryComponent()->GetWeightCapacityGrams()/1000.f,
            Player->IsTooHeavyToSprint() ? TEXT("无法冲刺") : Kg > 6.f ? TEXT("移动减速") : TEXT("轻装")),
            ScreenWidth - Margin, 178.f, Kg > 6.f ? RaidUI::Amber : RaidUI::Muted, .78f);
        if (Player->IsTreating())
        {
            const float Progress = Player->GetTreatmentComponent()->GetProgress();
            DrawSoftBackdrop(CenterX - 230.f, ScreenHeight - 230.f, 460.f, 62.f, .9f);
            DrawLabel(FString::Printf(TEXT("治疗中 %d%% · [Q] 取消"), FMath::FloorToInt(Progress * 100)),
                CenterX, ScreenHeight - 225.f, RaidUI::Green, .95f, true);
            DrawLabel(TEXT("开火、冲刺或受伤中断 · 完成才消耗"), CenterX, ScreenHeight - 201.f, RaidUI::Muted, .75f, true);
            DrawBar(CenterX - 210.f, ScreenHeight - 178.f, 420.f, 4.f, Progress, RaidUI::Green);
        }
    }
    const float StatusY = ScreenHeight - 140.f;
    const FLinearColor HealthColor = Player->Health <= 30.f ? RaidUI::Red : RaidUI::Green;
    DrawSoftBackdrop(Margin - 10.f, StatusY, 286.f, 112.f, 0.6f);
    DrawLabel(TEXT("生存状态"), Margin, StatusY + 4.f, RaidUI::Muted, 0.82f);
    DrawRightLabel(FString::Printf(TEXT("[Q] 医疗包 %d"), CountMedkits(Player)), Margin + 238.f,
        StatusY + 4.f, CountMedkits(Player) > 0 ? RaidUI::White : RaidUI::Muted, 0.82f);
    DrawLabel(FString::Printf(TEXT("%03d"), FMath::CeilToInt(FMath::Max(0.f, Player->Health))),
        Margin, StatusY + 23.f, HealthColor, 1.65f);
    DrawLabel(TEXT("生命"), Margin + 79.f, StatusY + 26.f, RaidUI::Muted, 0.82f);
    DrawBar(Margin + 79.f, StatusY + 55.f, 159.f, 4.f, Player->Health / 100.f, HealthColor);
    const FLinearColor PackColor = InventoryItemCount >= InventoryCapacity ? RaidUI::Amber : RaidUI::White;
    DrawFittedLabel(FString::Printf(TEXT("[TAB] 背包 %d/%d"), InventoryItemCount, InventoryCapacity),
        Margin, StatusY + 69.f, 172.f, PackColor, 0.82f);
    DrawRightLabel(FString::Printf(TEXT("$%lld"), InventoryTotalValue), Margin + 238.f,
        StatusY + 69.f, RaidUI::Green, 0.82f);
    DrawLabel(TEXT("成功撤离才计入本局结算"), Margin, StatusY + 91.f,
        RaidUI::Muted, 0.82f);

    const float AmmoX = ScreenWidth - Margin - 198.f;
    DrawSoftBackdrop(AmmoX - 12.f, StatusY, 222.f, 112.f, 0.6f);
    DrawFittedLabel(Player->WeaponDisplayName, AmmoX, StatusY + 4.f, 130.f, RaidUI::Muted, 0.82f);
    DrawRightLabel(TEXT("全自动"), AmmoX + 198.f, StatusY + 4.f, RaidUI::Muted, 0.82f);
    const FLinearColor AmmoColor = Player->MagazineAmmo == 0 ? RaidUI::Amber : RaidUI::White;
    DrawLabel(FString::Printf(TEXT("%02d"), Player->MagazineAmmo), AmmoX, StatusY + 23.f,
        AmmoColor, 1.9f);
    DrawLabel(FString::Printf(TEXT("/ %02d"), Player->MaxMagazineAmmo), AmmoX + 66.f,
        StatusY + 38.f, RaidUI::Muted, 0.9f);
    DrawRightLabel(FString::Printf(TEXT("备用 %02d"), Player->ReserveAmmo), AmmoX + 198.f,
        StatusY + 38.f, RaidUI::Muted, 0.82f);
    DrawFittedLabel(Player->bReloading ? (Player->bAutomaticReload ? TEXT("自动换弹 · 松开左键停火") : TEXT("换弹中 · 松开左键停火")) : TEXT("[R] 换弹   空匣自动换弹"), AmmoX,
        StatusY + 66.f, 198.f, Player->bReloading ? RaidUI::Amber : RaidUI::Muted, 0.82f);
    if (Player->bReloading)
    {
        DrawBar(AmmoX, StatusY + 101.f, 198.f, 3.f, Player->ReloadProgress, RaidUI::Amber);
    }
    else
    {
        const int32 Slots = FMath::Clamp(Player->MaxMagazineAmmo, 1, 24);
        const float SlotWidth = (198.f - (Slots - 1) * 3.f) / Slots;
        for (int32 Slot = 0; Slot < Slots; ++Slot)
        {
            const float Fraction = static_cast<float>(Slot) / Slots;
            DrawRect(Fraction < static_cast<float>(Player->MagazineAmmo) /
                FMath::Max(1, Player->MaxMagazineAmmo) ? AmmoColor : RaidUI::Border,
                (AmmoX + Slot * (SlotWidth + 3.f)) * UIScale,
                (StatusY + 101.f) * UIScale, SlotWidth * UIScale, 3.f * UIScale);
        }
    }

    DrawFittedLabel(bFrontier ?
        TEXT("WASD 移动   SHIFT 冲刺   左键 射击   E 交互 / 长按下载   TAB 背包   M 地图   ESC 关闭 / 暂停") :
        TEXT("WASD 移动   SHIFT 冲刺   空格 跳跃   左键 射击   E 打开容器   TAB 背包   ESC 关闭面板 / 暂停"),
        CenterX, ScreenHeight - 27.f, ScreenWidth - 64.f, RaidUI::Muted, 0.82f, true);

    if (bInventoryViewOpen)
    {
        return;
    }

    DrawLootToast(Player);
    if (bFrontier) DrawMissionFeedback();

    // Interaction owns the centre strip while focused. Extraction takes priority
    // inside its trigger so the time remaining can never be obscured by a crate.
    const ARaidLoot* Loot = Player->GetFocusedLoot();
    const float InteractionY = ScreenHeight - 184.f;
    if (bExtracting)
    {
        const bool bHasLoot = InventoryItemCount > 0;
        const float Remaining = FMath::Max(0.f, Extraction->GetSecondsRemaining());
        const float Progress = bHasLoot
            ? 1.f - Remaining / FMath::Max(0.1f, Extraction->ExtractSeconds) : 0.f;
        DrawSoftBackdrop(CenterX - 230.f, InteractionY - 8.f, 460.f, 118.f, 0.7f);
        DrawLabel(TEXT("正在撤离"), CenterX, InteractionY, RaidUI::Green, 0.82f, true);
        DrawLabel(bHasLoot ? FString::Printf(TEXT("剩余 %.1f 秒"), Remaining) : TEXT("背包为空"),
            CenterX, InteractionY + 24.f, bHasLoot ? RaidUI::White : RaidUI::Amber,
            bHasLoot ? 1.15f : 1.f, true);
        DrawBar(CenterX - 176.f, InteractionY + 56.f, 352.f, 4.f, Progress, RaidUI::Green);
        DrawLabel(bHasLoot ? FString::Printf(TEXT("可带出 %d 件物资  /  $%lld"),
            InventoryItemCount, InventoryTotalValue) : TEXT("需要先收集至少 1 件物资"),
            CenterX, InteractionY + 68.f, bHasLoot ? RaidUI::Green : RaidUI::Muted, 0.82f, true);
        DrawLabel(bHasLoot ? TEXT("留在区域内，离开或受伤将重置计时。") :
            TEXT("按 [E] 打开容器，再搜索并拿取物资"), CenterX, InteractionY + 91.f,
            RaidUI::Muted, 0.82f, true);
    }
    else if (bFrontier && bAtExtraction && !Extraction->IsAvailable())
    {
        DrawSoftBackdrop(CenterX - 230.f, InteractionY + 5.f, 460.f, 90.f, 0.72f);
        DrawLabel(Extraction->GetDisplayName() + TEXT(" / 锁定"), CenterX,
            InteractionY + 11.f, RaidUI::Amber, 0.95f, true);
        DrawFittedLabel(Extraction->GetUnavailableReason(), CenterX,
            InteractionY + 39.f, 424.f, RaidUI::White, 0.88f, true);
        DrawLabel(TEXT("南侧撤离始终开放"), CenterX,
            InteractionY + 66.f, RaidUI::Green, 0.82f, true);
    }
    else if (bFrontier && (MissionView.bInProgress ||
        (ObservedInteraction.IsValid() && ObservedInteraction->GetFocusedObjective())))
    {
        DrawSoftBackdrop(CenterX - 230.f, InteractionY - 8.f, 460.f, 112.f, 0.72f);
        DrawLabel(MissionView.bCompleted ? TEXT("情报下载完成") : TEXT("通讯终端 / 可选目标"),
            CenterX, InteractionY, MissionView.bCompleted ? RaidUI::Green : RaidUI::Amber, 0.9f, true);
        const FString HoldText = MissionView.bCompleted ? TEXT("北侧撤离已开放") :
            (MissionView.bInProgress ? FString::Printf(TEXT("保持按住 [E]  ·  剩余 %.1f 秒"),
                (1.f - FMath::Clamp(MissionView.Progress, 0.f, 1.f)) * MissionView.HoldSeconds) :
                FString::Printf(TEXT("长按 [E] %.0f 秒下载情报"), MissionView.HoldSeconds));
        DrawLabel(HoldText, CenterX, InteractionY + 26.f, RaidUI::White, 1.05f, true);
        DrawBar(CenterX - 176.f, InteractionY + 59.f, 352.f, 4.f,
            MissionView.Progress, MissionView.bCompleted ? RaidUI::Green : RaidUI::Amber);
        DrawLabel(MissionView.bCompleted ? TEXT("携带物资前往任一开放撤离点") :
            TEXT("松手、远离、失去视线或受伤会中断下载"), CenterX,
            InteractionY + 76.f, RaidUI::Muted, 0.82f, true);
    }
    else if (Loot)
    {
        DrawSoftBackdrop(CenterX - 230.f, InteractionY + 5.f, 460.f, 90.f, 0.72f);
        DrawLabel(Loot->bEnemyDrop ? TEXT("敌人掉落物") : TEXT("物资容器"),
            CenterX, InteractionY + 11.f, RaidUI::Muted, 0.82f, true);
        DrawFittedLabel(TEXT("[E] 打开  ") + Loot->GetContainerName(),
            CenterX, InteractionY + 36.f, 424.f, RaidUI::White, 1.f, true);
        DrawLabel(TEXT("在容器界面搜索、拿取和放回物资"), CenterX,
            InteractionY + 66.f, RaidUI::Green, 0.82f, true);
    }
    else if (Now - LastExtractionResetTime >= 0.f && Now - LastExtractionResetTime < 2.8f)
    {
        DrawSoftBackdrop(CenterX - 230.f, InteractionY + 24.f, 460.f, 54.f, 0.65f);
        DrawLabel(TEXT("撤离倒计时已重置"), CenterX, InteractionY + 28.f,
            RaidUI::Amber, 0.9f, true);
        DrawLabel(TEXT("返回撤离区域，并留在区域内"), CenterX, InteractionY + 53.f,
            RaidUI::Muted, 0.82f, true);
    }
    else if (!GameMode->Message.IsEmpty() &&
        !(GameMode->Message.StartsWith(TEXT("已获得 ")) &&
            Now - Player->LastLootTime >= 0.f && Now - Player->LastLootTime < 3.f))
    {
        DrawFittedLabel(GameMode->Message, CenterX, InteractionY + 43.f, 460.f,
            RaidUI::Amber, 0.82f, true);
    }

    if (Extraction && !bExtracting && !bFrontier)
    {
        const float DistanceMeters = FVector::Dist(Player->GetActorLocation(),
            Extraction->GetActorLocation()) / 100.f;
        DrawLabel(FString::Printf(TEXT("撤离点  /  %.0f 米"), DistanceMeters), CenterX,
            ScreenHeight - 50.f, RaidUI::Green, 0.82f, true);
    }
}

FString ARaidHUD::GetFrontierRegion(const FVector& Location) const
{
    for (const RaidUI::FFrontierRegion& Region : RaidUI::FrontierRegions)
    {
        if (Location.X >= Region.Min.X && Location.X <= Region.Max.X &&
            Location.Y >= Region.Min.Y && Location.Y <= Region.Max.Y) return Region.Name;
    }
    return TEXT("南侧通道");
}

void ARaidHUD::DrawFrontierBrief(const ARaidPlayer* Player)
{
    const float X = 32.f;
    const FLinearColor MissionColor = MissionView.bCompleted ? RaidUI::Green : RaidUI::Amber;
    DrawSoftBackdrop(X - 10.f, 20.f, 346.f, bInventoryViewOpen ? 62.f : 157.f, 0.52f);
    DrawUILine(X, 27.f, X, bInventoryViewOpen ? 70.f : 161.f, MissionColor, 2.f);
    DrawFittedLabel(TEXT("前线边境  //  ") + GetFrontierRegion(Player->GetActorLocation()),
        X + 12.f, 25.f, 316.f, RaidUI::Amber, 0.82f);
    DrawFittedLabel(InventoryItemCount == 0 ? TEXT("01  搜索并拿取至少 1 件物资") :
        TEXT("02  携带物资，存活并撤离"),
        X + 12.f, 49.f, 316.f, MissionColor, 0.93f);
    if (!bInventoryViewOpen)
    {
        if (MissionView.bInProgress)
        {
            DrawLabel(FString::Printf(TEXT("保持按住 [E]  /  下载 %02d%%"),
                FMath::RoundToInt(FMath::Clamp(MissionView.Progress, 0.f, 1.f) * 100.f)),
                X + 12.f, 77.f, RaidUI::White, 0.85f);
            DrawBar(X + 12.f, 102.f, 292.f, 4.f, MissionView.Progress, RaidUI::Amber);
            DrawLabel(TEXT("松手或受伤会中断下载"), X + 12.f, 114.f, RaidUI::Muted, 0.8f);
        }
        else
        {
            DrawFittedLabel(InventoryItemCount == 0 ? TEXT("靠近容器按 [E] → 搜索 → 手动拿取") :
                (MissionView.bCompleted ? TEXT("南北均开放：进入绿区，存活 8 秒") :
                TEXT("南侧 S-01：进入绿区，存活 8 秒")),
                X + 12.f, 77.f, 312.f, RaidUI::White, 0.85f);
            DrawFittedLabel(MissionView.bCompleted ? TEXT("可选情报已完成；北侧近路已开放") :
                TEXT("可选：通讯台地长按 E，解锁北侧近路"),
                X + 12.f, 102.f, 312.f, RaidUI::Muted, 0.8f);
        }
        DrawLabel(TEXT("[M] 战术地图"), X + 12.f, 142.f, RaidUI::Muted, 0.82f);
    }

    // Both exits stay visible. Their availability is independent of backpack contents.
    int32 ExitRow = 0;
    const FName ExitIds[] = { TEXT("S-01"), TEXT("N-02") };
    for (const FName ExitId : ExitIds)
    {
        for (TActorIterator<ARaidExtractionZone> It(GetWorld()); It; ++It)
        {
            if (It->GetExtractionId() != ExitId) continue;
            const float Distance = FVector::Dist2D(Player->GetActorLocation(), It->GetActorLocation()) / 100.f;
            const FString Line = FString::Printf(TEXT("%s  %s  %.0f 米"),
                *It->GetDisplayName(), It->IsAvailable() ? TEXT("开放") : TEXT("锁定"), Distance);
            DrawRightLabel(Line, ScreenWidth - 32.f, 84.f + ExitRow * 26.f,
                It->IsAvailable() ? RaidUI::Green : RaidUI::Amber, 0.82f);
            ++ExitRow;
            break;
        }
    }
    if (!MissionView.bCompleted)
    {
        DrawRightLabel(TEXT("北侧近路：需下载情报"), ScreenWidth - 32.f,
            138.f, RaidUI::Muted, 0.78f);
    }
}

void ARaidHUD::DrawNavigationMarker(const ARaidPlayer* Player, const FVector& Location,
    const FString& Label, const FLinearColor& Color, float Y)
{
    const FVector Direction = Location - Player->GetActorLocation();
    const float Bearing = FMath::RadiansToDegrees(FMath::Atan2(
        static_cast<float>(Direction.Y), static_cast<float>(Direction.X)));
    const float Delta = FMath::FindDeltaAngleDegrees(
        static_cast<float>(Player->GetControlRotation().Yaw), Bearing);
    const float MarkerX = ScreenWidth * 0.5f + FMath::Clamp(Delta, -60.f, 60.f) * 2.4f;
    DrawRect(Color, (MarkerX - 2.f) * UIScale, Y * UIScale, 4.f * UIScale, 4.f * UIScale);
    FString Caption = FString::Printf(TEXT("%s  %.0f 米"), *Label, Direction.Size2D() / 100.f);
    if (Delta < -60.f) Caption = TEXT("< ") + Caption;
    else if (Delta > 60.f) Caption += TEXT(" >");
    DrawFittedLabel(Caption, MarkerX, Y + 10.f, 230.f, Color, 0.8f, true);
}

void ARaidHUD::DrawMissionFeedback()
{
    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    const bool bCompleted = Now - LastMissionCompletedTime >= 0.f && Now - LastMissionCompletedTime < 5.f;
    const bool bInterrupted = Now - LastMissionInterruptedTime >= 0.f && Now - LastMissionInterruptedTime < 3.5f;
    if (!bCompleted && !bInterrupted) return;
    const float X = ScreenWidth * 0.5f;
    const FLinearColor Color = bCompleted ? RaidUI::Green : RaidUI::Amber;
    DrawSoftBackdrop(X - 210.f, 172.f, 420.f, 68.f, 0.8f);
    DrawUILine(X - 210.f, 172.f, X - 210.f, 240.f, Color, 3.f);
    DrawLabel(bCompleted ? TEXT("情报下载完成") : TEXT("下载中断"),
        X, 181.f, Color, 1.f, true);
    DrawLabel(bCompleted ? TEXT("北侧撤离已开放") : TEXT("重新长按 [E] 开始下载"),
        X, 212.f, RaidUI::White, 0.86f, true);
}

void ARaidHUD::DrawTacticalMap(const ARaidPlayer* Player)
{
    const float Width = 1080.f;
    const float Height = 620.f;
    const float X = (ScreenWidth - Width) * 0.5f;
    const float Y = (ScreenHeight - Height) * 0.5f;
    DrawRect(FLinearColor(0.005f, 0.012f, 0.012f, 0.67f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
    DrawPanel(X, Y, Width, Height, RaidUI::Amber);
    DrawLabel(TEXT("战术地图  //  前线边境"), X + 28.f, Y + 19.f, RaidUI::White, 1.25f);
    DrawRightLabel(TEXT("[M] / [ESC] 关闭地图"), X + Width - 28.f,
        Y + 25.f, RaidUI::Muted, 0.84f);
    DrawUILine(X + 28.f, Y + 60.f, X + Width - 28.f, Y + 60.f, RaidUI::Border);

    const float MapX = X + 28.f;
    const float MapY = Y + 82.f;
    const float MapW = 576.f;
    const float MapH = 504.f;
    const auto Project = [MapX, MapY, MapW, MapH](const FVector2D& Position)
    {
        return FVector2D(MapX + FMath::Clamp((Position.X + 8000.f) / 16000.f, 0.f, 1.f) * MapW,
            MapY + (1.f - FMath::Clamp((Position.Y + 7000.f) / 14000.f, 0.f, 1.f)) * MapH);
    };
    DrawRect(FLinearColor(0.038f, 0.058f, 0.052f, 1.f), MapX * UIScale, MapY * UIScale,
        MapW * UIScale, MapH * UIScale);
    // Reverse painter order matches GetFrontierRegion's priority at shared bounds.
    for (int32 Index = UE_ARRAY_COUNT(RaidUI::FrontierRegions) - 1; Index >= 0; --Index)
    {
        const RaidUI::FFrontierRegion& Region = RaidUI::FrontierRegions[Index];
        const FVector2D TopLeft = Project({ Region.Min.X, Region.Max.Y });
        const FVector2D BottomRight = Project({ Region.Max.X, Region.Min.Y });
        DrawRect(Region.Color, TopLeft.X * UIScale, TopLeft.Y * UIScale,
            (BottomRight.X - TopLeft.X) * UIScale, (BottomRight.Y - TopLeft.Y) * UIScale);
        DrawUILine(TopLeft.X, TopLeft.Y, BottomRight.X, TopLeft.Y, RaidUI::Border);
        DrawUILine(TopLeft.X, TopLeft.Y, TopLeft.X, BottomRight.Y, RaidUI::Border);
        DrawUILine(BottomRight.X, TopLeft.Y, BottomRight.X, BottomRight.Y, RaidUI::Border);
        DrawUILine(TopLeft.X, BottomRight.Y, BottomRight.X, BottomRight.Y, RaidUI::Border);
    }
    const FLinearColor Grid(0.47f, 0.56f, 0.50f, 0.10f);
    for (int32 Column = 0; Column <= 8; ++Column)
        DrawUILine(MapX + Column * 72.f, MapY, MapX + Column * 72.f, MapY + MapH, Grid);
    for (int32 Row = 0; Row <= 7; ++Row)
        DrawUILine(MapX, MapY + Row * 72.f, MapX + MapW, MapY + Row * 72.f, Grid);

    // Seven public building footprints from the authored Frontier layout.
    // These are static geography, never enemy or unsearched-loot information.
    const FVector4 Buildings[] = {
        FVector4(-7100, -5300, -5900, -4300), FVector4(-5900, -3200, -3700, -1400),
        FVector4(-7100, 1950, -4700, 3650), FVector4(-5000, 3900, -2600, 5500),
        FVector4(-2400, -1400, 1800, 1800), FVector4(3800, -3850, 5800, -2350),
        FVector4(600, 3600, 3000, 5400)
    };
    for (const FVector4& Building : Buildings)
    {
        const FVector2D A = Project({Building.X, Building.W});
        const FVector2D B = Project({Building.Z, Building.Y});
        DrawRect(FLinearColor(.035f, .045f, .041f, .65f), A.X * UIScale, A.Y * UIScale,
            (B.X - A.X) * UIScale, (B.Y - A.Y) * UIScale);
        const FLinearColor Outline(.67f, .72f, .64f, .75f);
        DrawUILine(A.X, A.Y, B.X, A.Y, Outline);
        DrawUILine(B.X, A.Y, B.X, B.Y, Outline);
        DrawUILine(B.X, B.Y, A.X, B.Y, Outline);
        DrawUILine(A.X, B.Y, A.X, A.Y, Outline);
    }

    const FVector2D MainRoute[] = {
        {-7000.f, -6100.f}, {-7000.f, -5500.f}, {-5700.f, -5500.f}, {-5700.f, -3600.f},
        {-3000.f, -3500.f}, {-3000.f, -2200.f}, {-1000.f, -2200.f}, {-1000.f, -400.f},
        {700.f, -350.f}, {1550.f, -350.f}, {1550.f, 400.f}, {1900.f, 400.f},
        {2700.f, 2400.f}, {4200.f, 2550.f}, {4200.f, 3600.f}, {3400.f, 3600.f},
        {3400.f, 3450.f}, {1800.f, 3450.f}, {1800.f, 3750.f}
    };
    const FVector2D WestRoute[] = {
        {-7400.f, -4200.f}, {-7400.f, 1400.f}, {-5900.f, 1500.f}, {-5900.f, 2100.f},
        {-6200.f, 2650.f}, {-6050.f, 2800.f}, {-5650.f, 2800.f}, {-4500.f, 2800.f},
        {-3900.f, 3300.f}, {-2200.f, 3300.f}, {-1300.f, 4400.f}, {-1000.f, 4400.f}, {400.f, 4400.f}
    };
    const FVector2D SouthRoute[] = {
        {1900.f, 400.f}, {3200.f, -1400.f}, {3500.f, -2100.f}, {6100.f, -2150.f},
        {6800.f, -3500.f}, {7600.f, -3900.f}, {7600.f, -5500.f}, {6900.f, -6000.f}
    };
    const auto DrawRoute = [this, &Project](const FVector2D* Points, int32 Count, const FLinearColor& Color)
    {
        for (int32 Index = 1; Index < Count; ++Index)
        {
            const FVector2D A = Project(Points[Index - 1]);
            const FVector2D B = Project(Points[Index]);
            DrawUILine(A.X, A.Y, B.X, B.Y, FLinearColor(0.015f, 0.027f, 0.021f, 0.85f), 7.f);
            DrawUILine(A.X, A.Y, B.X, B.Y, Color, 2.f);
        }
    };
    DrawRoute(MainRoute, UE_ARRAY_COUNT(MainRoute), FLinearColor(0.76f, 0.61f, 0.36f, 0.8f));
    DrawRoute(WestRoute, UE_ARRAY_COUNT(WestRoute), FLinearColor(0.51f, 0.65f, 0.60f, 0.7f));
    DrawRoute(SouthRoute, UE_ARRAY_COUNT(SouthRoute), FLinearColor(0.51f, 0.65f, 0.60f, 0.7f));
    const FVector2D NorthRoute[] = {{3150.f, 4500.f}, {3650.f, 5300.f}, {5000.f, 5700.f}, {5700.f, 5900.f}, {6500.f, 5900.f}};
    const FVector2D DrainageRoute[] = {{1000.f, -4550.f}, {7200.f, -4550.f}, {7600.f, -5100.f}, {6900.f, -6000.f}};
    DrawRoute(NorthRoute, UE_ARRAY_COUNT(NorthRoute), FLinearColor(0.76f, 0.61f, 0.36f, 0.8f));
    DrawRoute(DrainageRoute, UE_ARRAY_COUNT(DrainageRoute), FLinearColor(0.36f, 0.60f, 0.70f, 0.8f));
    for (const RaidUI::FFrontierRegion& Region : RaidUI::FrontierRegions)
    {
        const FVector2D Label = Project(Region.Label);
        DrawLabel(Region.Name, Label.X, Label.Y, RaidUI::White, 0.83f, true);
    }
    DrawLabel(TEXT("北 / N"), MapX + MapW * 0.5f, MapY - 19.f, RaidUI::Muted, 0.74f, true);
    DrawLabel(TEXT("20 米 / 格"), MapX + 7.f, MapY + MapH - 23.f, RaidUI::Muted, 0.72f);

    const FVector2D Mission = Project(FVector2D(MissionView.WorldLocation));
    const FLinearColor MissionColor = MissionView.bCompleted ? RaidUI::Green : RaidUI::Amber;
    DrawUILine(Mission.X, Mission.Y - 7.f, Mission.X + 7.f, Mission.Y, MissionColor, 2.f);
    DrawUILine(Mission.X + 7.f, Mission.Y, Mission.X, Mission.Y + 7.f, MissionColor, 2.f);
    DrawUILine(Mission.X, Mission.Y + 7.f, Mission.X - 7.f, Mission.Y, MissionColor, 2.f);
    DrawUILine(Mission.X - 7.f, Mission.Y, Mission.X, Mission.Y - 7.f, MissionColor, 2.f);
    DrawLabel(MissionView.bCompleted ? TEXT("情报 / 已完成") : TEXT("情报 / 上层"),
        Mission.X, Mission.Y + 12.f, MissionColor, 0.75f, true);

    for (TActorIterator<ARaidExtractionZone> It(GetWorld()); It; ++It)
    {
        const FVector2D Position = Project(FVector2D(It->GetActorLocation()));
        const FLinearColor Color = It->IsAvailable() ? RaidUI::Green : RaidUI::Amber;
        DrawRect(Color, (Position.X - 5.f) * UIScale, (Position.Y - 5.f) * UIScale,
            10.f * UIScale, 10.f * UIScale);
        const FString Caption = It->GetExtractionId().ToString() +
            (It->IsAvailable() ? TEXT(" / 开放") : TEXT(" / 锁定"));
        DrawRightLabel(Caption, FMath::Min(static_cast<float>(Position.X) + 12.f, MapX + MapW - 8.f),
            Position.Y + 12.f, Color, 0.75f);
    }

    const FVector2D PlayerPoint = Project(FVector2D(Player->GetActorLocation()));
    const float Heading = FMath::DegreesToRadians(static_cast<float>(Player->GetControlRotation().Yaw));
    const FVector2D Forward(FMath::Cos(Heading), -FMath::Sin(Heading));
    const FVector2D Right(-Forward.Y, Forward.X);
    const FVector2D Tip = PlayerPoint + Forward * 12.f;
    const FVector2D A = PlayerPoint - Forward * 6.f + Right * 6.f;
    const FVector2D B = PlayerPoint - Forward * 6.f - Right * 6.f;
    DrawUILine(Tip.X, Tip.Y, A.X, A.Y, RaidUI::White, 2.5f);
    DrawUILine(A.X, A.Y, B.X, B.Y, RaidUI::White, 2.5f);
    DrawUILine(B.X, B.Y, Tip.X, Tip.Y, RaidUI::White, 2.5f);
    DrawRect(RaidUI::White, (PlayerPoint.X - 2.f) * UIScale, (PlayerPoint.Y - 2.f) * UIScale,
        4.f * UIScale, 4.f * UIScale);

    const float SideX = X + 640.f;
    const float SideW = Width - 670.f;
    DrawLabel(TEXT("当前位置"), SideX, Y + 82.f, RaidUI::Muted, 0.82f);
    DrawFittedLabel(GetFrontierRegion(Player->GetActorLocation()), SideX, Y + 107.f,
        SideW, RaidUI::White, 1.2f);
    DrawLabel(TEXT("可选目标"), SideX, Y + 155.f, MissionColor, 0.82f);
    DrawFittedLabel(MissionView.Title, SideX, Y + 181.f, SideW, RaidUI::White, 1.f);
    DrawLabel(MissionView.bCompleted ? TEXT("情报下载完成，北侧近路已开放") :
        FString::Printf(TEXT("前往通讯台地上层，长按 [E] %.0f 秒"), MissionView.HoldSeconds),
        SideX, Y + 212.f, MissionColor, 0.85f);
    DrawFittedLabel(MissionView.bInProgress ? MissionView.StatusText :
        TEXT("可跳过情报，携带物资直接向南撤离"), SideX, Y + 241.f,
        SideW, RaidUI::Muted, 0.8f);
    DrawBar(SideX, Y + 269.f, SideW, 4.f, MissionView.Progress, MissionColor);
    DrawLabel(TEXT("撤离路线"), SideX, Y + 296.f, RaidUI::White, 0.95f);
    int32 ExitRow = 0;
    const FName ExitIds[] = { TEXT("S-01"), TEXT("N-02") };
    for (const FName ExitId : ExitIds)
    {
        for (TActorIterator<ARaidExtractionZone> It(GetWorld()); It; ++It)
        {
            if (It->GetExtractionId() != ExitId) continue;
            const float RowY = Y + 330.f + ExitRow * 46.f;
            DrawFittedLabel(It->GetDisplayName(), SideX, RowY, SideW - 118.f, RaidUI::White, 0.9f);
            DrawRightLabel(It->IsAvailable() ? TEXT("开放") : TEXT("需下载情报"), SideX + SideW,
                RowY, It->IsAvailable() ? RaidUI::Green : RaidUI::Amber, 0.85f);
            DrawLabel(FString::Printf(TEXT("%s  /  距离 %.0f 米"), *ExitId.ToString(),
                FVector::Dist2D(Player->GetActorLocation(), It->GetActorLocation()) / 100.f),
                SideX, RowY + 23.f, RaidUI::Muted, 0.76f);
            ++ExitRow;
            break;
        }
    }
    DrawUILine(SideX, Y + 426.f, SideX + SideW, Y + 426.f, RaidUI::Border);
    DrawLabel(TEXT("撤离需携带至少 1 件物资，并停留 8 秒"),
        SideX, Y + 444.f, RaidUI::Green, 0.82f);
    DrawLabel(TEXT("白色：你的位置与朝向    菱形：情报"),
        SideX, Y + 480.f, RaidUI::Muted, 0.8f);
    DrawLabel(TEXT("方形：撤离点    线条：建议路线"),
        SideX, Y + 505.f, RaidUI::Muted, 0.8f);
    DrawFittedLabel(TEXT("地图不暂停；开火 / 瞄准 / E 返回现场"),
        SideX, Y + 544.f, SideW, RaidUI::Amber, 0.85f);
    DrawLabel(FString::Printf(TEXT("生命 %03d  /  弹药 %02d  /  背包 %d/%d"),
        FMath::CeilToInt(FMath::Max(0.f, Player->Health)), Player->MagazineAmmo,
        InventoryItemCount, InventoryCapacity), SideX, Y + 573.f, RaidUI::White, 0.82f);
}

void ARaidHUD::DrawCrosshair(const ARaidPlayer* Player)
{
    const float X = ScreenWidth * 0.5f;
    const float Y = ScreenHeight * 0.5f;
    FLinearColor Color = Player->GetFocusedLoot() ? RaidUI::Amber : RaidUI::White;
    if (!Player->bAiming)
    {
        FLinearColor ArmColor = Color;
        ArmColor.A *= 1.f - FMath::Clamp(Player->ADSAlpha, 0.f, 1.f);
        DrawUILine(X - 10.f, Y, X - 5.f, Y, ArmColor, 1.f);
        DrawUILine(X + 5.f, Y, X + 10.f, Y, ArmColor, 1.f);
        DrawUILine(X, Y - 10.f, X, Y - 5.f, ArmColor, 1.f);
        DrawUILine(X, Y + 5.f, X, Y + 10.f, ArmColor, 1.f);
    }
    DrawRect(Color, (X - 1.f) * UIScale, (Y - 1.f) * UIScale, 2.f * UIScale, 2.f * UIScale);

    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    const float HitAge = Now - Player->LastHitTime;
    const float KillAge = Now - Player->LastKillTime;
    const bool bKill = KillAge >= 0.f && KillAge < 0.45f;
    if (bKill || (HitAge >= 0.f && HitAge < 0.22f))
    {
        FLinearColor HitColor = bKill ? RaidUI::Red : RaidUI::Amber;
        HitColor.A = bKill ? 1.f - KillAge / 0.45f : 1.f - HitAge / 0.22f;
        DrawUILine(X - 5.f, Y - 5.f, X - 13.f, Y - 13.f, HitColor, 1.8f);
        DrawUILine(X + 5.f, Y - 5.f, X + 13.f, Y - 13.f, HitColor, 1.8f);
        DrawUILine(X - 5.f, Y + 5.f, X - 13.f, Y + 13.f, HitColor, 1.8f);
        DrawUILine(X + 5.f, Y + 5.f, X + 13.f, Y + 13.f, HitColor, 1.8f);
    }
    if (KillAge >= 0.f && KillAge < 0.85f)
    {
        FLinearColor KillColor = RaidUI::Red;
        KillColor.A = FMath::Clamp((0.85f - KillAge) / 0.35f, 0.f, 1.f);
        DrawLabel(TEXT("已击毙"), X, Y + 25.f, KillColor, 0.82f, true);
    }
}

void ARaidHUD::DrawCompass(const ARaidPlayer* Player, const ARaidExtractionZone* Extraction)
{
    const float CenterX = ScreenWidth * 0.5f;
    float Yaw = FMath::Fmod(static_cast<float>(Player->GetControlRotation().Yaw), 360.f);
    if (Yaw < 0.f)
    {
        Yaw += 360.f;
    }

    // The map uses +X east and +Y north, matching the named boundary walls.
    const TCHAR* Directions[] = { TEXT("东"), TEXT("东北"), TEXT("北"), TEXT("西北"),
        TEXT("西"), TEXT("西南"), TEXT("南"), TEXT("东南") };
    const int32 HeadingIndex = FMath::RoundToInt(Yaw / 45.f) % 8;
    DrawLabel(FString::Printf(TEXT("%s  %03d"), Directions[HeadingIndex],
        FMath::RoundToInt(Yaw) % 360), CenterX, 22.f, RaidUI::White, 0.82f, true);
    DrawUILine(CenterX - 3.f, 42.f, CenterX, 46.f, RaidUI::Amber, 1.5f);
    DrawUILine(CenterX + 3.f, 42.f, CenterX, 46.f, RaidUI::Amber, 1.5f);

    const float PixelsPerDegree = 2.4f;
    for (int32 Mark = 0; Mark < 72; ++Mark)
    {
        const float MarkYaw = Mark * 5.f;
        const float Delta = FMath::FindDeltaAngleDegrees(Yaw, MarkYaw);
        if (FMath::Abs(Delta) > 60.f)
        {
            continue;
        }
        const float X = CenterX + Delta * PixelsPerDegree;
        const bool bCardinal = Mark % 9 == 0;
        FLinearColor TickColor = RaidUI::Muted;
        TickColor.A *= (1.f - FMath::Abs(Delta) / 80.f) * 0.7f;
        DrawUILine(X, 51.f, X, bCardinal ? 61.f : 56.f, TickColor);
        if (bCardinal)
        {
            DrawLabel(Directions[(Mark / 9) % 8], X, 66.f,
                TickColor, 0.82f, true);
        }
    }

    if (Extraction)
    {
        const FVector Direction = Extraction->GetActorLocation() - Player->GetActorLocation();
        const float Bearing = FMath::RadiansToDegrees(FMath::Atan2(
            static_cast<float>(Direction.Y), static_cast<float>(Direction.X)));
        const float Delta = FMath::FindDeltaAngleDegrees(Yaw, Bearing);
        const float MarkerX = CenterX + FMath::Clamp(Delta, -60.f, 60.f) * PixelsPerDegree;
        DrawRect(RaidUI::Green, (MarkerX - 2.f) * UIScale, 85.f * UIScale,
            4.f * UIScale, 4.f * UIScale);
        const FString Marker = FMath::Abs(Delta) <= 60.f ? TEXT("撤离点") :
            (Delta < 0.f ? TEXT("< 撤离点") : TEXT("撤离点 >"));
        DrawLabel(Marker, MarkerX, 95.f, RaidUI::Green, 0.82f, true);
    }
}

void ARaidHUD::DrawDamageVignette(const ARaidPlayer* Player)
{
    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    const float DamageAge = Now - Player->LastDamageTime;
    const float HitStrength = DamageAge >= 0.f
        ? FMath::Clamp(1.f - DamageAge / 0.7f, 0.f, 1.f) * 0.32f : 0.f;
    const float LowHealth = FMath::Clamp((35.f - Player->Health) / 35.f, 0.f, 1.f);
    const float Pulse = 0.74f + 0.26f * FMath::Sin(Now * 4.5f);
    const float Strength = FMath::Max(HitStrength, LowHealth * Pulse * 0.22f);
    if (Strength <= 0.005f)
    {
        return;
    }

    // One-pixel rectangular shells meet exactly, including at the corners.
    // Pixel-aligned edges avoid the repeated alpha blending of overlapping bands.
    const int32 PixelWidth = FMath::Max(1, FMath::FloorToInt(Canvas->ClipX));
    const int32 PixelHeight = FMath::Max(1, FMath::FloorToInt(Canvas->ClipY));
    const int32 DepthPixels = FMath::Clamp(FMath::CeilToInt(76.f * UIScale), 1,
        FMath::Max(1, FMath::Min(PixelWidth, PixelHeight) / 2));
    for (int32 Pixel = 0; Pixel < DepthPixels; ++Pixel)
    {
        const float T = (static_cast<float>(Pixel) + 0.5f) / DepthPixels;
        const float Fade = (1.f - T) * (1.f - T) * (1.f + 2.f * T);
        const FLinearColor Color(0.48f, 0.012f, 0.008f, Strength * Fade);
        const float Width = static_cast<float>(PixelWidth - 2 * Pixel);
        const float SideHeight = static_cast<float>(PixelHeight - 2 * Pixel - 2);
        DrawRect(Color, Pixel, Pixel, Width, 1.f);
        DrawRect(Color, Pixel, PixelHeight - Pixel - 1, Width, 1.f);
        if (SideHeight > 0.f)
        {
            DrawRect(Color, Pixel, Pixel + 1, 1.f, SideHeight);
            DrawRect(Color, PixelWidth - Pixel - 1, Pixel + 1, 1.f, SideHeight);
        }
    }
}

void ARaidHUD::DrawDamageDirection(const ARaidPlayer* Player)
{
    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    const float Age = Now - Player->LastDamageTime;
    if (!Player->bHasDamageSource || !FMath::IsFinite(Player->DamageSourceYaw) ||
        Age < 0.f || Age >= 0.95f)
    {
        return;
    }

    const float CameraYaw = Player->Camera ? static_cast<float>(Player->Camera->GetComponentRotation().Yaw)
        : static_cast<float>(Player->GetControlRotation().Yaw);
    const float Bearing = FMath::DegreesToRadians(
        FMath::FindDeltaAngleDegrees(CameraYaw, Player->DamageSourceYaw));
    const float CenterX = ScreenWidth * 0.5f;
    const float CenterY = ScreenHeight * 0.5f;
    const float Radius = 136.f;
    FLinearColor Color = RaidUI::Red;
    Color.A = FMath::Clamp((0.95f - Age) / 0.4f, 0.f, 1.f);
    const FLinearColor Shadow(0.02f, 0.f, 0.f, Color.A * 0.45f);

    // Front is above the aim point, right is screen-right, rear is below.
    // This is a brief record of the last real hit, with no enemy tracking.
    auto ArcPoint = [&](float Angle, float Distance)
    {
        return FVector2D(CenterX + FMath::Sin(Angle) * Distance,
            CenterY - FMath::Cos(Angle) * Distance);
    };
    constexpr int32 Segments = 32;
    const float HalfArc = FMath::DegreesToRadians(15.f);
    const FVector2D Tip = ArcPoint(Bearing, Radius + 10.f);
    const FVector2D Left = ArcPoint(Bearing - 0.055f, Radius + 1.f);
    const FVector2D Right = ArcPoint(Bearing + 0.055f, Radius + 1.f);
    // Finish the entire outline before drawing the red stroke, so neighbouring
    // segment outlines cannot cut dark seams through the visible arc.
    for (int32 Pass = 0; Pass < 2; ++Pass)
    {
        const FLinearColor StrokeColor = Pass == 0 ? Shadow : Color;
        const float Thickness = Pass == 0 ? 3.4f : 2.2f;
        for (int32 Segment = 0; Segment < Segments; ++Segment)
        {
            const FVector2D Start = ArcPoint(Bearing - HalfArc + 2.f * HalfArc * Segment / Segments, Radius);
            const FVector2D End = ArcPoint(Bearing - HalfArc + 2.f * HalfArc * (Segment + 1) / Segments, Radius);
            DrawUILine(Start.X, Start.Y, End.X, End.Y, StrokeColor, Thickness);
        }
        DrawUILine(Left.X, Left.Y, Tip.X, Tip.Y, StrokeColor, Thickness);
        DrawUILine(Right.X, Right.Y, Tip.X, Tip.Y, StrokeColor, Thickness);
    }
}

void ARaidHUD::DrawLootToast(const ARaidPlayer* Player)
{
    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    const float Age = Now - Player->LastLootTime;
    if (Age < 0.f || Age >= 2.8f || Player->LastLootQuantity <= 0 || Player->LastLootLabel.IsEmpty())
    {
        return;
    }

    const float CenterX = ScreenWidth * 0.5f;
    const float Fade = FMath::Clamp((2.8f - Age) / 0.5f, 0.f, 1.f);
    FLinearColor Color = RaidUI::Green;
    Color.A *= Fade;
    FLinearColor DetailColor = RaidUI::Muted;
    DetailColor.A *= Fade;
    DrawSoftBackdrop(CenterX - 220.f, 128.f, 440.f, 67.f, 0.68f * Fade);
    DrawUILine(CenterX - 220.f, 128.f, CenterX - 220.f, 195.f, Color, 2.f);
    DrawRightLabel(FString::Printf(TEXT("×%d"), Player->LastLootQuantity),
        CenterX + 202.f, 140.f, Color, 0.95f);
    DrawFittedLabel(TEXT("已获得  ") + Player->LastLootLabel, CenterX - 202.f,
        140.f, 348.f, Color, 0.95f);
    DrawLabel(FString::Printf(TEXT("背包 %d/%d  /  可结算 $%lld"),
        InventoryItemCount, InventoryCapacity, InventoryTotalValue),
        CenterX, 170.f, DetailColor, 0.82f, true);
}

void ARaidHUD::InvalidateInventoryButtons()
{
    ActiveInventoryButtons.Reset();
    InventoryCommands.Reset();
    bInventorySnapshotValid = false;
    HoveredButton = NAME_None;
}

void ARaidHUD::DrawInventoryButton(FName Action, const FString& Label, float X, float Y,
    float Width, float Height, bool bEnabled, bool bPrimary)
{
    const bool bHovered = bEnabled && HoveredButton == Action;
    const bool bBlocked = !bEnabled && (Label == TEXT("背包已满") || Label == TEXT("生命已满"));
    const FLinearColor Accent = bPrimary ? RaidUI::Amber : RaidUI::Green;
    const FLinearColor Outline = bBlocked ? FLinearColor(0.68f, 0.49f, 0.24f, 0.8f) : !bEnabled ? RaidUI::Border :
        (bHovered || bPrimary ? Accent : RaidUI::Border);
    const FLinearColor Fill = bHovered ? FLinearColor(0.17f, 0.22f, 0.15f, 0.98f) :
        bBlocked ? FLinearColor(0.11f, 0.095f, 0.055f, 0.85f) :
        FLinearColor(0.035f, 0.05f, 0.039f, bEnabled ? 0.98f : 0.55f);
    DrawRect(Fill, X * UIScale, Y * UIScale, Width * UIScale, Height * UIScale);
    DrawUILine(X, Y, X + Width, Y, Outline, bHovered ? 2.f : 1.f);
    DrawUILine(X, Y + Height, X + Width, Y + Height, Outline);
    DrawUILine(X, Y, X, Y + Height, Outline, bHovered ? 3.f : 1.f);
    DrawUILine(X + Width, Y, X + Width, Y + Height, Outline);
    const float TextY = Y + (Height - MeasureLabel(Label, 0.82f).Y) * 0.5f;
    DrawFittedLabel(Label, X + Width * 0.5f, TextY, Width - 14.f,
        bHovered ? Accent : bBlocked ? RaidUI::Amber : bEnabled ? RaidUI::White : RaidUI::Muted, 0.82f, true);
    if (bEnabled)
    {
        ActiveInventoryButtons.Add(Action);
        AddHitBox(FVector2D(X * UIScale, Y * UIScale),
            FVector2D(Width * UIScale, Height * UIScale), Action, true, 100);
    }
}

void ARaidHUD::DrawInventory(const ARaidPlayer* Player)
{
    ARaidLoot* Loot = ObservedContainer.Get();
    RenderedPlayer = const_cast<ARaidPlayer*>(Player);
    RenderedOpenLoot = Loot;
    RenderedContext.Player = RenderedPlayer;
    RenderedContext.Container = Loot;
    RenderedContext.Backpack = ObservedBackpack;
    RenderedContext.ContainerInventory = ObservedContainerInventory;
    RenderedContext.SessionRevision = InteractionViewRevision;
    RenderedContext.BackpackRevision = InventoryViewRevision;
    RenderedContext.ContainerRevision = ContainerViewRevision;
    RenderedContext.bHadContainer = Loot != nullptr;
    bInventorySnapshotValid = true;
    const float SearchProgress = ObservedInteraction.IsValid() ? ObservedInteraction->GetSearchProgress() : 0.f;
    const float FeedbackAge = GetWorld()->GetTimeSeconds() - LastInventoryFeedbackTime;
    const float FeedbackAlpha = FeedbackAge >= 0.f ? FMath::Clamp(1.f - FeedbackAge / 1.6f, 0.f, 1.f) : 0.f;

    constexpr int32 RowsPerPage = 6;
    constexpr float Width = 1120.f;
    constexpr float Height = 590.f;
    constexpr float PaneWidth = 542.f;
    constexpr float RowHeight = 50.f;
    constexpr float ItemNameScale = 0.88f;
    constexpr float ItemDetailScale = 0.73f;
    const float X = (ScreenWidth - Width) * 0.5f;
    const float Y = (ScreenHeight - Height) * 0.5f;
    const float RightX = X + Width - PaneWidth;
    const float PaneY = Y + 63.f;
    const float RowsY = Y + 188.f;
    const bool bFull = InventoryItemCount >= InventoryCapacity;
    DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.42f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
    DrawLabel(Loot ? TEXT("现场搜刮") : TEXT("背包管理"), X, Y + 4.f, RaidUI::White, 1.5f);
    DrawLabel(TEXT("战斗仍在继续 · 留意周围"), X + 170.f, Y + 15.f, RaidUI::Amber, 0.86f);
    DrawInventoryButton(TEXT("ContainerClose"), TEXT("关闭  [ESC / TAB]"),
        X + Width - 178.f, Y + 11.f, 178.f, 36.f);
    RegisterInventoryCommand(TEXT("ContainerClose"), ERaidInventoryCommand::Close);
    DrawPanel(X, PaneY, PaneWidth, 479.f, RaidUI::Amber);
    DrawPanel(RightX, PaneY, PaneWidth, 479.f, RaidUI::Green);

    DrawLabel(TEXT("来源容器"), X + 18.f, PaneY + 14.f, RaidUI::Muted, 0.78f);
    DrawFittedLabel(Loot ? ContainerName : TEXT("未打开容器"),
        X + 18.f, PaneY + 39.f, 282.f, RaidUI::White, 1.08f);
    if (Loot)
    {
        DrawRightLabel(FString::Printf(TEXT("已揭示 %d / 未搜 %d"), RevealedRows, UnsearchedRows),
            X + PaneWidth - 18.f, PaneY + 16.f, UnsearchedRows > 0 ? RaidUI::Amber : RaidUI::Green, 0.78f);
        DrawRightLabel(FString::Printf(TEXT("已取空 %d 项"), TakenRows),
            X + PaneWidth - 18.f, PaneY + 45.f, RaidUI::Muted, 0.73f);
    }
    DrawLabel(TEXT("随身背包"), RightX + 18.f, PaneY + 14.f, RaidUI::White, 1.08f);
    DrawRightLabel(FString::Printf(TEXT("%d / %d 件"), InventoryItemCount, InventoryCapacity),
        RightX + PaneWidth - 18.f, PaneY + 16.f, bFull ? RaidUI::Amber : RaidUI::Green, 1.f);
    DrawLabel(FString::Printf(TEXT("携带价值  $%lld"), InventoryTotalValue),
        RightX + 18.f, PaneY + 45.f, RaidUI::Green, 0.9f);
    DrawRightLabel(FString::Printf(TEXT("%.1f / %.1f kg"), Player->GetInventoryComponent()->GetWeightGrams()/1000.f, Player->GetInventoryComponent()->GetWeightCapacityGrams()/1000.f),
        RightX + PaneWidth - 18.f, PaneY + 47.f, RaidUI::Muted, 0.78f);
    DrawBar(RightX + 18.f, PaneY + 76.f, PaneWidth - 36.f, 4.f,
        static_cast<float>(InventoryItemCount) / FMath::Max(1, InventoryCapacity),
        bFull ? RaidUI::Amber : RaidUI::Green);
    DrawFittedLabel(bFull ? TEXT("背包已满：放回或使用 1 件后可继续拿取。") :
        TEXT("6 kg 起减速 · 10 kg 禁冲刺 · 12 kg 上限"),
        RightX + 18.f, PaneY + 91.f, PaneWidth - 36.f,
        bFull ? RaidUI::Amber : RaidUI::Muted, 0.78f);

    if (Loot)
    {
        const bool bSearching = bViewSearching;
        const bool bUnsearched = UnsearchedRows > 0;
        DrawInventoryButton(TEXT("ContainerSearch"), bSearching ? TEXT("暂停搜索") :
            (bUnsearched ? TEXT("继续搜索") : TEXT("搜索完成")),
            X + 18.f, PaneY + 77.f, 112.f, 32.f, bSearching || bUnsearched, true);
        RegisterInventoryCommand(TEXT("ContainerSearch"), bSearching ? ERaidInventoryCommand::CancelSearch : ERaidInventoryCommand::BeginSearch);
        const FString SearchStatus = bSearching ? FString::Printf(TEXT("自动搜索 %02d  ·  %d%%"), SearchRowIndex + 1,
            FMath::Clamp(FMath::FloorToInt(SearchProgress * 100.f), 0, 100)) :
            (bUnsearched ? TEXT("已暂停 · 已揭示物资可拿取") :
                (ContainerRows.IsEmpty() ? TEXT("容器已空 · 可放回物资") : TEXT("已全部揭示 · 手动拿取")));
        DrawFittedLabel(SearchStatus, X + 145.f, PaneY + 79.f, PaneWidth - 164.f,
            bSearching ? RaidUI::Amber : RaidUI::Muted, 0.78f);
        if (bSearching)
        {
            DrawBar(X + 145.f, PaneY + 104.f, PaneWidth - 163.f, 3.f,
                SearchProgress, RaidUI::Amber);
        }
    }
    else
    {
        DrawLabel(TEXT("靠近物资箱或敌人掉落物，按 [E] 打开。"),
            X + 18.f, PaneY + 83.f, RaidUI::Muted, 0.82f);
    }
    DrawUILine(X + 18.f, RowsY - 8.f, X + PaneWidth - 18.f, RowsY - 8.f, RaidUI::Border);
    DrawUILine(RightX + 18.f, RowsY - 8.f, RightX + PaneWidth - 18.f, RowsY - 8.f, RaidUI::Border);

    const int32 ContainerPages = FMath::Max(1, (ContainerRows.Num() + RowsPerPage - 1) / RowsPerPage);
    const int32 InventoryPages = FMath::Max(1, (InventoryView.Num() + RowsPerPage - 1) / RowsPerPage);
    ContainerPage = FMath::Clamp(ContainerPage, 0, ContainerPages - 1);
    InventoryPage = FMath::Clamp(InventoryPage, 0, InventoryPages - 1);
    for (int32 Row = 0; Row < RowsPerPage; ++Row)
    {
        const float RowY = RowsY + Row * RowHeight;
        const int32 VisibleIndex = ContainerPage * RowsPerPage + Row;
        if (Loot && ContainerRows.IsValidIndex(VisibleIndex))
        {
            const int32 Index = ContainerRows[VisibleIndex];
            const FRaidInventoryEntry& Entry = ContainerView[Index];
            const FName TakeAction(*FString::Printf(TEXT("Take_%d"), Index));
            const bool bCurrent = !Entry.bRevealed && bViewSearching && SearchEntryId == Entry.EntryId;
            const bool bTaken = Entry.Quantity <= 0;
            const bool bHovered = HoveredButton == TakeAction;
            const bool bRecent = !bLastFeedbackInventoryOnly && FeedbackAlpha > 0.f &&
                Entry.bRevealed && Entry.Definition.ItemId == LastInventoryFeedbackItemId &&
                Entry.Definition.DisplayName == LastInventoryFeedbackLabel && Entry.Definition.UnitValue == LastInventoryFeedbackUnitValue;
            const FLinearColor Accent = bCurrent ? RaidUI::Amber : bRecent ? RaidUI::Green : RaidUI::Border;
            const FLinearColor RowFill = bCurrent ? FLinearColor(0.24f, 0.18f, 0.07f, 0.32f) :
                bHovered || bRecent ? FLinearColor(0.18f, 0.3f, 0.19f, bHovered ? 0.36f : FeedbackAlpha * 0.3f) :
                FLinearColor(0.3f, 0.38f, 0.32f, Row % 2 == 0 ? 0.09f : 0.035f);
            DrawRect(RowFill, (X + 12.f) * UIScale, RowY * UIScale,
                (PaneWidth - 24.f) * UIScale, RowHeight * UIScale);
            DrawUILine(X + 13.f, RowY + 5.f, X + 13.f, RowY + 45.f, Accent,
                bCurrent || bHovered || bRecent ? 3.f : 1.f);
            DrawLabel(FString::Printf(TEXT("%02d"), Index + 1), X + 25.f, RowY + 6.f,
                bCurrent ? RaidUI::Amber : RaidUI::Muted, 0.73f);
            const bool bMedkit = Entry.bRevealed && Entry.Definition.UseEffect == ERaidItemUseEffect::Heal;
            const bool bAmmo = Entry.bRevealed && Entry.Definition.UseEffect == ERaidItemUseEffect::Ammo;
            DrawLabel(!Entry.bRevealed ? TEXT("未知") : bMedkit ? TEXT("医疗") : bAmmo ? TEXT("弹药") : TEXT("物资"),
                X + 23.f, RowY + 31.f, bTaken || !Entry.bRevealed ? RaidUI::Muted :
                    bMedkit ? RaidUI::Green : bAmmo ? RaidUI::Amber : RaidUI::Muted, 0.66f);
            if (!Entry.bRevealed)
            {
                // Unknown rows never reveal identity, quantity, value or rarity.
                DrawLabel(bCurrent ? TEXT("搜索中…") : TEXT("未搜索物资"),
                    X + 65.f, RowY + 4.f, bCurrent ? RaidUI::Amber : RaidUI::Muted, ItemNameScale);
                DrawLabel(bCurrent ? TEXT("正在辨认物资") : TEXT("搜索后揭示"),
                    X + 65.f, RowY + 31.f, RaidUI::Muted, ItemDetailScale);
                if (bCurrent)
                {
                    DrawRightLabel(FString::Printf(TEXT("%d%%"),
                        FMath::Clamp(FMath::FloorToInt(SearchProgress * 100.f), 0, 100)),
                        X + PaneWidth - 21.f, RowY + 7.f, RaidUI::Amber, 0.8f);
                    DrawBar(X + PaneWidth - 112.f, RowY + 34.f, 94.f, 3.f,
                        SearchProgress, RaidUI::Amber);
                }
                else
                {
                    DrawRightLabel(TEXT("未搜"), X + PaneWidth - 24.f, RowY + 15.f, RaidUI::Muted, 0.8f);
                }
            }
            else
            {
                DrawFittedLabel(Entry.Definition.DisplayName, X + 65.f, RowY + 4.f,
                    PaneWidth - 193.f, bTaken ? RaidUI::Muted : RaidUI::White, ItemNameScale);
                DrawLabel(bTaken ? TEXT("已拿走 · 0 件") : FString::Printf(TEXT("×%d · $%d · %.1f kg/件"), Entry.Quantity, Entry.Definition.UnitValue, Entry.Definition.UnitWeightGrams / 1000.f),
                    X + 65.f, RowY + 31.f, bRecent ? RaidUI::Green : RaidUI::Muted, ItemDetailScale);
                if (bTaken)
                {
                    DrawRightLabel(TEXT("已取空"), X + PaneWidth - 24.f, RowY + 15.f,
                        bRecent ? RaidUI::Green : RaidUI::Muted, 0.8f);
                }
                else
                {
                    const bool bFits = !bFull && Player->GetInventoryComponent()->GetWeightGrams() + Entry.Definition.UnitWeightGrams <= Player->GetInventoryComponent()->GetWeightCapacityGrams();
                    DrawInventoryButton(TakeAction, !bFits ? TEXT("容量不足") : TEXT("拿取 1"),
                        X + PaneWidth - 112.f, RowY + 9.f, 94.f, 32.f, bFits, true);
                    RegisterInventoryCommand(TakeAction, ERaidInventoryCommand::Take, &Entry);
                }
            }
        }
        const int32 ItemIndex = InventoryPage * RowsPerPage + Row;
        if (InventoryView.IsValidIndex(ItemIndex))
        {
            const FRaidInventoryEntry& Item = InventoryView[ItemIndex];
            const bool bMedkit = Item.Definition.UseEffect == ERaidItemUseEffect::Heal;
            const bool bAmmo = Item.Definition.UseEffect == ERaidItemUseEffect::Ammo;
            const FName StoreAction(*FString::Printf(TEXT("Store_%d"), ItemIndex));
            const FName UseAction(*FString::Printf(TEXT("Use_%d"), ItemIndex));
            const bool bHovered = HoveredButton == StoreAction || HoveredButton == UseAction;
            const bool bRecent = FeedbackAlpha > 0.f && Item.Definition.ItemId == LastInventoryFeedbackItemId &&
                Item.Definition.DisplayName == LastInventoryFeedbackLabel && Item.Definition.UnitValue == LastInventoryFeedbackUnitValue;
            const FLinearColor RowFill = bHovered || bRecent ?
                FLinearColor(0.18f, 0.3f, 0.19f, bHovered ? 0.36f : FeedbackAlpha * 0.3f) :
                FLinearColor(0.3f, 0.38f, 0.32f, Row % 2 == 0 ? 0.09f : 0.035f);
            DrawRect(RowFill, (RightX + 12.f) * UIScale, RowY * UIScale,
                (PaneWidth - 24.f) * UIScale, RowHeight * UIScale);
            DrawUILine(RightX + 13.f, RowY + 5.f, RightX + 13.f, RowY + 45.f,
                bHovered || bRecent ? RaidUI::Green : RaidUI::Border, bHovered || bRecent ? 3.f : 1.f);
            DrawLabel(FString::Printf(TEXT("%02d"), ItemIndex + 1), RightX + 25.f, RowY + 6.f, RaidUI::Muted, 0.73f);
            DrawLabel(bMedkit ? TEXT("医疗") : bAmmo ? TEXT("弹药") : TEXT("物资"),
                RightX + 23.f, RowY + 31.f, bMedkit ? RaidUI::Green : bAmmo ? RaidUI::Amber : RaidUI::Muted, 0.66f);
            DrawFittedLabel(Item.Definition.DisplayName, RightX + 65.f, RowY + 4.f, 248.f, RaidUI::White, ItemNameScale);
            const FString Effect = bMedkit ? FString::Printf(TEXT(" · 3秒 +%d"), Item.Definition.UseAmount) :
                (bAmmo ? FString::Printf(TEXT(" · 备用 +%d"), Item.Definition.UseAmount) : TEXT(""));
            DrawFittedLabel(FString::Printf(TEXT("×%d · $%lld · %.1fkg"), Item.Quantity,
                static_cast<int64>(Item.Definition.UnitValue) * Item.Quantity, Item.Quantity * Item.Definition.UnitWeightGrams / 1000.f) + Effect, RightX + 65.f,
                RowY + 31.f, 251.f, bRecent ? RaidUI::Green : RaidUI::Muted, ItemDetailScale);
            if (Loot)
            {
                DrawInventoryButton(StoreAction, TEXT("放回 1"), RightX + 322.f,
                    RowY + 9.f, 80.f, 32.f, Item.Quantity > 0);
                RegisterInventoryCommand(StoreAction, ERaidInventoryCommand::Store, &Item);
            }
            if (bMedkit || bAmmo)
            {
                const bool bCanUse = Item.Quantity > 0 && (!bMedkit || Player->Health < 100.f);
                DrawInventoryButton(UseAction,
                    bAmmo ? TEXT("补充弹药") : (bCanUse ? TEXT("使用 1") : TEXT("生命已满")),
                    RightX + 412.f, RowY + 9.f, 112.f, 32.f, bCanUse);
                RegisterInventoryCommand(UseAction, ERaidInventoryCommand::Use, &Item);
            }
        }
    }
    if (ContainerRows.IsEmpty())
    {
        DrawLabel(Loot ? TEXT("容器已空") : TEXT("未连接来源容器"),
            X + PaneWidth * 0.5f, RowsY + 92.f, RaidUI::White, 1.1f, true);
        DrawLabel(Loot ? TEXT("点击背包中的“放回 1”，把物资留在这里。") :
            TEXT("关闭背包，再用 [E] 打开附近的容器。"),
            X + PaneWidth * 0.5f, RowsY + 127.f, RaidUI::Muted, 0.82f, true);
    }
    if (InventoryView.IsEmpty())
    {
        DrawLabel(TEXT("背包为空"), RightX + PaneWidth * 0.5f,
            RowsY + 92.f, RaidUI::White, 1.1f, true);
        DrawLabel(TEXT("搜索容器后，点击“拿取 1”放入背包。"),
            RightX + PaneWidth * 0.5f, RowsY + 127.f, RaidUI::Muted, 0.82f, true);
    }
    const float FooterY = RowsY + RowsPerPage * RowHeight + 10.f;
    DrawInventoryButton(TEXT("ContainerPrev"), TEXT("上一页"), X + 18.f, FooterY,
        82.f, 28.f, ContainerPage > 0);
    DrawInventoryButton(TEXT("ContainerNext"), TEXT("下一页"), X + PaneWidth - 100.f, FooterY,
        82.f, 28.f, ContainerPage + 1 < ContainerPages);
    DrawLabel(FString::Printf(TEXT("第 %d / %d 页"), ContainerPage + 1, ContainerPages),
        X + PaneWidth * 0.5f, FooterY + 5.f, RaidUI::Muted, 0.78f, true);
    DrawInventoryButton(TEXT("InventoryPrev"), TEXT("上一页"), RightX + 18.f, FooterY,
        82.f, 28.f, InventoryPage > 0);
    DrawInventoryButton(TEXT("InventoryNext"), TEXT("下一页"), RightX + PaneWidth - 100.f, FooterY,
        82.f, 28.f, InventoryPage + 1 < InventoryPages);
    DrawLabel(FString::Printf(TEXT("第 %d / %d 页"), InventoryPage + 1, InventoryPages),
        RightX + PaneWidth * 0.5f, FooterY + 5.f, RaidUI::Muted, 0.78f, true);

    const ARaidExtractionZone* Extraction = FindExtraction(Player);
    const bool bCountdownActive = Extraction && Extraction->IsPlayerInside() && InventoryItemCount > 0
        && (!MissionView.bPresent || Extraction->IsAvailable());
    const ARaidGameMode* GameMode = Cast<ARaidGameMode>(UGameplayStatics::GetGameMode(this));
    const FString Footer = bCountdownActive ? FString::Printf(
        TEXT("撤离剩余 %.1f 秒 · 离开区域或受伤会重置计时"),
        FMath::Max(0.f, Extraction->GetSecondsRemaining())) :
        (GameMode && !GameMode->Message.IsEmpty() ? GameMode->Message :
            TEXT("[E] 打开容器 · 搜索后逐件拿取 · 放回或使用可腾出背包空间"));
    DrawSoftBackdrop(X, Y + 555.f, Width, 32.f, 0.85f);
    DrawFittedLabel(Footer, X + 14.f, Y + 563.f, Width - 28.f,
        bCountdownActive ? RaidUI::Green : RaidUI::Amber, 0.8f);
}
void ARaidHUD::DrawResult(const ARaidGameMode* GameMode)
{
    const bool bSuccess = GameMode->State == ERaidState::Extracted;
    const ARaidPlayer* Player = GetOwningPlayerController()
        ? Cast<ARaidPlayer>(GetOwningPlayerController()->GetPawn()) : nullptr;
    const bool bAbandoned = !bSuccess && Player && Player->IsAlive();
    const FLinearColor ResultColor = bSuccess ? RaidUI::Green : RaidUI::Red;
    DrawRect(FLinearColor(0.012f, 0.02f, 0.015f, 0.88f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
    const float Width = 660.f;
    const float Height = 480.f;
    const float X = (ScreenWidth - Width) * 0.5f;
    const float Y = (ScreenHeight - Height) * 0.5f;
    const float CenterX = ScreenWidth * 0.5f;
    DrawPanel(X, Y, Width, Height, ResultColor);
    DrawLabel(bSuccess ? TEXT("撤离已确认") :
        (bAbandoned ? TEXT("行动已放弃") : TEXT("行动人员阵亡")),
        CenterX, Y + 22.f, ResultColor, 0.82f, true);
    DrawLabel(bSuccess ? TEXT("撤离成功") : TEXT("行动失败"),
        CenterX, Y + 44.f, RaidUI::White, 1.6f, true);
    DrawLabel(FString::Printf(TEXT("$%lld"), GameMode->SecuredValue),
        CenterX, Y + 98.f, ResultColor, 2.55f, true);
    DrawLabel(TEXT("带出物资价值"), CenterX, Y + 166.f, RaidUI::Muted, 0.82f, true);
    DrawUILine(X + 28.f, Y + 189.f, X + Width - 28.f, Y + 189.f, RaidUI::Border);

    const float StatXs[] = { CenterX - 202.f, CenterX, CenterX + 202.f };
    DrawLabel(TEXT("带出物资"), StatXs[0], Y + 207.f, RaidUI::Muted, 0.82f, true);
    DrawLabel(FString::FromInt(GameMode->SecuredItems), StatXs[0], Y + 232.f,
        ResultColor, 1.3f, true);
    DrawLabel(TEXT("击毙敌人"), StatXs[1], Y + 207.f, RaidUI::Muted, 0.82f, true);
    DrawLabel(FString::FromInt(GameMode->EnemiesKilled), StatXs[1], Y + 232.f,
        RaidUI::White, 1.3f, true);
    DrawLabel(TEXT("行动用时"), StatXs[2], Y + 207.f, RaidUI::Muted, 0.82f, true);
    DrawLabel(FormatTime(GameMode->RaidElapsed), StatXs[2], Y + 232.f,
        RaidUI::White, 1.3f, true);
    const FString Outcome = bSuccess ? TEXT("带出价值已计入本次结算；下局重新开始。") :
        (Player ? FString::Printf(TEXT("本次损失：%d 件物资  /  $%lld"),
            InventoryItemCount, InventoryTotalValue) : TEXT("本次携带的物资已全部丢失。"));
    DrawFittedLabel(Outcome, CenterX, Y + 279.f, Width - 56.f, RaidUI::Muted, 0.82f, true);
    if (MissionView.bPresent)
    {
        DrawLabel(MissionView.bCompleted ? TEXT("可选情报目标：已完成") : TEXT("可选情报目标：未完成"),
            CenterX, Y + 300.f, MissionView.bCompleted ? RaidUI::Green : RaidUI::Muted, 0.78f, true);
    }
    if (!bSuccess && !bAbandoned && Player && !Player->DeathCause.IsEmpty())
        DrawFittedLabel(Player->DeathCause, CenterX, Y + 327.f, Width - 56.f, RaidUI::Red, .84f, true);
    DrawMenuButton(TEXT("Restart"), TEXT("再次行动"), X + 28.f, Y + 357.f, 292.f, 60.f, true);
    DrawMenuButton(TEXT("Menu"), TEXT("返回主菜单"), X + 340.f, Y + 357.f, 292.f, 60.f);
    DrawFittedLabel(TEXT("[ENTER] 再次行动  /  重置生命、弹药、敌人与物资"),
        CenterX, Y + 441.f, Width - 56.f, RaidUI::Muted, 0.82f, true);
}

FVector2D ARaidHUD::MeasureLabel(const FString& Text, float FontScale) const
{
    const float DPIScale = Canvas->Canvas ? FMath::Max(0.01f, Canvas->Canvas->GetDPIScale()) : 1.f;
    RaidUI::FMeasuredTextItem Item(FVector2D::ZeroVector, FText::FromString(Text), HUDFont, RaidUI::White);
    RaidUI::ConfigureText(Item, HUDFont, FMath::Max(0.01f, FontScale), UIScale, DPIScale);
    return Item.Measure(DPIScale) / UIScale;
}

void ARaidHUD::DrawLabel(const FString& Text, float X, float Y, const FLinearColor& Color,
    float FontScale, bool bCentered)
{
    // Callers choose a readable size; fitting may intentionally choose a smaller
    // value. Do not reapply a minimum here or the fitted line can overflow.
    FontScale = FMath::Max(FontScale, 0.01f);
    if (bCentered)
    {
        X -= MeasureLabel(Text, FontScale).X * 0.5f;
    }
    const float DPIScale = Canvas->Canvas ? FMath::Max(0.01f, Canvas->Canvas->GetDPIScale()) : 1.f;
    // Canvas text internally multiplies its origin by DPI. Our layout and
    // hitboxes already use physical pixels, so compensate before passing it in.
    FCanvasTextItem Item(FVector2D(FMath::RoundToFloat(X * UIScale) / DPIScale,
        FMath::RoundToFloat(Y * UIScale) / DPIScale),
        FText::FromString(Text), HUDFont, Color);
    RaidUI::ConfigureText(Item, HUDFont, FontScale, UIScale, DPIScale);
    Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.8f * Color.A), FVector2D(1.f / DPIScale, 1.f / DPIScale));
    Canvas->DrawItem(Item);
}

void ARaidHUD::DrawRightLabel(const FString& Text, float X, float Y,
    const FLinearColor& Color, float FontScale)
{
    DrawLabel(Text, X - MeasureLabel(Text, FontScale).X, Y, Color, FontScale);
}

void ARaidHUD::DrawFittedLabel(const FString& Text, float X, float Y, float MaxWidth,
    const FLinearColor& Color, float FontScale, bool bCentered)
{
    if (MaxWidth <= 0.f || Text.IsEmpty())
    {
        return;
    }
    FontScale = FMath::Max(FontScale, 0.01f);
    FString VisibleText = Text;
    // Permit a small size adjustment first, then truncate unusually long item
    // names. Every decision uses the same rounded runtime size as DrawLabel.
    const float MinimumScale = FMath::Min(FontScale, 0.78f);
    while (MeasureLabel(VisibleText, FontScale).X > MaxWidth && FontScale > MinimumScale)
    {
        FontScale = FMath::Max(MinimumScale, FontScale - 0.025f);
    }
    if (MeasureLabel(VisibleText, FontScale).X > MaxWidth)
    {
        const FString Ellipsis(TEXT("…"));
        while (!VisibleText.IsEmpty() && MeasureLabel(VisibleText + Ellipsis, FontScale).X > MaxWidth)
        {
            VisibleText.LeftChopInline(1);
            // Do not leave half of a UTF-16 supplementary glyph at the boundary.
            if (!VisibleText.IsEmpty() && VisibleText[VisibleText.Len() - 1] >= 0xD800 &&
                VisibleText[VisibleText.Len() - 1] <= 0xDBFF)
            {
                VisibleText.LeftChopInline(1);
            }
        }
        if (MeasureLabel(Ellipsis, FontScale).X > MaxWidth)
        {
            return;
        }
        VisibleText += Ellipsis;
    }
    DrawLabel(VisibleText, X, Y, Color, FontScale, bCentered);
}

void ARaidHUD::DrawSoftBackdrop(float X, float Y, float Width, float Height, float Opacity)
{
    DrawRect(FLinearColor(0.01f, 0.018f, 0.014f, Opacity * 0.82f),
        X * UIScale, Y * UIScale, Width * UIScale, Height * UIScale);
}

void ARaidHUD::DrawPanel(float X, float Y, float Width, float Height, const FLinearColor& BorderColor)
{
    DrawRect(FLinearColor(0.025f, 0.035f, 0.027f, 0.94f),
        X * UIScale, Y * UIScale, Width * UIScale, Height * UIScale);
    DrawUILine(X, Y, X + Width, Y, BorderColor, 2.f);
    DrawUILine(X, Y + Height, X + Width, Y + Height, RaidUI::Border);
    DrawUILine(X, Y, X, Y + Height, RaidUI::Border);
    DrawUILine(X + Width, Y, X + Width, Y + Height, RaidUI::Border);
}

void ARaidHUD::DrawBar(float X, float Y, float Width, float Height, float Fraction,
    const FLinearColor& Color)
{
    DrawRect(FLinearColor(0.1f, 0.16f, 0.19f, 1.f),
        X * UIScale, Y * UIScale, Width * UIScale, Height * UIScale);
    const float FilledWidth = Width * FMath::Clamp(Fraction, 0.f, 1.f);
    if (FilledWidth > 0.f)
    {
        DrawRect(Color, X * UIScale, Y * UIScale, FilledWidth * UIScale, Height * UIScale);
    }
}

void ARaidHUD::DrawUILine(float X1, float Y1, float X2, float Y2,
    const FLinearColor& Color, float Thickness)
{
    DrawLine(X1 * UIScale, Y1 * UIScale, X2 * UIScale, Y2 * UIScale, Color,
        Thickness * UIScale);
}

ARaidExtractionZone* ARaidHUD::FindExtraction(const ARaidPlayer* Player) const
{
    ARaidExtractionZone* Closest = nullptr;
    ARaidExtractionZone* ClosestAvailable = nullptr;
    float ClosestDistanceSquared = TNumericLimits<float>::Max();
    float ClosestAvailableDistanceSquared = TNumericLimits<float>::Max();
    for (TActorIterator<ARaidExtractionZone> It(GetWorld()); It; ++It)
    {
        // A locked exit still owns its local feedback when standing inside it.
        // Otherwise guide Frontier players to an exit they can actually use.
        if (MissionView.bPresent && It->IsPlayerInside()) return *It;
        const float DistanceSquared = FVector::DistSquared(Player->GetActorLocation(), It->GetActorLocation());
        if (DistanceSquared < ClosestDistanceSquared)
        {
            Closest = *It;
            ClosestDistanceSquared = DistanceSquared;
        }
        if (MissionView.bPresent && It->IsAvailable() && DistanceSquared < ClosestAvailableDistanceSquared)
        {
            ClosestAvailable = *It;
            ClosestAvailableDistanceSquared = DistanceSquared;
        }
    }
    return ClosestAvailable ? ClosestAvailable : Closest;
}

int32 ARaidHUD::CountMedkits(const ARaidPlayer* Player) const
{
    return Player && ObservedPlayer.Get() == Player ? InventoryMedkitCount : 0;
}

FString ARaidHUD::FormatTime(float Seconds) const
{
    const int32 WholeSeconds = FMath::Max(0, FMath::FloorToInt(Seconds));
    return FString::Printf(TEXT("%02d:%02d"), WholeSeconds / 60, WholeSeconds % 60);
}
