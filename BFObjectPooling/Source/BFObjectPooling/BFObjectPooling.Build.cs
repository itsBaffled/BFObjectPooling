using UnrealBuildTool;

public class BFObjectPooling : ModuleRules
{
    public BFObjectPooling(ReadOnlyTargetRules Target) : base(Target)
    {
        	PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        

        
        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "UMG",          // UUserWidget pools
                "Niagara",      // Niagara pools
                "GameplayTags", // Pooled object tag support
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
            }
        );
    }
}