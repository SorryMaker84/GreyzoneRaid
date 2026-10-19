#include "RaidText.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Font.h"
#include "GameFramework/Actor.h"

UFont* RaidText::LoadWorldFont()
{
    // Explicitly included by ProjectPackagingSettings and referenced by saved
    // map labels. Never consult the Windows installed-font directory at runtime.
    UFont* Font = LoadObject<UFont>(nullptr,
        TEXT("/Game/UI/Fonts/F_RaidChineseWorld.F_RaidChineseWorld"));
    static bool bReportedMissing = false;
    if ((!Font || Font->FontCacheType != EFontCacheType::Offline) && !bReportedMissing)
    {
        bReportedMissing = true;
        UE_LOG(LogTemp, Error, TEXT("R4_WORLD_FONT_MISSING: packaged offline Chinese font unavailable."));
    }
    return Font;
}

void RaidText::ApplyWorldFont(AActor* Actor)
{
    if (!Actor) return;
    UFont* Font = LoadWorldFont();
    if (!Font) return;
    TInlineComponentArray<UTextRenderComponent*> Labels(Actor);
    for (UTextRenderComponent* Label : Labels)
        if (Label && Label->Font != Font) Label->SetFont(Font);
}
