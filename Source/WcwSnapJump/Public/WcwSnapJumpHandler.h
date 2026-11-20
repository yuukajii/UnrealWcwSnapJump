// Copyright (c) 2026 WinterCrownWORKS. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "WcwSnapJumpTypes.h"
#include "WcwSnapJumpHandler.generated.h"

/**
 * Unified Title Customization Handler for WcwSnapJump (Metadata Collection, UI Formatting, Level Loading & Post-Jump Setup).
 *
 * Inherit from this class in C++ or Blueprint and assign it to:
 *   Project Settings -> Extension -> Wcw Snap Jump -> CustomJumpHandlerClass
 *
 * What you can Override:
 *   [1] Metadata & Stat/GPU Collection:
 *       - GatherSnapData              : Replace the entire snapshot metadata collection flow from scratch.
 *       - GatherStatAndGpuTelemetry   : Replace or customize `stat unit`, `stat memory`, and `profileGPU` Top 10 collection.
 *       - OnGatherTitleCustomMeta     : Append Title-specific Event Progress, Quest Flags, or Cutscene playback state into `CustomTitleTags` / `CustomJsonPayload`.
 *   [2] UI Display Formatting (applies to both Runtime Browser & Editor Viewer):
 *       - FormatThumbnailSummary      : Customize the 3-4 line summary shown next to each screenshot thumbnail.
 *       - FormatDetailedReport        : Customize the full detail report shown in the inspector pane.
 *   [3] Level Loading & Jump / Event Playback:
 *       - ShouldTransitionLevel       : Decide whether a map load/reload is needed (e.g. force reload when resetting a cutscene).
 *       - HandleLevelTransition       : Replace `UGameplayStatics::OpenLevel` with your Title's custom Fade / Async Level Loader / World Partition manager.
 *       - HandleSameLevelTeleport     : Customize how the Player Character / Camera is teleported.
 *       - HandlePostJumpSetup         : Executed right after teleport OR after level load completes — restore Event flags, spawn NPCs, or start Cutscene playback here!
 */
UCLASS(Blueprintable, BlueprintType)
class WCWSNAPJUMP_API UWcwSnapJumpHandler : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * If true (default), UWcwSnapJumpSubsystem automatically triggers HandleSameLevelTeleport & HandlePostJumpSetup
	 * as soon as Unreal's `FCoreUObjectDelegates::PostLoadMapWithWorld` fires.
	 *
	 * Set this to FALSE if your Title uses a custom multi-stage Async Level Loader (e.g. waiting for Streaming Sublevels,
	 * World Partition cells, or Fade-In), and call `UWcwSnapJumpLibrary::NotifyWcwLevelLoadCompleted(this)` when ready!
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Wcw|SnapJump|Jump")
	bool bAutoApplyOnEnginePostLoadMap = true;

	// =========================================================================
	// [1] Metadata, Stat/GPU & Title Event/Cutscene Info Gathering Overrides
	// =========================================================================

	/**
	 * Master telemetry & metadata gathering entry point called when `Wcw.SnapJump.Shot` is executed.
	 * Default implementation gathers Level, Player/Camera Transform, calls `GatherStatAndGpuTelemetry`, and calls `OnGatherTitleCustomMeta`.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wcw|SnapJump|Gather")
	void GatherSnapData(UObject* WorldContextObject, const FString& CaptureTag, FWcwSnapJumpData& OutSnapData);
	virtual void GatherSnapData_Implementation(UObject* WorldContextObject, const FString& CaptureTag, FWcwSnapJumpData& OutSnapData);

	/**
	 * Gathers Frame Stats (`stat unit`), Memory (`stat memory`), and GPU Top Passes (`stat gpu` / `profileGPU`).
	 * Override this in C++ or Blueprint to plug in your Title's own GPU profiler, RDG pass timings, or clear GPU stats if unused.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wcw|SnapJump|Gather")
	void GatherStatAndGpuTelemetry(UObject* WorldContextObject, FWcwStatSnapshot& OutStats, FWcwMemorySnapshot& OutMemory, TArray<FWcwGpuPassEntry>& OutGpuPasses);
	virtual void GatherStatAndGpuTelemetry_Implementation(UObject* WorldContextObject, FWcwStatSnapshot& OutStats, FWcwMemorySnapshot& OutMemory, TArray<FWcwGpuPassEntry>& OutGpuPasses);

	/**
	 * Called at the end of `GatherSnapData` so your Title can write Event Progress, Quest IDs, Cutscene Asset & Playback Frame,
	 * Time-of-Day, Weather, etc., into `InOutSnapData.CustomTitleTags` or `InOutSnapData.CustomJsonPayload`.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wcw|SnapJump|Gather")
	void OnGatherTitleCustomMeta(UObject* WorldContextObject, UPARAM(ref) FWcwSnapJumpData& InOutSnapData);
	virtual void OnGatherTitleCustomMeta_Implementation(UObject* WorldContextObject, FWcwSnapJumpData& InOutSnapData);

	// =========================================================================
	// [2] UI Text Formatting Overrides (Runtime Browser & Editor Viewer)
	// =========================================================================

	/**
	 * Formats the compact multi-line summary displayed next to each thumbnail in the Browser UI.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wcw|SnapJump|Format")
	FString FormatThumbnailSummary(const FWcwSnapJumpData& SnapData);
	virtual FString FormatThumbnailSummary_Implementation(const FWcwSnapJumpData& SnapData);

	/**
	 * Formats the full detailed report shown in the detail pane of both the Runtime Browser and Editor Viewer.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wcw|SnapJump|Format")
	FString FormatDetailedReport(const FWcwSnapJumpData& SnapData);
	virtual FString FormatDetailedReport_Implementation(const FWcwSnapJumpData& SnapData);

	// =========================================================================
	// [3] Level Loading, Teleportation & Post-Jump Event/Cutscene Overrides
	// =========================================================================

	/**
	 * Top-level jump execution entry point invoked by `UWcwSnapJumpSubsystem::JumpToSnapLocation`.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wcw|SnapJump|Jump")
	bool ExecuteJump(UObject* WorldContextObject, const FWcwSnapJumpData& SnapData, bool bAutoLoadLevel, bool bAutoTeleportPlayer);
	virtual bool ExecuteJump_Implementation(UObject* WorldContextObject, const FWcwSnapJumpData& SnapData, bool bAutoLoadLevel, bool bAutoTeleportPlayer);

	/**
	 * Determines whether a level load/transition is required.
	 * Default returns true when `CurrentLevelName != SnapData.LevelName`.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wcw|SnapJump|Jump")
	bool ShouldTransitionLevel(UWorld* World, const FWcwSnapJumpData& SnapData);
	virtual bool ShouldTransitionLevel_Implementation(UWorld* World, const FWcwSnapJumpData& SnapData);

	/**
	 * Called when `ShouldTransitionLevel` returns true.
	 * Default calls `UGameplayStatics::OpenLevel`.
	 * Override this to invoke your Title's custom Level Loader / Fade Manager / Streaming Manager, passing Title-specific parameters from `SnapData`!
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wcw|SnapJump|Jump")
	bool HandleLevelTransition(UWorld* World, const FWcwSnapJumpData& SnapData);
	virtual bool HandleLevelTransition_Implementation(UWorld* World, const FWcwSnapJumpData& SnapData);

	/**
	 * Applies Character Location/Rotation and Controller Rotation once in the target level.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wcw|SnapJump|Jump")
	bool HandleSameLevelTeleport(UWorld* World, const FWcwSnapJumpData& SnapData);
	virtual bool HandleSameLevelTeleport_Implementation(UWorld* World, const FWcwSnapJumpData& SnapData);

	/**
	 * Called right after `HandleSameLevelTeleport` (both on immediate same-level jumps AND after a cross-level load finishes!).
	 * Override this to restore Event Progress flags, load sublevels, spawn enemies, or play a Cutscene (`ULevelSequence`)!
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wcw|SnapJump|Jump")
	void HandlePostJumpSetup(UWorld* World, const FWcwSnapJumpData& SnapData);
	virtual void HandlePostJumpSetup_Implementation(UWorld* World, const FWcwSnapJumpData& SnapData);
};
