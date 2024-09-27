using UnrealBuildTool;

public class BFObjectPooling_K2 : ModuleRules
{
    public BFObjectPooling_K2(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "BlueprintGraph",
                "KismetCompiler",
                "UnrealEd", 
                "BFObjectPooling",
            }
        );
    }
}