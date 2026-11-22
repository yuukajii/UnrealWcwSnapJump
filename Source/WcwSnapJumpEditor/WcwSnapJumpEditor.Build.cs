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
				"DeveloperSettings",
				"WcwSnapJump"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				"InputCore",
				"ApplicationCore",
				"UnrealEd",
				"DeveloperToolSettings",
				"Settings",
				"LevelEditor",
				"WorkspaceMenuStructure",
				"ToolMenus",
				"EditorStyle",
				"DesktopPlatform",
				"AssetTools",
				"AssetRegistry",
				"ImageWrapper",
				"HTTP",
				"Json",
				"JsonUtilities"
			}
		);
	}
}
