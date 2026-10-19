#include "RaidFrameworkTestDirector.h"
#include "RaidEnemy.h"
#include "RaidGameMode.h"
#include "RaidHUD.h"
#include "RaidInventoryComponent.h"
#include "RaidInteractionComponent.h"
#include "RaidInventoryModel.h"
#include "RaidItemCatalog.h"
#include "RaidLoot.h"
#include "RaidPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
    using ECode = ERaidInventoryResultCode;

    FRaidItemDefinition Definition(FName Id, const TCHAR* Label, int32 Value,
        ERaidItemUseEffect Effect = ERaidItemUseEffect::None, int32 Amount = 0)
    {
        FRaidItemDefinition Out;
        Out.ItemId = Id; Out.DisplayName = Label; Out.UnitValue = Value;
        Out.UseEffect = Effect; Out.UseAmount = Amount;
        return Out;
    }

    FRaidInventorySeed Seed(const FRaidItemDefinition& Item, int32 Quantity, bool Revealed = true)
    {
        FRaidInventorySeed Out;
        Out.Definition = Item; Out.Quantity = Quantity; Out.bRevealed = Revealed;
        return Out;
    }

    FRaidTransferRequest Request(const FRaidInventoryState& From, const FRaidInventoryState& To,
        int32 EntryId, int32 Quantity, ERaidTransferPolicy Policy = ERaidTransferPolicy::Exact)
    {
        FRaidTransferRequest Out;
        Out.SourceEntryId = EntryId; Out.Quantity = Quantity; Out.Policy = Policy;
        Out.ExpectedSourceRevision = From.Revision; Out.ExpectedDestinationRevision = To.Revision;
        return Out;
    }

    // Structural snapshot comparison checks every stored field, independently
    // of the model's cached queries or transaction implementation.
    bool SameState(const FRaidInventoryState& A, const FRaidInventoryState& B)
    {
        if (A.Revision != B.Revision || A.NextEntryId != B.NextEntryId || A.Capacity != B.Capacity
            || A.bKeepEmptyRows != B.bKeepEmptyRows || A.bInitialized != B.bInitialized
            || A.Entries.Num() != B.Entries.Num()) return false;
        for (int32 Index = 0; Index < A.Entries.Num(); ++Index)
        {
            const FRaidInventoryEntry& X = A.Entries[Index];
            const FRaidInventoryEntry& Y = B.Entries[Index];
            if (X.EntryId != Y.EntryId || X.Quantity != Y.Quantity || X.bRevealed != Y.bRevealed
                || X.Definition.ItemId != Y.Definition.ItemId || X.Definition.DisplayName != Y.Definition.DisplayName
                || X.Definition.UnitValue != Y.Definition.UnitValue || X.Definition.UseEffect != Y.Definition.UseEffect
                || X.Definition.UseAmount != Y.Definition.UseAmount) return false;
        }
        return true;
    }
}

ARaidFrameworkTestDirector::ARaidFrameworkTestDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;
}

void ARaidFrameworkTestDirector::BeginPlay()
{
    Super::BeginPlay();
    if (!FParse::Param(FCommandLine::Get(), TEXT("RaidFrameworkTest"))) { Destroy(); return; }
    StartedAt = FPlatformTime::Seconds();
    UE_LOG(LogTemp, Display, TEXT("RAID_FRAMEWORK: kernel transactions, registered components and real pawn/container lifecycle; no hardware input injection."));
}

bool ARaidFrameworkTestDirector::Check(bool Passed, const TCHAR* Name, const FString& Detail)
{
    ++Checks;
    if (!Passed) ++Failures;
    UE_LOG(LogTemp, Display, TEXT("RAID_FRAMEWORK_CHECK:%s %s | %s"), Passed ? TEXT("PASS") : TEXT("FAIL"), Name, *Detail);
    return Passed;
}

void ARaidFrameworkTestDirector::Abort(const TCHAR* Name, const FString& Detail)
{
    Check(false, Name, Detail);
    Finish();
}

void ARaidFrameworkTestDirector::Finish()
{
    if (bFinished) return;
    bFinished = true;
    UE_LOG(LogTemp, Display, TEXT("RAID_FRAMEWORK_RESULT: %s checks=%d failures=%d elapsed=%.2fs"),
        Failures == 0 ? TEXT("PASS") : TEXT("FAIL"), Checks, Failures, FPlatformTime::Seconds() - StartedAt);
    FPlatformMisc::RequestExitWithStatus(false, Failures == 0 ? 0 : 1);
}

void ARaidFrameworkTestDirector::Advance(EStep Next)
{
    Step = Next;
    StepSeconds = 0.f;
}

bool ARaidFrameworkTestDirector::RunKernelCases()
{
    UE_LOG(LogTemp, Display, TEXT("RAID_FRAMEWORK_PHASE: kernel"));
    const FRaidItemDefinition Tech = Definition(TEXT("Tech"), TEXT("Probe tech"), 120);
    const FRaidItemDefinition Med = Definition(TEXT("Medkit"), TEXT("Probe medical"), 75, ERaidItemUseEffect::Heal, 45);
    FRaidInventoryState Source, Destination;
    if (!Check(FRaidInventoryModel::InitializeOnce(Source, {Seed(Tech, 5), Seed(Med, 0, false)}, -1, true).IsSuccess()
        && FRaidInventoryModel::InitializeOnce(Destination, {Seed(Med, 2)}, 2, false).IsSuccess(),
        TEXT("kernel_fixtures_initialize_once_with_real_capacity_and_known_source"))) return false;
    Check(Source.Entries.Num() == 2 && Source.Entries[0].EntryId != Source.Entries[1].EntryId
        && Source.Entries[1].Quantity == 0 && !Source.Entries[1].bRevealed,
        TEXT("container_initialization_preserves_stable_empty_authored_row"));
    const int32 TechId = Source.Entries[0].EntryId;
    FRaidInventoryState BeforeSource = Source, BeforeDestination = Destination;
    Check(FRaidInventoryModel::InitializeOnce(Source, {}, 0, false).Code == ECode::AlreadyInitialized
        && SameState(Source, BeforeSource), TEXT("second_initialization_cannot_reset_live_inventory"));
    Check(FRaidInventoryModel::Transfer(Source, Destination, Request(Source, Destination, TechId, 2)).Code == ECode::InsufficientCapacity
        && SameState(Source, BeforeSource) && SameState(Destination, BeforeDestination),
        TEXT("exact_transfer_into_full_backpack_leaves_every_field_unchanged"));
    Check(FRaidInventoryModel::Transfer(Source, Destination, Request(Source, Destination, TechId, 6)).Code == ECode::InsufficientQuantity
        && SameState(Source, BeforeSource) && SameState(Destination, BeforeDestination),
        TEXT("exact_transfer_cannot_partially_satisfy_insufficient_source"));

    FRaidInventoryModel::Consume(Destination, Destination.Entries[0].EntryId, 1, Destination.Revision);
    const int32 TotalBefore = FRaidInventoryModel::GetItemCount(Source) + FRaidInventoryModel::GetItemCount(Destination);
    const int64 ValueBefore = FRaidInventoryModel::GetTotalValue(Source) + FRaidInventoryModel::GetTotalValue(Destination);
    const FRaidTransferRequest PartialRequest = Request(Source, Destination, TechId, 4, ERaidTransferPolicy::UpToCapacity);
    const FRaidTransferResult Partial = FRaidInventoryModel::Transfer(Source, Destination, PartialRequest);
    Check(Partial.IsSuccess() && Partial.Quantity == 1 && FRaidInventoryModel::GetItemCount(Destination) == 2
        && Source.Entries[0].Quantity == 4
        && FRaidInventoryModel::GetItemCount(Source) + FRaidInventoryModel::GetItemCount(Destination) == TotalBefore
        && FRaidInventoryModel::GetTotalValue(Source) + FRaidInventoryModel::GetTotalValue(Destination) == ValueBefore,
        TEXT("partial_transfer_moves_only_free_capacity_and_conserves_quantity_and_value"));
    BeforeSource = Source; BeforeDestination = Destination;
    Check(FRaidInventoryModel::Transfer(Source, Destination, PartialRequest).Code == ECode::StaleRevision
        && SameState(Source, BeforeSource) && SameState(Destination, BeforeDestination),
        TEXT("duplicate_successful_command_is_rejected_without_a_second_transfer"));
    FRaidTransferRequest Stale = Request(Source, Destination, TechId, 1);
    --Stale.ExpectedSourceRevision;
    Check(FRaidInventoryModel::Transfer(Source, Destination, Stale).Code == ECode::StaleRevision
        && SameState(Source, BeforeSource) && SameState(Destination, BeforeDestination), TEXT("source_revision_is_independently_checked"));
    Stale = Request(Source, Destination, TechId, 1); --Stale.ExpectedDestinationRevision;
    Check(FRaidInventoryModel::Transfer(Source, Destination, Stale).Code == ECode::StaleRevision
        && SameState(Source, BeforeSource) && SameState(Destination, BeforeDestination), TEXT("destination_revision_is_independently_checked"));
    Check(FRaidInventoryModel::Transfer(Source, Source, Request(Source, Source, TechId, 1)).Code == ECode::SelfTransfer
        && SameState(Source, BeforeSource), TEXT("same_state_transfer_is_rejected"));

    FRaidInventoryState Hidden, Empty;
    FRaidInventoryModel::InitializeOnce(Hidden, {Seed(Tech, 2, false)}, -1, true);
    FRaidInventoryModel::InitializeOnce(Empty, {}, 8, false);
    const int32 HiddenId = Hidden.Entries[0].EntryId;
    const FRaidInventoryState BeforeHidden = Hidden, BeforeEmpty = Empty;
    Check(FRaidInventoryModel::Transfer(Hidden, Empty, Request(Hidden, Empty, HiddenId, 1)).Code == ECode::HiddenEntry
        && FRaidInventoryModel::Consume(Hidden, HiddenId, 1, Hidden.Revision).Code == ECode::HiddenEntry
        && SameState(Hidden, BeforeHidden) && SameState(Empty, BeforeEmpty), TEXT("unknown_item_cannot_be_transferred_or_consumed"));
    Check(FRaidInventoryModel::Reveal(Hidden, HiddenId, Hidden.Revision).IsSuccess()
        && Hidden.Entries[0].Quantity == 2 && FRaidInventoryModel::GetItemCount(Empty) == 0,
        TEXT("revealing_changes_knowledge_without_moving_or_granting_items"));
    const FRaidInventoryState Revealed = Hidden;
    Check(FRaidInventoryModel::Reveal(Hidden, HiddenId, Hidden.Revision).Code == ECode::AlreadyRevealed
        && SameState(Hidden, Revealed), TEXT("repeated_reveal_has_no_revision_or_quantity_effect"));
    const FRaidTransferResult All = FRaidInventoryModel::Transfer(Hidden, Empty, Request(Hidden, Empty, HiddenId, 2));
    Check(All.IsSuccess() && All.Quantity == 2 && Hidden.Entries.Num() == 1
        && Hidden.Entries[0].EntryId == HiddenId && Hidden.Entries[0].Quantity == 0
        && Empty.Entries[0].Quantity == 2, TEXT("exact_success_keeps_empty_container_handle_and_moves_all_requested_units"));

    FRaidInventoryState Compact;
    FRaidInventoryModel::InitializeOnce(Compact, {Seed(Tech, 1), Seed(Med, 1)}, 8, false);
    const int32 RemovedId = Compact.Entries[0].EntryId, RemainingId = Compact.Entries[1].EntryId;
    FRaidInventoryModel::Consume(Compact, RemovedId, 1, Compact.Revision);
    const FRaidInventoryResult Readded = FRaidInventoryModel::Grant(Compact, Tech, 1, Compact.Revision);
    Check(Readded.IsSuccess() && Readded.EntryId != RemovedId && Readded.EntryId != RemainingId
        && !FRaidInventoryModel::FindEntry(Compact, RemovedId)
        && FRaidInventoryModel::FindEntry(Compact, RemainingId)->Definition.ItemId == Med.ItemId,
        TEXT("backpack_compaction_never_reuses_or_retargets_an_old_entry_handle"));

    FRaidInventoryState Variants;
    FRaidInventoryModel::InitializeOnce(Variants, {Seed(Tech, 1)}, -1, false);
    FRaidItemDefinition LabelVariant = Tech; LabelVariant.DisplayName = TEXT("Other label");
    FRaidItemDefinition PriceVariant = Tech; PriceVariant.UnitValue = 121;
    FRaidInventoryModel::Grant(Variants, LabelVariant, 1, Variants.Revision);
    FRaidInventoryModel::Grant(Variants, PriceVariant, 1, Variants.Revision);
    FRaidInventoryModel::Grant(Variants, Tech, 2, Variants.Revision);
    Check(Variants.Entries.Num() == 3 && Variants.Entries[0].Quantity == 3
        && Variants.Entries[1].Quantity == 1 && Variants.Entries[2].Quantity == 1,
        TEXT("R5_label_and_price_variants_stay_distinct_while_identical_keys_merge"));
    FRaidItemDefinition BadEffect = Tech; BadEffect.UseEffect = ERaidItemUseEffect::Ammo; BadEffect.UseAmount = 24;
    const FRaidInventoryState BeforeVariants = Variants;
    Check(FRaidInventoryModel::Grant(Variants, BadEffect, 1, Variants.Revision).Code == ECode::IncompatibleDefinition
        && SameState(Variants, BeforeVariants), TEXT("same_stack_key_cannot_merge_different_use_effects"));
    FRaidInventoryState DifferentAmount;
    FRaidItemDefinition OtherMed = Med; OtherMed.UseAmount = 44;
    FRaidInventoryModel::InitializeOnce(DifferentAmount, {Seed(OtherMed, 1)}, -1, false);
    const FRaidInventoryState BeforeDifferentAmount = DifferentAmount, BeforeCompact = Compact;
    Check(FRaidInventoryModel::Transfer(DifferentAmount, Compact,
        Request(DifferentAmount, Compact, DifferentAmount.Entries[0].EntryId, 1)).Code == ECode::IncompatibleDefinition
        && SameState(DifferentAmount, BeforeDifferentAmount) && SameState(Compact, BeforeCompact),
        TEXT("same_stack_key_cannot_transfer_incompatible_use_amount"));

    FRaidInventoryState UnknownDestination;
    FRaidInventoryModel::InitializeOnce(UnknownDestination, {Seed(Tech, 1, false)}, -1, true);
    const int32 UnknownId = UnknownDestination.Entries[0].EntryId;
    FRaidInventoryModel::Transfer(Empty, UnknownDestination, Request(Empty, UnknownDestination, Empty.Entries[0].EntryId, 1));
    Check(UnknownDestination.Entries.Num() == 2 && UnknownDestination.Entries[0].EntryId == UnknownId
        && UnknownDestination.Entries[0].Quantity == 1 && !UnknownDestination.Entries[0].bRevealed
        && UnknownDestination.Entries[1].Quantity == 1 && UnknownDestination.Entries[1].bRevealed,
        TEXT("storing_known_item_does_not_reveal_or_merge_into_unknown_same_key_row"));

    FRaidInventoryState InvalidSeedState;
    const FRaidInventoryState BeforeInvalidSeed = InvalidSeedState;
    FRaidItemDefinition Invalid = Tech; Invalid.ItemId = NAME_None;
    Check(FRaidInventoryModel::InitializeOnce(InvalidSeedState, {Seed(Tech, 1), Seed(Invalid, 1)}, 8, false).Code == ECode::InvalidDefinition
        && SameState(InvalidSeedState, BeforeInvalidSeed), TEXT("invalid_later_seed_does_not_partially_initialize_first_row"));
    const FRaidInventoryState BeforeInvalidCommand = Compact;
    Check(FRaidInventoryModel::Grant(Compact, Tech, 0, Compact.Revision).Code == ECode::InvalidQuantity
        && FRaidInventoryModel::Consume(Compact, RemainingId, -1, Compact.Revision).Code == ECode::InvalidQuantity
        && SameState(Compact, BeforeInvalidCommand), TEXT("zero_and_negative_mutation_quantities_do_not_change_state"));

    FRaidInventoryState Maximum;
    FRaidItemDefinition Expensive = Tech; Expensive.UnitValue = MAX_int32;
    FRaidInventoryModel::InitializeOnce(Maximum, {Seed(Expensive, MAX_int32)}, -1, false);
    const FRaidInventoryState BeforeMaximum = Maximum;
    Check(FRaidInventoryModel::GetTotalValue(Maximum) == static_cast<int64>(MAX_int32) * MAX_int32
        && FRaidInventoryModel::Grant(Maximum, Expensive, 1, Maximum.Revision).Code == ECode::Overflow
        && SameState(Maximum, BeforeMaximum), TEXT("maximum_quantities_use_wide_value_math_and_reject_integer_overflow"));
    const FRaidInventoryState BeforeOverflowSource = Empty;
    Check(FRaidInventoryModel::Transfer(Empty, Maximum, Request(Empty, Maximum, Empty.Entries[0].EntryId, 1)).Code == ECode::Overflow
        && SameState(Empty, BeforeOverflowSource) && SameState(Maximum, BeforeMaximum), TEXT("exact_transfer_overflow_does_not_debit_source"));
    FRaidInventoryState RevisionLimit = Compact;
    RevisionLimit.Revision = MAX_int64;
    const FRaidInventoryState BeforeRevisionLimit = RevisionLimit;
    Check(FRaidInventoryModel::Consume(RevisionLimit, RemainingId, 1, MAX_int64).Code == ECode::Overflow
        && FRaidInventoryModel::Grant(RevisionLimit, Tech, 1, MAX_int64).Code == ECode::Overflow
        && SameState(RevisionLimit, BeforeRevisionLimit), TEXT("revision_overflow_rejects_consume_and_grant_before_commit"));
    const FRaidInventoryState BeforeRevisionSource = Empty;
    Check(FRaidInventoryModel::Transfer(Empty, RevisionLimit, Request(Empty, RevisionLimit, Empty.Entries[0].EntryId, 1)).Code == ECode::Overflow
        && SameState(Empty, BeforeRevisionSource) && SameState(RevisionLimit, BeforeRevisionLimit), TEXT("destination_revision_overflow_rejects_both_sides"));
    FRaidInventoryState HandlesExhausted;
    FRaidInventoryModel::InitializeOnce(HandlesExhausted, {}, -1, false);
    HandlesExhausted.NextEntryId = static_cast<int64>(MAX_int32) + 1;
    const FRaidInventoryState BeforeHandles = HandlesExhausted;
    Check(FRaidInventoryModel::Grant(HandlesExhausted, Tech, 1, HandlesExhausted.Revision).Code == ECode::Overflow
        && SameState(HandlesExhausted, BeforeHandles), TEXT("exhausted_handles_never_wrap_or_reuse_an_old_ID"));

    URaidItemCatalog* Catalog = NewObject<URaidItemCatalog>(this);
    Catalog->Definitions = {Tech, Med};
    FRaidItemDefinition Resolved;
    FString Error;
    Check(Catalog->ResolveLegacy(Med.ItemId, TEXT("R5 medical variant"), 91, Resolved, &Error)
        && Resolved.DisplayName == TEXT("R5 medical variant") && Resolved.UnitValue == 91
        && Resolved.UseEffect == ERaidItemUseEffect::Heal && Resolved.UseAmount == 45,
        TEXT("catalog_resolves_legacy_label_value_without_guessing_use_behavior"));
    Check(!Catalog->ResolveLegacy(TEXT("MissingDefinition"), TEXT("unknown"), 1, Resolved, &Error)
        && Resolved.ItemId.IsNone() && !Error.IsEmpty(), TEXT("unknown_catalog_item_returns_explicit_failure_without_stale_definition"));
    Catalog->Definitions.Add(Med);
    Check(!Catalog->Validate(&Error) && !Error.IsEmpty(), TEXT("ambiguous_duplicate_catalog_ID_is_rejected"));
    return Failures == 0;
}

URaidInventoryComponent* ARaidFrameworkTestDirector::SpawnInventory(AActor*& OutOwner)
{
    OutOwner = GetWorld()->SpawnActor<AActor>();
    if (!OutOwner) return nullptr;
    URaidInventoryComponent* Inventory = NewObject<URaidInventoryComponent>(OutOwner);
    OutOwner->AddInstanceComponent(Inventory);
    Inventory->RegisterComponent();
    return Inventory;
}

void ARaidFrameworkTestDirector::ObserveChange(URaidInventoryComponent* Inventory, int64 Revision)
{
    ++ChangeCalls;
    if (ObserverMode == 1)
    {
        bObserversSawCompleteCommit &= ObservedSource->GetItemCount() == ExpectedSourceCount
            && ObservedDestination->GetItemCount() == ExpectedDestinationCount
            && ObservedSource->GetRevision() == ExpectedSourceRevision
            && ObservedDestination->GetRevision() == ExpectedDestinationRevision
            && Revision == Inventory->GetRevision();
        bReentryRejected &= ObservedSource->Grant(ProbeDefinition, 1, ObservedSource->GetRevision()).Code == ECode::Busy;
        const FRaidInventoryEntry* Entry = ObservedDestination->GetEntries().IsEmpty()
            ? nullptr : &ObservedDestination->GetEntries()[0];
        bReentryRejected &= Entry && ObservedDestination->Consume(Entry->EntryId, 1,
            ObservedDestination->GetRevision()).Code == ECode::Busy;
    }
    else if (ObserverMode == 2)
    {
        const FRaidInventoryEntry* Remaining = Inventory->FindEntry(ConsumeEntryId);
        bObserversSawCompleteCommit &= EffectValue == 100 && EffectCommits == 1 && Remaining && Remaining->Quantity == 1
            && Inventory->GetItemCount() == ExpectedDestinationCount && Revision == ExpectedDestinationRevision;
        bReentryRejected &= Inventory->Consume(ConsumeEntryId, 1, Inventory->GetRevision()).Code == ECode::Busy;
    }
}

void ARaidFrameworkTestDirector::ObserveEnding(URaidInventoryComponent* Inventory)
{
    ++EndingCalls;
    bEndingReadWasValid = Inventory == ObservedDestination && Inventory->GetItemCount() == ExpectedDestinationCount;
    bEndingMutationRejected = Inventory->Grant(ProbeDefinition, 1, Inventory->GetRevision()).Code == ECode::EndingPlay;
}

bool ARaidFrameworkTestDirector::RunComponentCases()
{
    UE_LOG(LogTemp, Display, TEXT("RAID_FRAMEWORK_PHASE: registered_components"));
    AActor* SourceOwner = nullptr;
    AActor* DestinationOwner = nullptr;
    ObservedSource = SpawnInventory(SourceOwner);
    ObservedDestination = SpawnInventory(DestinationOwner);
    if (!Check(ObservedSource && ObservedDestination && ObservedSource->IsRegistered() && ObservedDestination->IsRegistered()
        && ObservedSource->HasBegunPlay() && ObservedDestination->HasBegunPlay(),
        TEXT("inventory_components_are_registered_and_begun_in_the_real_game_world"))) return false;
    ProbeDefinition = Definition(TEXT("Tech"), TEXT("Observer probe"), 120);
    if (!Check(ObservedSource->InitializeOnce({Seed(ProbeDefinition, 3)}, -1, true).IsSuccess()
        && ObservedDestination->InitializeOnce({}, 8, false).IsSuccess(), TEXT("observer_transaction_fixtures_have_live_available_source"))) return false;
    const FDelegateHandle SourceHandle = ObservedSource->OnInventoryChanged.AddUObject(this, &ARaidFrameworkTestDirector::ObserveChange);
    const FDelegateHandle DestinationHandle = ObservedDestination->OnInventoryChanged.AddUObject(this, &ARaidFrameworkTestDirector::ObserveChange);
    ExpectedSourceCount = 1; ExpectedDestinationCount = 2;
    ExpectedSourceRevision = ObservedSource->GetRevision() + 1;
    ExpectedDestinationRevision = ObservedDestination->GetRevision() + 1;
    ObserverMode = 1; ChangeCalls = 0;
    FRaidTransferRequest Move;
    Move.SourceEntryId = ObservedSource->GetEntries()[0].EntryId;
    Move.Quantity = 2;
    Move.ExpectedSourceRevision = ObservedSource->GetRevision();
    Move.ExpectedDestinationRevision = ObservedDestination->GetRevision();
    const FRaidTransferResult Transfer = ObservedSource->TransferTo(ObservedDestination, Move);
    Check(Transfer.IsSuccess() && Transfer.Quantity == 2 && ChangeCalls == 2 && bObserversSawCompleteCommit,
        TEXT("both_real_component_delegates_observe_both_ledgers_already_committed"));
    Check(bReentryRejected && ObservedSource->GetItemCount() == 1 && ObservedDestination->GetItemCount() == 2,
        TEXT("writes_to_either_component_are_Busy_during_both_transfer_notifications"));
    const int32 CallsBeforeStale = ChangeCalls;
    Check(ObservedSource->TransferTo(ObservedDestination, Move).Code == ECode::StaleRevision
        && ChangeCalls == CallsBeforeStale && ObservedSource->GetItemCount() == 1 && ObservedDestination->GetItemCount() == 2,
        TEXT("failed_component_command_broadcasts_no_change_and_moves_no_items"));
    ObservedSource->OnInventoryChanged.Remove(SourceHandle);
    ObservedDestination->OnInventoryChanged.Remove(DestinationHandle);

    const FRaidItemDefinition Med = Definition(TEXT("Medkit"), TEXT("Commit probe"), 75, ERaidItemUseEffect::Heal, 45);
    const FRaidInventoryResult Granted = ObservedDestination->Grant(Med, 2, ObservedDestination->GetRevision());
    if (!Check(Granted.IsSuccess(), TEXT("consume_order_probe_has_two_real_available_units"))) return false;
    ConsumeEntryId = Granted.EntryId;
    ExpectedDestinationCount = ObservedDestination->GetItemCount() - 1;
    ExpectedDestinationRevision = ObservedDestination->GetRevision() + 1;
    EffectValue = 55; EffectCommits = 0; ChangeCalls = 0; ObserverMode = 2;
    bObserversSawCompleteCommit = true; bReentryRejected = true;
    const FDelegateHandle ConsumeHandle = ObservedDestination->OnInventoryChanged.AddUObject(this, &ARaidFrameworkTestDirector::ObserveChange);
    const int64 BeforeConsumeRevision = ObservedDestination->GetRevision();
    const FRaidInventoryResult Consumed = ObservedDestination->ConsumeWithCommit(ConsumeEntryId, 1, BeforeConsumeRevision, [this]()
    {
        EffectValue = 100;
        ++EffectCommits;
        bReentryRejected &= ObservedDestination->Grant(ProbeDefinition, 1, ObservedDestination->GetRevision()).Code == ECode::Busy;
    });
    Check(Consumed.IsSuccess() && ChangeCalls == 1 && bObserversSawCompleteCommit && EffectValue == 100,
        TEXT("consume_effect_is_applied_before_first_inventory_change_observer"));
    Check(bReentryRejected, TEXT("effect_commit_and_change_observer_both_reject_nested_inventory_writes"));
    Check(ObservedDestination->ConsumeWithCommit(ConsumeEntryId, 1, BeforeConsumeRevision, [this]() { ++EffectCommits; }).Code == ECode::StaleRevision
        && EffectCommits == 1 && ChangeCalls == 1 && ObservedDestination->FindEntry(ConsumeEntryId)->Quantity == 1,
        TEXT("stale_consume_cannot_apply_effect_twice_with_another_unit_still_available"));
    ObservedDestination->OnInventoryChanged.Remove(ConsumeHandle);
    ObservedDestination->OnInventoryEndingPlay.AddUObject(this, &ARaidFrameworkTestDirector::ObserveEnding);
    DestinationOwner->Destroy();
    Check(EndingCalls == 1 && bEndingReadWasValid && bEndingMutationRejected,
        TEXT("real_owner_destruction_notifies_endplay_once_with_readable_state_and_closed_write_gate"));
    Check(ObservedDestination->Grant(ProbeDefinition, 1, ObservedDestination->GetRevision()).Code == ECode::EndingPlay
        && EffectCommits == 1 && ChangeCalls == 1, TEXT("ended_component_rejects_later_commands_without_notifications"));
    SourceOwner->Destroy();
    ObservedSource = nullptr;
    ObservedDestination = nullptr;
    return Failures == 0;
}

bool ARaidFrameworkTestDirector::CheckProductionCatalog()
{
    const URaidItemCatalog* Catalog = Player->GetItemCatalog();
    URaidInventoryComponent* Backpack = Player->GetInventoryComponent();
    URaidInteractionComponent* Interaction = Player->GetInteractionComponent();
    FString Error;
    const FString AssetPath = IsValid(Catalog) ? Catalog->GetPathName() : TEXT("missing");
    const bool bCatalogReady = IsValid(Catalog) && Catalog->IsAsset()
        && AssetPath == TEXT("/Game/Data/DA_RaidItems.DA_RaidItems")
        && Catalog->Validate(&Error) && Catalog->Definitions.Num() == 4;
    if (!Check(bCatalogReady && IsValid(Backpack) && IsValid(Interaction) && Player->CanAct()
        && !Player->IsInventoryOpen() && Backpack->IsInitialized(),
        TEXT("production_catalog_asset_loaded_without_fallback"),
        FString::Printf(TEXT("asset=%s is_asset=%d definitions=%d error=%s"), *AssetPath,
            IsValid(Catalog) && Catalog->IsAsset(), IsValid(Catalog) ? Catalog->Definitions.Num() : 0, *Error))) return false;

    auto SameDefinition = [](const FRaidItemDefinition& A, const FRaidItemDefinition& B)
    {
        return A.ItemId == B.ItemId && A.DisplayName == B.DisplayName && A.UnitValue == B.UnitValue
            && A.UseEffect == B.UseEffect && A.UseAmount == B.UseAmount;
    };
    const float SavedHealth = Player->Health;
    const int32 SavedReserve = Player->ReserveAmmo;
    const int32 OriginalCount = Backpack->GetItemCount();
    const int64 OriginalValue = Backpack->GetTotalValue();

    // Read the same loaded asset used by Player::AddItem. No test definition or
    // direct component grant may substitute for a missing cooked catalog here.
    const FRaidItemDefinition* Medkit = Catalog->Definitions.FindByPredicate([](const FRaidItemDefinition& Item)
        { return Item.ItemId == FName(TEXT("Medkit")); });
    Player->Health = 50.f;
    const bool bMedGranted = Medkit && Medkit->UseEffect == ERaidItemUseEffect::Heal && Medkit->UseAmount > 0
        && Player->AddItem(Medkit->ItemId, Medkit->DisplayName, 1, Medkit->UnitValue);
    const FRaidInventoryEntry* MedEntry = bMedGranted ? Backpack->GetEntries().FindByPredicate([&](const FRaidInventoryEntry& Entry)
        { return SameDefinition(Entry.Definition, *Medkit); }) : nullptr;
    const bool bMedDefinitionMatches = MedEntry && MedEntry->Quantity == 1 && MedEntry->bRevealed
        && Backpack->GetItemCount() == OriginalCount + 1 && Backpack->GetTotalValue() == OriginalValue + Medkit->UnitValue;
    const int32 MedEntryId = MedEntry ? MedEntry->EntryId : INDEX_NONE;
    const FRaidInventoryResult MedUsed = bMedDefinitionMatches
        ? Interaction->UseEntry(MedEntryId, Backpack->GetRevision()) : FRaidInventoryResult();
    const float ExpectedHealth = 50.f; // R10: accepted treatment is deferred, not instant healing.
    const bool bMedPassed = bMedGranted && bMedDefinitionMatches && MedUsed.IsSuccess() && MedUsed.Quantity == 0
        && FMath::IsNearlyEqual(Player->Health, ExpectedHealth) && Player->IsTreating()
        && Player->ReserveAmmo == SavedReserve && Backpack->GetItemCount() == OriginalCount + 1
        && Backpack->GetTotalValue() == OriginalValue + Medkit->UnitValue && Backpack->FindEntry(MedEntryId);
    const bool bMedCheck = Check(bMedPassed, TEXT("production_catalog_medkit_grant_and_deferred_use"),
        FString::Printf(TEXT("asset=%s item=%s label=%s unit_value=%d effect=%d amount=%d granted=%d definition_matches=%d use_code=%d health=50->%.1f expected=%.1f count=%d->%d"),
            *AssetPath, Medkit ? *Medkit->ItemId.ToString() : TEXT("missing"), Medkit ? *Medkit->DisplayName : TEXT("missing"),
            Medkit ? Medkit->UnitValue : -1, Medkit ? static_cast<int32>(Medkit->UseEffect) : -1,
            Medkit ? Medkit->UseAmount : -1, bMedGranted, bMedDefinitionMatches, static_cast<int32>(MedUsed.Code),
            Player->Health, ExpectedHealth, OriginalCount, Backpack->GetItemCount()));
    Player->Health = SavedHealth;
    Player->ReserveAmmo = SavedReserve;
    Player->CancelTreatment();
    if (Backpack->FindEntry(MedEntryId)) Backpack->Consume(MedEntryId,1,Backpack->GetRevision());
    if (!bMedCheck) return false;

    const FRaidItemDefinition* Ammo = Catalog->Definitions.FindByPredicate([](const FRaidItemDefinition& Item)
        { return Item.ItemId == FName(TEXT("Ammo")); });
    const bool bAmmoGranted = Ammo && Ammo->UseEffect == ERaidItemUseEffect::Ammo && Ammo->UseAmount > 0
        && Player->AddItem(Ammo->ItemId, Ammo->DisplayName, 1, Ammo->UnitValue);
    const FRaidInventoryEntry* AmmoEntry = bAmmoGranted ? Backpack->GetEntries().FindByPredicate([&](const FRaidInventoryEntry& Entry)
        { return SameDefinition(Entry.Definition, *Ammo); }) : nullptr;
    const bool bAmmoDefinitionMatches = AmmoEntry && AmmoEntry->Quantity == 1 && AmmoEntry->bRevealed
        && Backpack->GetItemCount() == OriginalCount + 1 && Backpack->GetTotalValue() == OriginalValue + Ammo->UnitValue
        && Player->ReserveAmmo == SavedReserve;
    const int32 AmmoEntryId = AmmoEntry ? AmmoEntry->EntryId : INDEX_NONE;
    const FRaidInventoryResult AmmoUsed = bAmmoDefinitionMatches
        ? Interaction->UseEntry(AmmoEntryId, Backpack->GetRevision()) : FRaidInventoryResult();
    const int64 ExpectedReserve = static_cast<int64>(SavedReserve) + (Ammo ? Ammo->UseAmount : 0);
    const bool bAmmoPassed = bAmmoGranted && bAmmoDefinitionMatches && AmmoUsed.IsSuccess() && AmmoUsed.Quantity == 1
        && static_cast<int64>(Player->ReserveAmmo) == ExpectedReserve && Player->ReserveAmmo > SavedReserve
        && FMath::IsNearlyEqual(Player->Health, SavedHealth) && Backpack->GetItemCount() == OriginalCount
        && Backpack->GetTotalValue() == OriginalValue && !Backpack->FindEntry(AmmoEntryId);
    const bool bAmmoCheck = Check(bAmmoPassed, TEXT("production_catalog_ammo_grant_and_use"),
        FString::Printf(TEXT("asset=%s item=%s label=%s unit_value=%d effect=%d amount=%d granted=%d definition_matches=%d use_code=%d reserve=%d->%d expected=%lld count=%d->%d"),
            *AssetPath, Ammo ? *Ammo->ItemId.ToString() : TEXT("missing"), Ammo ? *Ammo->DisplayName : TEXT("missing"),
            Ammo ? Ammo->UnitValue : -1, Ammo ? static_cast<int32>(Ammo->UseEffect) : -1,
            Ammo ? Ammo->UseAmount : -1, bAmmoGranted, bAmmoDefinitionMatches, static_cast<int32>(AmmoUsed.Code),
            SavedReserve, Player->ReserveAmmo, static_cast<long long>(ExpectedReserve), OriginalCount, Backpack->GetItemCount()));
    Player->Health = SavedHealth;
    Player->ReserveAmmo = SavedReserve;
    return bAmmoCheck;
}

FRaidInventoryCommandContext ARaidFrameworkTestDirector::CaptureContext() const
{
    FRaidInventoryCommandContext Context;
    Context.Player = Player;
    Context.Container = Player->GetOpenLoot();
    Context.Backpack = Player->GetInventoryComponent();
    Context.ContainerInventory = Player->GetOpenLoot() ? Player->GetOpenLoot()->GetInventoryComponent() : nullptr;
    Context.SessionRevision = Player->GetInteractionRevision();
    Context.BackpackRevision = Player->GetInventoryComponent()->GetRevision();
    Context.ContainerRevision = Context.ContainerInventory.IsValid() ? Context.ContainerInventory->GetRevision() : INDEX_NONE;
    Context.bHadContainer = Player->GetOpenLoot() != nullptr;
    return Context;
}

ARaidLoot* ARaidFrameworkTestDirector::SpawnSearchCase(const FVector& Position, bool bDifferentUnknown)
{
    const FTransform Transform(Position);
    ARaidLoot* Loot = GetWorld()->SpawnActorDeferred<ARaidLoot>(ARaidLoot::StaticClass(), Transform,
        this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Loot) return nullptr;
    Loot->SearchDuration = 1.5f;
    Loot->bGenerateSupplementalLoot = false;
    const FRaidItemDefinition Known = Definition(TEXT("Tech"), TEXT("Known lifecycle item"), 120);
    const FRaidItemDefinition Unknown = bDifferentUnknown
        ? Definition(TEXT("Ammo"), TEXT("Replacement unknown item"), 40, ERaidItemUseEffect::Ammo, 24)
        : Definition(TEXT("Medkit"), TEXT("Original unknown item"), 75, ERaidItemUseEffect::Heal, 45);
    URaidInventoryComponent* Inventory = Loot->GetInventoryComponent();
    if (!Inventory || !Inventory->InitializeOnce({Seed(Known, 2), Seed(Unknown, 1, false)}, -1, true).IsSuccess())
    {
        Loot->Destroy();
        return nullptr;
    }
    UGameplayStatics::FinishSpawningActor(Loot, Transform);
    return Loot;
}

void ARaidFrameworkTestDirector::PlaceAndAim(ARaidPlayer* Pawn, ARaidLoot* Loot)
{
    Pawn->GetCharacterMovement()->StopMovementImmediately();
    Pawn->SetActorLocation(FVector(-1900.f, -1400.f, 96.f), false, nullptr, ETeleportType::TeleportPhysics);
    Pawn->GetCapsuleComponent()->UpdateOverlaps();
    const FRotator View = (Loot->GetActorLocation() - Pawn->Camera->GetComponentLocation()).Rotation();
    PC->SetControlRotation(View);
    Pawn->Camera->SetWorldRotation(View);
}

bool ARaidFrameworkTestDirector::OpenAndSearch(ARaidLoot* Loot)
{
    if (!IsValid(Loot) || Player->GetFocusedLoot() != Loot) return false;
    PC->HandleInteract();
    if (Player->GetOpenLoot() != Loot) return false;
    return Player->IsSearching(); // Opening is the production search trigger in R9.
}

bool ARaidFrameworkTestDirector::ExecuteAxis(FName Name, float Value)
{
    UInputComponent* Input = Player->InputComponent.Get();
    if (!Input) return false;
    for (FInputAxisBinding& Binding : Input->AxisBindings)
    {
        if (Binding.AxisName == Name && Binding.AxisDelegate.IsBound())
        {
            Binding.AxisDelegate.Execute(Value);
            return true;
        }
    }
    return false;
}

bool ARaidFrameworkTestDirector::ExecuteAction(FName Name, bool Pressed)
{
    UInputComponent* Input = Player->InputComponent.Get();
    if (!Input) return false;
    for (int32 Index = 0; Index < Input->GetNumActionBindings(); ++Index)
    {
        FInputActionBinding& Binding = Input->GetActionBinding(Index);
        if (Binding.GetActionName() == Name && Binding.KeyEvent == (Pressed ? IE_Pressed : IE_Released)
            && Binding.ActionDelegate.IsBound())
        {
            Binding.ActionDelegate.Execute(FKey());
            return true;
        }
    }
    return false;
}

void ARaidFrameworkTestDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bFinished) return;
    if (FPlatformTime::Seconds() - StartedAt > 90.0) { Abort(TEXT("framework_suite_timeout_90s")); return; }
    StepSeconds += DeltaSeconds;
    if (StepSeconds < .2f) return;
    if (Step != EStep::Ready && (!IsValid(Mode) || !IsValid(PC) || !IsValid(Player) || !IsValid(HUD)))
    { Abort(TEXT("lifecycle_world_references_invalid")); return; }
    switch (Step)
    {
    case EStep::Ready:
        Mode = Cast<ARaidGameMode>(UGameplayStatics::GetGameMode(this));
        PC = Cast<ARaidPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
        Player = PC ? Cast<ARaidPlayer>(PC->GetPawn()) : nullptr;
        HUD = PC ? Cast<ARaidHUD>(PC->GetHUD()) : nullptr;
        if (!Mode || !PC || !Player || !HUD || !Player->GetInventoryComponent()->IsInitialized())
        {
            if (StepSeconds > 8.f) Abort(TEXT("framework_controller_player_HUD_not_ready"));
            break;
        }
        if (StepSeconds < .6f) break;
        if (!Check(Mode->IsRaidActive() && Player->CanAct(), TEXT("framework_runs_in_normal_running_world_with_possessed_player")))
        { Finish(); break; }
        if (!RunKernelCases() || !RunComponentCases()) { Finish(); break; }
        Advance(EStep::LifecycleSetup);
        break;

    case EStep::LifecycleSetup:
        UE_LOG(LogTemp, Display, TEXT("RAID_FRAMEWORK_PHASE: interaction_lifecycle"));
        // These cases isolate possession and callback lifetime. Normal-AI combat
        // remains covered by the separate R5 container regression phase.
        for (TActorIterator<ARaidEnemy> It(GetWorld()); It; ++It)
        {
            It->DetectionRadius = 0.f;
            It->AttackRange = 0.f;
        }
        UE_LOG(LogTemp, Display, TEXT("RAID_FRAMEWORK_FIXTURE: saved guards detection/attack suppressed for deterministic possession lifecycle; no combat claim from this suite."));
        SearchBox = SpawnSearchCase(FVector(-1720.f, -1400.f, 48.f));
        if (!SearchBox) { Abort(TEXT("lifecycle_deferred_container_seed_failed")); break; }
        PlaceAndAim(Player, SearchBox);
        if (!Check(OpenAndSearch(SearchBox), TEXT("controller_opens_world_focused_case_and_starts_actual_search"))) { Finish(); break; }
        Advance(EStep::SearchBeforeUnpossess);
        break;

    case EStep::SearchBeforeUnpossess:
        if (Player->IsSearching() && Player->GetSearchProgress() > .2f)
        {
            OldContext = CaptureContext();
            KnownEntryId = SearchBox->GetInventoryComponent()->GetEntries()[0].EntryId;
            SavedSourceRevision = SearchBox->GetRevision();
            SavedBackpackRevision = Player->GetInventoryComponent()->GetRevision();
            if (!Check(Player->GetSearchProgress() < .8f && PC->IsInventoryContextCurrent(OldContext)
                && SearchBox->GetInventoryComponent()->FindEntry(KnownEntryId)->Quantity == 2
                && Player->GetLootCount() < Player->MaxLootItems,
                TEXT("unpossess_probe_has_valid_context_partial_search_and_a_takeable_item"))) { Finish(); break; }
            PC->UnPossess();
            Check(Player->IsAlive() && !Player->CanAct() && PC->GetPawn() == nullptr
                && !Player->IsInventoryOpen() && !Player->GetOpenLoot() && !Player->IsSearching(),
                TEXT("real_UnPossess_cleans_living_old_pawn_search_and_panel"));
            Check(PC->SubmitInventoryCommand(ERaidInventoryCommand::Take, KnownEntryId, OldContext) == ECode::StaleRevision
                && SearchBox->GetRevision() == SavedSourceRevision
                && Player->GetInventoryComponent()->GetRevision() == SavedBackpackRevision
                && SearchBox->GetInventoryComponent()->FindEntry(KnownEntryId)->Quantity == 2,
                TEXT("unpossessed_pawn_context_cannot_take_real_available_item"));
            Check(HUD->GetObservedPlayer() == nullptr, TEXT("UnPossess_releases_HUD_observation_of_the_still_living_pawn"));
            Advance(EStep::WaitAfterUnpossess);
        }
        else if (StepSeconds > 3.f) Abort(TEXT("partial_search_before_UnPossess_not_observed"));
        break;

    case EStep::WaitAfterUnpossess:
        if (StepSeconds < SearchBox->SearchDuration + .3f) break;
        Check(!SearchBox->GetInventoryComponent()->GetEntries()[1].bRevealed
            && SearchBox->GetRevision() == SavedSourceRevision && !Player->IsSearching(),
            TEXT("unpossessed_old_search_cannot_reveal_after_its_original_deadline"));
        PC->Possess(Player);
        PlaceAndAim(Player, SearchBox);
        if (!Check(Player->CanAct() && OpenAndSearch(SearchBox), TEXT("same_living_pawn_can_be_repossessed_and_start_a_new_search"))) { Finish(); break; }
        Advance(EStep::SearchBeforeReplacement);
        break;

    case EStep::SearchBeforeReplacement:
        if (Player->IsSearching() && Player->GetSearchProgress() > .2f)
        {
            OldContext = CaptureContext();
            SavedSourceRevision = SearchBox->GetRevision();
            SavedBackpackRevision = Player->GetInventoryComponent()->GetRevision();
            if (!Check(PC->IsInventoryContextCurrent(OldContext) && Player->GetSearchProgress() < .8f,
                TEXT("pawn_replacement_begins_from_a_current_live_search_context"))) { Finish(); break; }
            PreviousPlayer = Player;
            FActorSpawnParameters Spawn;
            Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            ARaidPlayer* NewPlayer = GetWorld()->SpawnActor<ARaidPlayer>(ARaidPlayer::StaticClass(),
                FVector(-1900.f, -1570.f, 96.f), FRotator::ZeroRotator, Spawn);
            if (!NewPlayer) { Abort(TEXT("replacement_player_spawn_failed")); break; }
            PC->Possess(NewPlayer);
            Player = NewPlayer;
            Check(PC->GetPawn() == Player && Player->CanAct() && PreviousPlayer->IsAlive()
                && !PreviousPlayer->CanAct() && !PreviousPlayer->IsSearching() && !PreviousPlayer->IsInventoryOpen(),
                TEXT("real_pawn_replacement_cleans_old_possession_and_activates_new_pawn"));
            Check(PC->SubmitInventoryCommand(ERaidInventoryCommand::Take, KnownEntryId, OldContext) == ECode::StaleRevision
                && SearchBox->GetRevision() == SavedSourceRevision
                && PreviousPlayer->GetInventoryComponent()->GetRevision() == SavedBackpackRevision
                && Player->GetLootCount() == 0,
                TEXT("old_pawn_command_cannot_mutate_either_backpack_after_replacement"));
            PreviousPlayer->SetActorLocation(FVector(-2110.f, -1750.f, 96.f), false, nullptr, ETeleportType::TeleportPhysics);
            Advance(EStep::WaitAfterReplacement);
        }
        else if (StepSeconds > 3.f) Abort(TEXT("partial_search_before_pawn_replacement_not_observed"));
        break;

    case EStep::WaitAfterReplacement:
    {
        if (StepSeconds < .3f) break;
        Check(HUD->GetObservedPlayer() == Player && !PC->IsMoveInputIgnored() && !PC->IsLookInputIgnored() && !PC->bShowMouseCursor,
            TEXT("new_possession_updates_HUD_and_restores_world_input_policy"));
        const int32 Refreshes = HUD->GetInventoryViewRefreshCount();
        const int64 ViewRevision = HUD->GetInventoryViewRevision();
        const FRaidInventoryResult OldGrant = PreviousPlayer->GetInventoryComponent()->Grant(ProbeDefinition, 1,
            PreviousPlayer->GetInventoryComponent()->GetRevision());
        Check(OldGrant.IsSuccess() && HUD->GetInventoryViewRefreshCount() == Refreshes
            && HUD->GetInventoryViewRevision() == ViewRevision && HUD->GetInventoryViewItemCount() == 0,
            TEXT("old_living_pawn_inventory_events_no_longer_refresh_new_pawn_HUD"));
        const FRaidInventoryResult NewGrant = Player->GetInventoryComponent()->Grant(ProbeDefinition, 1,
            Player->GetInventoryComponent()->GetRevision());
        Check(NewGrant.IsSuccess() && HUD->GetInventoryViewRefreshCount() > Refreshes
            && HUD->GetInventoryViewItemCount() == 1 && HUD->GetInventoryViewTotalValue() == ProbeDefinition.UnitValue
            && HUD->GetInventoryViewRevision() == Player->GetInventoryComponent()->GetRevision(),
            TEXT("new_pawn_inventory_delegate_immediately_refreshes_HUD_snapshot"));
        PreviousPlayer->Destroy();
        Check(HUD->GetObservedPlayer() == Player && PC->GetPawn() == Player && Player->CanAct(),
            TEXT("destroying_replaced_pawn_does_not_clear_new_pawn_subscription"));
        PC->SetControlRotation(FRotator::ZeroRotator);
        Player->Camera->SetWorldRotation(FRotator::ZeroRotator);
        ControlStartPosition = Player->GetActorLocation();
        Advance(EStep::ControlMove);
        break;
    }

    case EStep::ControlMove:
        if (StepSeconds < .65f)
        {
            if (!ExecuteAxis(TEXT("MoveForward"), 1.f)) Abort(TEXT("new_pawn_MoveForward_binding_missing"));
        }
        else
        {
            Check(Player->GetActorLocation().X - ControlStartPosition.X > 60.f,
                TEXT("new_pawn_bound_movement_produces_real_displacement"));
            ExecuteAxis(TEXT("MoveForward"), 0.f);
            Player->ConsumeMovementInputVector();
            Player->GetCharacterMovement()->StopMovementImmediately();
            ControlStartView = PC->GetControlRotation();
            Advance(EStep::ControlLook);
        }
        break;

    case EStep::ControlLook:
        if (StepSeconds < .6f)
        {
            if (!ExecuteAxis(TEXT("Turn"), 30.f * DeltaSeconds) || !ExecuteAxis(TEXT("LookUp"), 15.f * DeltaSeconds))
                Abort(TEXT("new_pawn_look_bindings_missing"));
        }
        else
        {
            const FRotator View = PC->GetControlRotation();
            Check(FMath::Abs(FMath::FindDeltaAngleDegrees(ControlStartView.Yaw, View.Yaw)) > 2.f
                && FMath::Abs(FMath::FindDeltaAngleDegrees(ControlStartView.Pitch, View.Pitch)) > 1.f,
                TEXT("new_pawn_bound_view_axes_update_both_angles"));
            ExecuteAxis(TEXT("Turn"), 0.f); ExecuteAxis(TEXT("LookUp"), 0.f);
            const FRotator SafeAim = FVector(0.f, -1000.f, 800.f).Rotation();
            PC->SetControlRotation(SafeAim); Player->Camera->SetWorldRotation(SafeAim);
            BeforeMagazine = Player->MagazineAmmo; BeforeReserve = Player->ReserveAmmo;
            Check(ExecuteAction(TEXT("Fire"), true) && Player->MagazineAmmo == BeforeMagazine - 1
                && Player->ReserveAmmo == BeforeReserve && ExecuteAction(TEXT("Fire"), false),
                TEXT("new_pawn_bound_fire_press_and_release_consume_one_round"));
            Advance(EStep::ControlFireWait);
        }
        break;

    case EStep::ControlFireWait:
        if (StepSeconds < .65f) break;
        Check(Player->MagazineAmmo == BeforeMagazine - 1 && Player->ReserveAmmo == BeforeReserve,
            TEXT("new_pawn_fire_release_stops_repeating_timer"));
        SearchBox->Destroy();
        SearchBox = SpawnSearchCase(FVector(-1720.f, -1400.f, 48.f));
        if (!SearchBox) { Abort(TEXT("destroy_case_container_spawn_failed")); break; }
        PlaceAndAim(Player, SearchBox);
        if (!Check(OpenAndSearch(SearchBox), TEXT("container_destroy_probe_starts_actual_search"))) { Finish(); break; }
        Advance(EStep::SearchBeforeDestroy);
        break;

    case EStep::SearchBeforeDestroy:
        if (Player->IsSearching() && Player->GetSearchProgress() > .2f)
        {
            OldContext = CaptureContext();
            KnownEntryId = SearchBox->GetInventoryComponent()->GetEntries()[0].EntryId;
            SavedBackpackRevision = Player->GetInventoryComponent()->GetRevision();
            if (!Check(PC->IsInventoryContextCurrent(OldContext) && Player->GetSearchProgress() < .8f
                && SearchBox->GetInventoryComponent()->FindEntry(KnownEntryId)->Quantity == 2,
                TEXT("destroy_probe_context_is_current_and_has_available_takeable_source"))) { Finish(); break; }
            SearchBox->Destroy();
            Check(!Player->GetOpenLoot() && !Player->IsInventoryOpen() && !Player->IsSearching()
                && !PC->IsMoveInputIgnored() && !PC->IsLookInputIgnored() && !PC->bShowMouseCursor,
                TEXT("real_container_EndPlay_closes_search_and_restores_input_immediately"));
            Check(PC->SubmitInventoryCommand(ERaidInventoryCommand::Take, KnownEntryId, OldContext) == ECode::StaleRevision
                && Player->GetInventoryComponent()->GetRevision() == SavedBackpackRevision && Player->GetLootCount() == 1,
                TEXT("destroyed_container_command_cannot_grant_an_available_item"));
            SearchBox = nullptr;
            ReplacementBox = SpawnSearchCase(FVector(-1720.f, -1400.f, 48.f), true);
            if (!ReplacementBox) { Abort(TEXT("replacement_search_container_spawn_failed")); break; }
            ReplacementBox->SearchDuration = 4.f;
            PlaceAndAim(Player, ReplacementBox);
            Advance(EStep::WaitAfterDestroy);
        }
        else if (StepSeconds > 3.f) Abort(TEXT("partial_search_before_container_destroy_not_observed"));
        break;

    case EStep::WaitAfterDestroy:
        if (!bReplacementSearchStarted)
        {
            if (!Check(OpenAndSearch(ReplacementBox) && ReplacementBox->GetInventoryComponent()->GetEntries()[0].EntryId == KnownEntryId,
                TEXT("new_container_reuses_numeric_handle_but_has_new_identity_and_search"))) { Finish(); break; }
            SavedSourceRevision = ReplacementBox->GetRevision();
            bReplacementSearchStarted = true;
        }
        if (!bCheckedOldDeadline && StepSeconds >= 2.f)
        {
            Check(Player->GetOpenLoot() == ReplacementBox && Player->IsSearching() && Player->GetSearchProgress() < .8f
                && !ReplacementBox->GetInventoryComponent()->GetEntries()[1].bRevealed
                && ReplacementBox->GetRevision() == SavedSourceRevision
                && Player->GetInventoryComponent()->GetRevision() == SavedBackpackRevision,
                TEXT("old_destroyed_search_deadline_cannot_reveal_new_container_matching_handle"));
            Check(PC->SubmitInventoryCommand(ERaidInventoryCommand::Take, KnownEntryId, OldContext) == ECode::StaleRevision
                && ReplacementBox->GetInventoryComponent()->FindEntry(KnownEntryId)->Quantity == 2
                && Player->GetLootCount() == 1, TEXT("old_context_stays_rejected_while_a_new_takeable_container_is_open"));
            bCheckedOldDeadline = true;
        }
        if (bCheckedOldDeadline && !Player->IsSearching())
        {
            Check(StepSeconds >= 4.f && ReplacementBox->GetInventoryComponent()->GetEntries()[1].bRevealed
                && ReplacementBox->GetRevision() == SavedSourceRevision + 1
                && Player->GetInventoryComponent()->GetRevision() == SavedBackpackRevision && Player->GetLootCount() == 1,
                TEXT("only_new_search_deadline_reveals_one_row_without_auto_collection"));
            Player->CloseLoot();
            Advance(EStep::Finish);
        }
        else if (StepSeconds > 7.f) Abort(TEXT("replacement_search_did_not_complete_at_its_own_deadline"));
        break;

    case EStep::Finish:
        CheckProductionCatalog();
        Finish();
        break;
    }
}
