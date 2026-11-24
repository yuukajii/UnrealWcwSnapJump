// Copyright (c) 2026 WinterCrownWORKS. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "WcwSnapJumpHandler.h"
#include "WcwSnapJumpSettings.generated.h"

/**
 * Project Settings for WcwSnapJump plugin (placed under Extension category per WCW conventions).
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Wcw Snap Jump"))
class WCWSNAPJUMP_API UWcwSnapJumpSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UWcwSnapJumpSettings();

	//~ Begin UDeveloperSettings Interface
	virtual FName GetCategoryName() const override { return FName(TEXT("Extension")); }
	//~ End UDeveloperSettings Interface

	/** Master fallback toggle to enable/disable WcwSnapJump capture & runtime jump features */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "General")
	bool bEnableWcwSnapJump = true;

	/** Relative directory inside Project/Saved/ where custom PNG snapshots are stored */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Capture")
	FString SaveDirectoryRelative = TEXT("WcwSnapJump");

	/** Subfolder name created next to the target UDataTable when importing textures for Packaged Builds */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Capture")
	FString ImportedTextureSubfolderName = TEXT("CustomScreenShot");

	/** Saved PNG width in pixels (default 512 for lightweight snapshot thumbnails) */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Capture", meta = (ClampMin = "64", ClampMax = "4096"))
	int32 ScreenshotWidth = 512;

	/** Saved PNG height in pixels (default 512 for lightweight snapshot thumbnails) */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Capture", meta = (ClampMin = "64", ClampMax = "4096"))
	int32 ScreenshotHeight = 512;

	/** Whether to capture Slate UI in the custom screenshot */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Capture")
	bool bShowUIInScreenshot = false;

	/** Also write a companion .json sidecar file next to the .png for external QA scripts */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Capture")
	bool bWriteJsonSidecar = true;

	/**
	 * Exclude runtime-generated transient streaming cells (such as World Partition /Memory/UEDPIE_... and /_Generated_/ cells)
	 * when recording LoadedStreamingLevels so only real sublevel assets (/Game/..., plugin mounts) are stored.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Capture")
	bool bExcludeTransientStreamingCells = true;

	/** Number of top GPU passes to record (default: 10) */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Telemetry", meta = (ClampMin = "1", ClampMax = "50"))
	int32 MaxGpuTopPasses = 10;

	/** Optional default DataTable for runtime snapshot gallery (auto-created at /Game/WcwSnapJump/DT_WcwSnapJump if unset) */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "RuntimeJump", meta = (AllowedClasses = "/Script/Engine.DataTable"))
	FSoftObjectPath DefaultSnapJumpDataTable;

	/**
	 * Automatically load and make visible recorded StreamLevels (stripping UEDPIE_ prefixes and skipping /Memory/ transient cells)
	 * before teleporting the player. Turn OFF if your Title's own progression manager exclusively controls sublevel streaming.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "RuntimeJump")
	bool bAutoRestoreStreamingLevels = true;

	/**
	 * Synchronously flush pending StreamLevel loading & physics collision registration (BlockTillLevelStreamingCompleted)
	 * before teleporting the player, preventing the character from falling through unloaded sublevel floors.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "RuntimeJump", meta = (EditCondition = "bAutoRestoreStreamingLevels"))
	bool bBlockOnStreamLevelLoadForJump = true;

	/**
	 * If a snapshot's primary level matches a registered ULevelStreaming inside the currently active PersistentLevel,
	 * keep the PersistentLevel alive and stream/show that sublevel in-place instead of calling OpenLevel.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "RuntimeJump", meta = (EditCondition = "bAutoRestoreStreamingLevels"))
	bool bTreatRegisteredStreamLevelAsSameWorld = true;

	/**
	 * When jumping in the non-PIE Editor viewport via Wcw SnapJump Viewer, automatically make matching loaded sublevels
	 * visible in the Editor viewport (without dirtying map packages).
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "RuntimeJump", meta = (EditCondition = "bAutoRestoreStreamingLevels"))
	bool bAutoShowStreamingLevelsInEditorJump = true;

	/**
	 * Pluggable Title Customization & Jump Handler Class.
	 * Defaults to UWcwSnapJumpHandler. Point this to your Title's custom C++ or Blueprint subclass
	 * to customize Stat/GPU/Event/Cutscene gathering, UI text formatting, and Level Load / Post-Jump playback!
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "RuntimeJump")
	TSoftClassPtr<UWcwSnapJumpHandler> CustomJumpHandlerClass;

	/** Helper to return resolved absolute directory path for saved PNG snapshots */
	UFUNCTION(BlueprintPure, Category = "Wcw|SnapJump")
	static FString GetResolvedScreenshotDirectory();
};
