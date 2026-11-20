// Copyright (c) 2026 WinterCrownWORKS. All Rights Reserved.
// Licensed under the MIT License.

using UnrealBuildTool;

public class WcwSnapJumpEditor : ModuleRules
{
	public WcwSnapJumpEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"WcwSnapJump"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				"InputCore",
				"UnrealEd",
				"DeveloperToolSettings",
				"LevelEditor",
				"WorkspaceMenuStructure",
				"ToolMenus",
				"EditorStyle",
				"DesktopPlatform",
				"AssetTools",
				"AssetRegistry",
				"ImageWrapper"
			}
		);
	}
}
