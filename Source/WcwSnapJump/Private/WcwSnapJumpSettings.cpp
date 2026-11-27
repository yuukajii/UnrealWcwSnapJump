// Copyright (c) 2026 WinterCrownWORKS. All Rights Reserved.
// Licensed under the MIT License.

#include "WcwSnapJumpSettings.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

UWcwSnapJumpSettings::UWcwSnapJumpSettings()
{
	CategoryName = TEXT("Extension");
	SectionName = TEXT("WcwSnapJump");
	CustomJumpHandlerClass = UWcwSnapJumpHandler::StaticClass();
}

FString UWcwSnapJumpSettings::GetResolvedScreenshotDirectory()
{
	const UWcwSnapJumpSettings* Settings = GetDefault<UWcwSnapJumpSettings>();
	const FString RelDir = (Settings && !Settings->SaveDirectoryRelative.IsEmpty())
		? Settings->SaveDirectoryRelative
		: TEXT("WcwSnapJump");

#if PLATFORM_DESKTOP || WITH_EDITOR
	const FString FullDir = FPaths::ConvertRelativePathToFull(FPaths::ScreenShotDir() / RelDir);
	IFileManager::Get().MakeDirectory(*FullDir, true);
	return FullDir;
#else
	IFileManager& FM = IFileManager::Get();
	FM.MakeDirectory(*FPaths::ScreenShotDir(), false);

	FString TargetDir = FPaths::ScreenShotDir() / RelDir;
	while (TargetDir.Len() > 1 && (TargetDir.EndsWith(TEXT("/")) || TargetDir.EndsWith(TEXT("\\"))))
	{
		TargetDir.LeftChopInline(1);
	}

	if (!FM.DirectoryExists(*TargetDir))
	{
		if (!FM.MakeDirectory(*TargetDir, false))
		{
			FM.MakeDirectory(*TargetDir, true);
		}
	}
	return TargetDir;
#endif
}
