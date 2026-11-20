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
	const FString RelDir = Settings ? Settings->SaveDirectoryRelative : TEXT("WcwSnapJump");

#if PLATFORM_DESKTOP || WITH_EDITOR
	// On Desktop/Editor, full OS path is convenient for Editor tools and Explorer inspection
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / RelDir);
#else
	// On Console platforms (PS5 / Xbox / Switch), do NOT call ConvertRelativePathToFull!
	// Console IPlatformFile relies on the virtual "../../../<Project>/Saved/" path to route writes
	// to Devkit HostFS or the writable Saved partition (just like Saved/Logs and FPaths::ScreenShotDir()).
	const FString PrimarySavedPath = FPaths::ProjectSavedDir() / RelDir;
	if (IFileManager::Get().DirectoryExists(*PrimarySavedPath) || IFileManager::Get().MakeDirectory(*PrimarySavedPath, true))
	{
		return PrimarySavedPath;
	}

	// Fallback to writable persistent download/cache storage if ProjectSavedDir is read-only (e.g. detached console run)
	return FPaths::ProjectPersistentDownloadDir() / RelDir;
#endif
}
