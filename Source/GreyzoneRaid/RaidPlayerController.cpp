#include "RaidPlayerController.h"

#include "RaidGameMode.h"
#include "RaidHUD.h"
#include "RaidInteractionComponent.h"
#include "RaidInventoryComponent.h"
#include "RaidLoot.h"
#include "RaidPlayer.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Templates/UnrealTemplate.h"

void ARaidPlayerController::BeginPlay()
{
    Super::BeginPlay();
    ObservePawn(Cast<ARaidPlayer>(GetPawn()));
    RefreshInputMode();
}

void ARaidPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bEndingPlay = true;
    ReleasePawn(true);
    Super::EndPlay(EndPlayReason);
}

void ARaidPlayerController::OnPossess(APawn* InPawn)
{
    // Super can unpossess the previous pawn. Its cleanup must finish first.
    Super::OnPossess(InPawn);
    ObservePawn(Cast<ARaidPlayer>(GetPawn()));
    bHasAppliedInputMode = false;
    RefreshInputMode();
}

void ARaidPlayerController::OnUnPossess()
{
    ReleasePawn(true);
    Super::OnUnPossess();
    bHasAppliedInputMode = false;
    RefreshInputMode();
}

void ARaidPlayerController::ObservePawn(ARaidPlayer* RaidPawn)
{
    if (ObservedPlayer.Get() != RaidPawn)
    {
        ReleasePawn(false);
        ObservedPlayer = RaidPawn;
        URaidInteractionComponent* Interaction = RaidPawn ? RaidPawn->GetInteractionComponent() : nullptr;
        ObservedInteraction = Interaction;
        if (Interaction)
        {
            InteractionChangedHandle = Interaction->OnInteractionChanged.AddUObject(
                this, &ARaidPlayerController::HandleInteractionChanged);
            InteractionEndingHandle = Interaction->OnInteractionEndingPlay.AddUObject(
                this, &ARaidPlayerController::HandleInteractionEndingPlay);
        }
    }
    // HUD creation can follow possession, so BeginPlay repeats this safe handoff.
    if (ARaidHUD* RaidHUD = Cast<ARaidHUD>(GetHUD())) RaidHUD->SetObservedPlayer(RaidPawn);
}

void ARaidPlayerController::ReleasePawn(bool bCleanup)
{
    TGuardValue<bool> ReleasingGuard(bReleasingPawn, true);
    ARaidPlayer* Previous = ObservedPlayer.Get();
    if (ARaidHUD* RaidHUD = Cast<ARaidHUD>(GetHUD())) RaidHUD->SetObservedPlayer(nullptr);
    if (URaidInteractionComponent* Interaction = ObservedInteraction.Get())
    {
        Interaction->OnInteractionChanged.Remove(InteractionChangedHandle);
        Interaction->OnInteractionEndingPlay.Remove(InteractionEndingHandle);
    }
    InteractionChangedHandle.Reset();
    InteractionEndingHandle.Reset();
    ObservedInteraction.Reset();
    ObservedPlayer.Reset();
    if (bCleanup && IsValid(Previous)) Previous->StopInteractionForUnpossess();
}

void ARaidPlayerController::HandleInteractionChanged(URaidInteractionComponent* Interaction)
{
    if (ObservedInteraction.Get() == Interaction) RefreshInputMode();
}

void ARaidPlayerController::HandleInteractionEndingPlay(URaidInteractionComponent* Interaction)
{
    if (ObservedInteraction.Get() != Interaction) return;
    ReleasePawn(false);
    RefreshInputMode();
}

void ARaidPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    check(InputComponent);
    InputComponent->BindAction(TEXT("Interact"), IE_Pressed, this, &ARaidPlayerController::HandleInteract);
    InputComponent->BindAction(TEXT("Interact"), IE_Released, this, &ARaidPlayerController::HandleReleaseInteract);
    InputComponent->BindAction(TEXT("Inventory"), IE_Pressed, this, &ARaidPlayerController::HandleToggleInventory);
    InputComponent->BindKey(EKeys::M, IE_Pressed, this, &ARaidPlayerController::HandleToggleTacticalMap);
    InputComponent->BindAction(TEXT("Heal"), IE_Pressed, this, &ARaidPlayerController::HandleUseMedkit);
    InputComponent->BindAction(TEXT("Restart"), IE_Pressed, this, &ARaidPlayerController::HandleConfirm).bExecuteWhenPaused = true;
    InputComponent->BindAction(TEXT("Quit"), IE_Pressed, this, &ARaidPlayerController::HandleBack).bExecuteWhenPaused = true;
}

void ARaidPlayerController::RefreshInputMode()
{
    if (bEndingPlay || bReleasingPawn || !IsLocalController()) return;
    const ARaidGameMode* Mode = Cast<ARaidGameMode>(UGameplayStatics::GetGameMode(this));
    const ARaidPlayer* RaidPawn = Cast<ARaidPlayer>(GetPawn());
    const URaidInteractionComponent* Interaction = RaidPawn ? RaidPawn->GetInteractionComponent() : nullptr;
    const bool bUseUI = !Mode || !Mode->IsRaidActive() || Mode->IsPauseMenuOpen() || !RaidPawn || !RaidPawn->CanAct()
        || ObservedPlayer.Get() != RaidPawn || ObservedInteraction.Get() != Interaction
        || !IsValid(Interaction) || Interaction->IsInventoryOpen();
    // Inventory/search events may arrive often; flushing held keys is a mode
    // transition, not a side effect of each inventory notification.
    if (bHasAppliedInputMode && bAppliedUIInput == bUseUI) return;
    ResetIgnoreMoveInput();
    ResetIgnoreLookInput();
    SetIgnoreMoveInput(bUseUI);
    SetIgnoreLookInput(bUseUI);
    bShowMouseCursor = bUseUI;
    bEnableClickEvents = bUseUI;
    bEnableMouseOverEvents = bUseUI;
    if (bUseUI)
    {
        FInputModeGameAndUI InputMode;
        InputMode.SetHideCursorDuringCapture(false);
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        SetInputMode(InputMode);
    }
    else SetInputMode(FInputModeGameOnly());
    bAppliedUIInput = bUseUI;
    bHasAppliedInputMode = true;
    FlushPressedKeys();
}

void ARaidPlayerController::HandleMenuAction(FName Action)
{
    if (bEndingPlay || bReleasingPawn) return;
    if (ARaidGameMode* Mode = Cast<ARaidGameMode>(UGameplayStatics::GetGameMode(this))) Mode->HandleMenuAction(Action);
}

void ARaidPlayerController::HandleBack()
{
    if (bEndingPlay || bReleasingPawn) return;
    if (ARaidHUD* RaidHUD = Cast<ARaidHUD>(GetHUD()); RaidHUD && RaidHUD->IsTacticalMapOpen())
    {
        RaidHUD->CloseTacticalMap();
        return;
    }
    if (ARaidGameMode* Mode = Cast<ARaidGameMode>(UGameplayStatics::GetGameMode(this))) Mode->HandleBack();
}

void ARaidPlayerController::HandleConfirm()
{
    if (bEndingPlay || bReleasingPawn) return;
    if (ARaidGameMode* Mode = Cast<ARaidGameMode>(UGameplayStatics::GetGameMode(this)))
    {
        if (Mode->IsPauseMenuOpen()) Mode->ResumeRaid();
        else if (Mode->State == ERaidState::MainMenu) Mode->StartRaid();
        else Mode->RestartRaid();
    }
}

void ARaidPlayerController::HandleInteract()
{
    if (URaidInteractionComponent* Interaction = GetCommandInteraction())
    {
        ReturnToWorldView();
        Interaction->Interact();
    }
}

void ARaidPlayerController::HandleReleaseInteract()
{
    // A release also cleans up a hold when CanAct changed after its press.
    if (URaidInteractionComponent* Interaction = ObservedInteraction.Get()) Interaction->ReleaseInteract();
}

void ARaidPlayerController::HandleToggleInventory()
{
    if (URaidInteractionComponent* Interaction = GetCommandInteraction())
    {
        if (ARaidHUD* RaidHUD = Cast<ARaidHUD>(GetHUD())) RaidHUD->CloseTacticalMap();
        Interaction->ToggleInventory();
    }
}

void ARaidPlayerController::HandleToggleTacticalMap()
{
    if (bEndingPlay || bReleasingPawn) return;
    if (ARaidHUD* RaidHUD = Cast<ARaidHUD>(GetHUD())) RaidHUD->ToggleTacticalMap();
}

void ARaidPlayerController::HandleUseMedkit()
{
    if (URaidInteractionComponent* Interaction = GetCommandInteraction()) Interaction->UseMedkit();
}

void ARaidPlayerController::PrepareTacticalMapInput()
{
    // Opening the opaque map must not keep spending ammunition out of view.
    // Movement and the world continue; a fresh combat press returns to the view.
    if (ARaidPlayer* RaidPawn=ObservedPlayer.Get(); RaidPawn && RaidPawn==GetPawn() && RaidPawn->CanAct())
    {
        RaidPawn->StopFire();
        RaidPawn->StopAim();
    }
}

void ARaidPlayerController::ReturnToWorldView()
{
    if (ARaidHUD* RaidHUD=Cast<ARaidHUD>(GetHUD())) RaidHUD->CloseTacticalMap();
}

URaidInteractionComponent* ARaidPlayerController::GetCommandInteraction() const
{
    if (bEndingPlay || bReleasingPawn) return nullptr;
    const ARaidPlayer* RaidPawn = Cast<ARaidPlayer>(GetPawn());
    if (!IsValid(RaidPawn) || ObservedPlayer.Get() != RaidPawn || !RaidPawn->CanAct()) return nullptr;
    URaidInteractionComponent* Interaction = RaidPawn->GetInteractionComponent();
    return IsValid(Interaction) && ObservedInteraction.Get() == Interaction ? Interaction : nullptr;
}

bool ARaidPlayerController::IsInventoryContextCurrent(const FRaidInventoryCommandContext& Context) const
{
    const ARaidGameMode* Mode = Cast<ARaidGameMode>(UGameplayStatics::GetGameMode(this));
    const ARaidPlayer* RaidPawn = Cast<ARaidPlayer>(GetPawn());
    if (bEndingPlay || bReleasingPawn || !Mode || !Mode->IsRaidActive() ||
        !IsValid(RaidPawn) || !RaidPawn->CanAct() || ObservedPlayer.Get() != RaidPawn ||
        Context.Player.Get() != RaidPawn) return false;
    const URaidInteractionComponent* Interaction = RaidPawn->GetInteractionComponent();
    const URaidInventoryComponent* Backpack = RaidPawn->GetInventoryComponent();
    if (!IsValid(Interaction) || !IsValid(Backpack) || ObservedInteraction.Get() != Interaction ||
        !Interaction->IsInventoryOpen() ||
        Context.Backpack.Get() != Backpack || Context.BackpackRevision != Backpack->GetRevision() ||
        Context.SessionRevision != Interaction->GetSessionRevision()) return false;
    const ARaidLoot* Container = Interaction->GetOpenLoot();
    if (Context.bHadContainer != (Container != nullptr) || Context.Container.Get() != Container) return false;
    if (!Container) return !Context.ContainerInventory.IsValid();
    const URaidInventoryComponent* Source = Container->GetInventoryComponent();
    return IsValid(Source) && Context.ContainerInventory.Get() == Source &&
        Context.ContainerRevision == Source->GetRevision();
}

ERaidInventoryResultCode ARaidPlayerController::SubmitInventoryCommand(ERaidInventoryCommand Command,
    int32 EntryId, const FRaidInventoryCommandContext& Context)
{
    if (!IsInventoryContextCurrent(Context)) return ERaidInventoryResultCode::StaleRevision;
    URaidInteractionComponent* Interaction = CastChecked<ARaidPlayer>(GetPawn())->GetInteractionComponent();
    switch (Command)
    {
    case ERaidInventoryCommand::Close:
        Interaction->Close();
        return ERaidInventoryResultCode::Success;
    case ERaidInventoryCommand::BeginSearch:
    {
        const TWeakObjectPtr<URaidInteractionComponent> RequestedInteraction = Interaction;
        Interaction->BeginSearch();
        // A listener can end the component or change possession synchronously.
        return RequestedInteraction.IsValid() && ObservedInteraction == RequestedInteraction &&
            ObservedPlayer == Context.Player && RequestedInteraction->GetOpenLoot() == Context.Container.Get() &&
            RequestedInteraction->IsSearching()
            ? ERaidInventoryResultCode::Success : ERaidInventoryResultCode::InvalidState;
    }
    case ERaidInventoryCommand::CancelSearch:
        Interaction->CancelSearch();
        return ERaidInventoryResultCode::Success;
    case ERaidInventoryCommand::Take:
        return Interaction->TakeEntry(EntryId, Context.ContainerRevision, Context.BackpackRevision).Code;
    case ERaidInventoryCommand::Store:
        return Interaction->StoreEntry(EntryId, Context.BackpackRevision, Context.ContainerRevision).Code;
    case ERaidInventoryCommand::Use:
        return Interaction->UseEntry(EntryId, Context.BackpackRevision).Code;
    default:
        return ERaidInventoryResultCode::InvalidState;
    }
}
