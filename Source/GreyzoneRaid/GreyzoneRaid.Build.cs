using UnrealBuildTool;
public class GreyzoneRaid : ModuleRules
{
    public GreyzoneRaid(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "InputCore", "AIModule", "NavigationSystem", "SlateCore", "Json"
        });
    }
}
