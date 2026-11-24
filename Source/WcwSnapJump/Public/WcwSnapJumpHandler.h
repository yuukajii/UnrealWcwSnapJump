// Copyright (c) 2026 WinterCrownWORKS. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "WcwSnapJumpTypes.h"
#include "WcwSnapJumpHandler.generated.h"

/**
 * Unified Title Customization Handler for WcwSnapJump (Metadata Collection, UI Formatting, Level Loading, StreamLevel Restoration, Post-Jump Setup & Issue Tracker Overrides).
 *
 * Inherit from this class in C++ or Blueprint and assign it to:
 *   Project Settings -> Extension -> Wcw Snap Jump -> CustomJumpHandlerClass
 *
 * What you can Override:
 *   [1] Metadata & Stat/GPU Collection:
 *       - GatherSnapData                     : Replace the entire snapshot metadata collection flow from scratch.
 *       - GatherStatAndGpuTelemetry          : Replace or customize `stat unit`, `stat memory`, and `profileGPU` Top 10 collection.
 *       - OnGatherTitleCustomMeta            : Append Title-specific Event Progress, Quest Flags, or Cutscene playback state into `CustomTitleTags` / `CustomJsonPayload`.
 *   [2] UI Display Formatting (applies to both Runtime Browser & Editor Viewer):
 *       - FormatThumbnailSummary             : Customize the 3-4 line summary shown next to each screenshot thumbnail.
 *       - FormatDetailedReport               : Customize the full detail report shown in the inspector pane.
 *   [3] Level Loading, StreamLevel Restoration & Jump / Event Playback:
 *       - ShouldTransitionLevel              : Decide whether a map load/reload is needed (automatically keeps PersistentLevel alive if target is a registered StreamLevel).
 *       - HandleLevelTransition              : Replace `UGameplayStatics::OpenLevel` with your Title's custom Fade / Async Level Loader / World Partition manager.
 *       - RestoreStreamingLevels             : Load & make visible recorded StreamLevels (stripping `UEDPIE_`, skipping `/Memory/`, and flushing collision before teleport).
 *       - HandleSameLevelTeleport            : Customize how the Player Character / Camera is teleported.
 *       - HandlePostJumpSetup                : Executed right after teleport OR after level load completes -- restore Event flags, spawn NPCs, or start Cutscene playback here!
 *   [4] Issue Tracker / Redmine Ticket & UI Overrides (v0.2.0+, zero Runtime HTTP/Slate dependency):
 *       - GatherIssueTrackerUiFields         : Add interactive Priority, Custom Field (Text/Dropdown/MultiSelect/CheckBox), or Standard Issue controls to Wcw SnapJump Viewer!
 *       - FormatIssueTrackerSubject          : Customize auto-prefixed ticket Subject rules (e.g. "[PS5][LevelName][Tag] ...").
 *       - FormatIssueTrackerTelemetryBlock   : Customize the telemetry summary block (Textile <pre> or Markdown ```) appended to ticket descriptions.
 *       - GatherIssueTrackerPresetSelection  : Auto-select Tracker, Category, Assignee, or Parent Issue ID in the Editor UI per snapshot.
 *       - GatherIssueTrackerIssueFields      : Inject standard Redmine `issue` fields (e.g. "priority_id", "fixed_version_id", "is_private").
 *       - GatherIssueTrackerCustomFields     : Populate Redmine `custom_fields` (Field Name or Numeric ID -> Value) dynamically from `SnapData`.
 */
UCLASS(Blueprintable, BlueprintType)
class WCWSNAPJUMP_API UWcwSnapJumpHandler : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * If true (default), UWcwSnapJumpSubsystem automatically triggers RestoreStreamingLevels, HandleSameLevelTeleport & HandlePostJumpSetup
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
	 * Default implementation gathers normalized Level & StreamLevel paths, Player/Camera Transform, calls `GatherStatAndGpuTelemetry`, and calls `OnGatherTitleCustomMeta`.
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
	// [3] Level Loading, StreamLevel Restoration, Teleportation & Post-Jump Overrides
	// =========================================================================

	/**
	 * Top-level jump execution entry point invoked by `UWcwSnapJumpSubsystem::JumpToSnapLocation`.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wcw|SnapJump|Jump")
	bool ExecuteJump(UObject* WorldContextObject, const FWcwSnapJumpData& SnapData, bool bAutoLoadLevel, bool bAutoTeleportPlayer);
	virtual bool ExecuteJump_Implementation(UObject* WorldContextObject, const FWcwSnapJumpData& SnapData, bool bAutoLoadLevel, bool bAutoTeleportPlayer);

	/**
	 * Determines whether a level load/transition is required.
	 * Default compares normalized `LevelName` / `LevelPackageName` (stripping `UEDPIE_`), and returns false when the target level
	 * is already a registered `ULevelStreaming` inside the active `PersistentLevel` (when `bTreatRegisteredStreamLevelAsSameWorld` is enabled).
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wcw|SnapJump|Jump")
	bool ShouldTransitionLevel(UWorld* World, const FWcwSnapJumpData& SnapData);
	virtual bool ShouldTransitionLevel_Implementation(UWorld* World, const FWcwSnapJumpData& SnapData);

	/**
	 * Called when `ShouldTransitionLevel` returns true.
	 * Default normalizes `UEDPIE_` prefixes away and calls `UGameplayStatics::OpenLevel`.
	 * Override this to invoke your Title's custom Level Loader / Fade Manager / Streaming Manager, passing Title-specific parameters from `SnapData`!
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wcw|SnapJump|Jump")
	bool HandleLevelTransition(UWorld* World, const FWcwSnapJumpData& SnapData);
	virtual bool HandleLevelTransition_Implementation(UWorld* World, const FWcwSnapJumpData& SnapData);

	/**
	 * Restores (loads & makes visible) streaming sublevels recorded in `SnapData.LoadedStreamingLevels`
	 * (plus `SnapData.LevelPackageName` if jumping into a registered sublevel of the active PersistentLevel).
	 * Strips `UEDPIE_` prefixes, skips `/Memory/` & `/_Generated_/` transient cells, and flushes level streaming / physics collision
	 * before player teleportation when `bAutoRestoreStreamingLevels` and `bBlockOnStreamLevelLoadForJump` are enabled in Project Settings.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wcw|SnapJump|Jump")
	bool RestoreStreamingLevels(UWorld* World, const FWcwSnapJumpData& SnapData);
	virtual bool RestoreStreamingLevels_Implementation(UWorld* World, const FWcwSnapJumpData& SnapData);

	/**
	 * Applies Character Location/Rotation and Controller Rotation once in the target level.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wcw|SnapJump|Jump")
	bool HandleSameLevelTeleport(UWorld* World, const FWcwSnapJumpData& SnapData);
	virtual bool HandleSameLevelTeleport_Implementation(UWorld* World, const FWcwSnapJumpData& SnapData);

	/**
	 * Called right after `HandleSameLevelTeleport` (both on immediate same-level jumps AND after a cross-level load finishes!).
	 * Override this to restore Event Progress flags, load custom dynamic sublevels, spawn enemies, or play a Cutscene (`ULevelSequence`)!
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wcw|SnapJump|Jump")
	void HandlePostJumpSetup(UWorld* World, const FWcwSnapJumpData& SnapData);
	virtual void HandlePostJumpSetup_Implementation(UWorld* World, const FWcwSnapJumpData& SnapData);

	// =========================================================================
	// [4] Issue Tracker / Redmine Ticket & UI Overrides (Editor Ticket Posting, v0.2.0+)
	// =========================================================================

	/**
	 * Declares interactive Title-specific UI controls (Priority dropdown, Custom Field text boxes, dropdowns, multi-selects, or checkboxes)
	 * rendered dynamically inside Wcw SnapJump Viewer's Redmine Ticket Post tab!
	 * Default returns an empty array (only standard controls are shown). Override in C++ or Blueprint to give QA/artists
	 * per-Title editable UI controls whose values are submitted directly on Post.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wcw|SnapJump|IssueTracker")
	void GatherIssueTrackerUiFields(const FWcwSnapJumpData& SnapData, TArray<FWcwIssueTrackerUiField>& OutUiFields);
	virtual void GatherIssueTrackerUiFields_Implementation(const FWcwSnapJumpData& SnapData, TArray<FWcwIssueTrackerUiField>& OutUiFields);

	/**
	 * Formats the auto-prefixed ticket Subject when `Auto-prefix subject` is enabled in Wcw SnapJump Viewer.
	 * Default returns `"[CaptureTag] " + UserEnteredTitle`. Override to include Platform, LevelName, QuestPhase, etc.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wcw|SnapJump|IssueTracker")
	FString FormatIssueTrackerSubject(const FWcwSnapJumpData& SnapData, const FString& UserEnteredTitle);
	virtual FString FormatIssueTrackerSubject_Implementation(const FWcwSnapJumpData& SnapData, const FString& UserEnteredTitle);

	/**
	 * Formats the telemetry summary block appended below the user's Description when posting a ticket.
	 * Default wraps Environment, Transform, Stats, GPU Top 5, and CustomTitleTags in Redmine `<pre>...</pre>`.
	 * Override to customize Markdown code fences, Japanese labels, or Title-specific reproduction notes.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wcw|SnapJump|IssueTracker")
	FString FormatIssueTrackerTelemetryBlock(const FWcwSnapJumpData& SnapData);
	virtual FString FormatIssueTrackerTelemetryBlock_Implementation(const FWcwSnapJumpData& SnapData);

	/**
	 * Called when a snapshot row is selected in Wcw SnapJump Viewer so your Title can automatically pre-select
	 * Tracker, Category, Assignee, or Parent Issue ID based on `SnapData` (e.g. routing by LevelName or FPS).
	 * Users can still manually adjust the dropdowns in the Editor UI before clicking Post.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wcw|SnapJump|IssueTracker")
	void GatherIssueTrackerPresetSelection(const FWcwSnapJumpData& SnapData, FWcwIssueTrackerPresetSelection& OutPresetSelection);
	virtual void GatherIssueTrackerPresetSelection_Implementation(const FWcwSnapJumpData& SnapData, FWcwIssueTrackerPresetSelection& OutPresetSelection);

	/**
	 * Called before creating a Redmine ticket to supply or override standard Redmine `issue` JSON fields
	 * (e.g. "priority_id" -> "5", "fixed_version_id" -> "2", "status_id" -> "1", "is_private" -> "true", "estimated_hours" -> "1.5").
	 * Values written into `InOutIssueFields` override matching keys in `UWcwSnapJumpRedmineSettings::DefaultExtraIssueFields`.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wcw|SnapJump|IssueTracker")
	void GatherIssueTrackerIssueFields(const FWcwSnapJumpData& SnapData, UPARAM(ref) TMap<FString, FString>& InOutIssueFields);
	virtual void GatherIssueTrackerIssueFields_Implementation(const FWcwSnapJumpData& SnapData, TMap<FString, FString>& InOutIssueFields);

	/**
	 * Called by WcwSnapJumpEditor before creating a Redmine ticket so your Title can dynamically supply
	 * required or optional Redmine `custom_fields` (Key: Redmine Custom Field Name such as "CustomFieldTest"
	 * or Numeric ID string such as "1"; Value: String value, e.g. BuildVersion / Changelist / QuestId from `SnapData.CustomTitleTags`).
	 * Values written into `InOutCustomFields` override matching keys in `UWcwSnapJumpRedmineSettings::DefaultCustomFields`.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wcw|SnapJump|IssueTracker")
	void GatherIssueTrackerCustomFields(const FWcwSnapJumpData& SnapData, UPARAM(ref) TMap<FString, FString>& InOutCustomFields);
	virtual void GatherIssueTrackerCustomFields_Implementation(const FWcwSnapJumpData& SnapData, TMap<FString, FString>& InOutCustomFields);
};
