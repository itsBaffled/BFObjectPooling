// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PoolingExample : ModuleRules
{
	public PoolingExample(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "BFObjectPooling", "UMG" });
	}
}
