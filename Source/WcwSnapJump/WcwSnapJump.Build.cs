// Copyright (c) 2026 WinterCrownWORKS. All Rights Reserved.
// Licensed under the MIT License.

using UnrealBuildTool;

public class WcwSnapJump : ModuleRules
{
	public WcwSnapJump(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"DeveloperSettings",
				"Json",
				"JsonUtilities",
				"RHI",
				"RenderCore"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AssetRegistry",
				"ImageWrapper",
				"Slate",
				"SlateCore",
				"UMG"
			}
		);
	}
}
