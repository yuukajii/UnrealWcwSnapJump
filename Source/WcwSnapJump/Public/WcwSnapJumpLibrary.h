// Copyright (c) 2026 WinterCrownWORKS. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WcwSnapJumpTypes.h"
#include "WcwSnapJumpLibrary.generated.h"

class UTexture2D;
class UDataTable;

/**
 * Blueprint & C++ Function Library for capturing, reading, formatting, and jumping with WcwSnapJump.
 */
UCLASS()
class WCWSNAPJUMP_API UWcwSnapJumpLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Capture a custom PNG snapshot with embedded metadata from Blueprint or UI. */
	UFUNCTION(BlueprintCallable, Category = "Wcw|SnapJump", meta = (WorldContext = "WorldContextObject"))
	static void CaptureWcwSnap(UObject* WorldContextObject, const FString& CaptureTag = TEXT("QA"));

	/** Set or update a persistent custom key-value tag (e.g. QuestPhase, ActiveCutscene) included in subsequent snapshots. */
	UFUNCTION(BlueprintCallable, Category = "Wcw|SnapJump", meta = (WorldContext = "WorldContextObject"))
	static void SetWcwCustomTitleTag(UObject* WorldContextObject, const FString& Key, const FString& Value);

	/** Read a custom key-value tag from FWcwSnapJumpData cleanly in Blueprint or C++. */
	UFUNCTION(BlueprintPure, Category = "Wcw|SnapJump")
	static bool GetWcwCustomTitleTag(const FWcwSnapJumpData& SnapData, const FString& Key, FString& OutValue);

	/**
	 * Notify WcwSnapJumpSubsystem that your Title's custom Async Level Loader has finished loading sublevels/actors,
	 * triggering pending player teleport, `HandlePostJumpSetup`, and `OnJumpRequested`!
	 */
	UFUNCTION(BlueprintCallable, Category = "Wcw|SnapJump|Jump", meta = (WorldContext = "WorldContextObject"))
	static void NotifyWcwLevelLoadCompleted(UObject* WorldContextObject);

	/** Read embedded FWcwSnapJumpData directly from a PNG file on disk (without decoding image pixels). */
	UFUNCTION(BlueprintCallable, Category = "Wcw|SnapJump")
	static bool ReadSnapDataFromPngFile(const FString& PngFilePath, FWcwSnapJumpData& OutSnapData);

	/** Load a PNG file from disk into a transient UTexture2D for thumbnail display, AND extract its embedded metadata. */
	UFUNCTION(BlueprintCallable, Category = "Wcw|SnapJump")
	static UTexture2D* LoadSnapThumbnailAndData(const FString& PngFilePath, FWcwSnapJumpData& OutSnapData, bool& bOutMetaValid);

	/** Scan Saved/WcwSnapJump (or a custom directory) and return all PNG files containing WcwSnapJump metadata. */
	UFUNCTION(BlueprintCallable, Category = "Wcw|SnapJump")
	static TArray<FWcwSnapJumpData> ScanSnapDirectory(const FString& OptionalDirectoryPath = TEXT(""));

	/** Retrieve all rows from a WcwSnapJump DataTable, resolving PNG metadata if CachedSnapData is empty. */
	UFUNCTION(BlueprintCallable, Category = "Wcw|SnapJump")
	static TArray<FWcwSnapJumpTableRow> GetResolvedRowsFromDataTable(UDataTable* DataTable);

	/** Jump to the level, character transform, and custom state stored inside FWcwSnapJumpData. */
	UFUNCTION(BlueprintCallable, Category = "Wcw|SnapJump", meta = (WorldContext = "WorldContextObject"))
	static bool JumpToSnapData(UObject* WorldContextObject, const FWcwSnapJumpData& SnapData, bool bAutoLoadLevelIfDifferent = true, bool bAutoTeleportPlayer = true);

	/** Format compact thumbnail summary (delegates to CustomJumpHandlerClass so Title overrides work in both Editor & Runtime!). */
	UFUNCTION(BlueprintPure, Category = "Wcw|SnapJump")
	static FString FormatCompactThumbnailSummary(const FWcwSnapJumpData& SnapData);

	/** Format detailed telemetry & Title state report (delegates to CustomJumpHandlerClass so Title overrides work in both Editor & Runtime!). */
	UFUNCTION(BlueprintPure, Category = "Wcw|SnapJump")
	static FString FormatDetailedTelemetryReport(const FWcwSnapJumpData& SnapData);

	/** Invoke CustomJumpHandlerClass::GatherIssueTrackerUiFields to build interactive Title-specific UI controls in Wcw SnapJump Viewer. */
	UFUNCTION(BlueprintCallable, Category = "Wcw|SnapJump|IssueTracker")
	static void GatherIssueTrackerUiFields(const FWcwSnapJumpData& SnapData, TArray<FWcwIssueTrackerUiField>& OutUiFields);

	/** Format auto-prefixed Issue Tracker / Redmine ticket Subject via CustomJumpHandlerClass. */
	UFUNCTION(BlueprintPure, Category = "Wcw|SnapJump|IssueTracker")
	static FString FormatIssueTrackerSubject(const FWcwSnapJumpData& SnapData, const FString& UserEnteredTitle);

	/** Format Issue Tracker / Redmine telemetry description block via CustomJumpHandlerClass. */
	UFUNCTION(BlueprintPure, Category = "Wcw|SnapJump|IssueTracker")
	static FString FormatIssueTrackerTelemetryBlock(const FWcwSnapJumpData& SnapData);

	/** Invoke CustomJumpHandlerClass::GatherIssueTrackerPresetSelection for auto-selecting Tracker/Category/Assignee/ParentIssueId. */
	UFUNCTION(BlueprintCallable, Category = "Wcw|SnapJump|IssueTracker")
	static void GatherIssueTrackerPresetSelection(const FWcwSnapJumpData& SnapData, FWcwIssueTrackerPresetSelection& OutPresetSelection);

	/** Invoke CustomJumpHandlerClass::GatherIssueTrackerIssueFields to collect Title-specific standard Redmine issue fields. */
	UFUNCTION(BlueprintCallable, Category = "Wcw|SnapJump|IssueTracker")
	static void GatherIssueTrackerIssueFields(const FWcwSnapJumpData& SnapData, UPARAM(ref) TMap<FString, FString>& InOutIssueFields);

	/** Invoke CustomJumpHandlerClass::GatherIssueTrackerCustomFields to collect Title-specific Redmine custom fields. */
	UFUNCTION(BlueprintCallable, Category = "Wcw|SnapJump|IssueTracker")
	static void GatherIssueTrackerCustomFields(const FWcwSnapJumpData& SnapData, UPARAM(ref) TMap<FString, FString>& InOutCustomFields);
};
