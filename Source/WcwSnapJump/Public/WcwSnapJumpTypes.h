// Copyright (c) 2026 WinterCrownWORKS. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "WcwSnapJumpTypes.generated.h"

/**
 * Capture platform, OS/hardware (GPU/CPU), RHI, build configuration, and render resolution snapshot.
 */
USTRUCT(BlueprintType)
struct WCWSNAPJUMP_API FWcwEnvironmentSnapshot
{
	GENERATED_BODY()

	/** Platform identifier (e.g. Windows, PS5, XSX, Switch) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	FString PlatformName;

	/** Build configuration & runtime context (e.g. Development (Editor/PIE), Test (Packaged)) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	FString BuildEnvironment;

	/** Active GPU adapter name (e.g. NVIDIA GeForce RTX 4080) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	FString GpuAdapterName;

	/** User-facing GPU driver version if available on PC */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	FString GpuDriverVersion;

	/** CPU brand string (e.g. AMD Ryzen 9 7950X 16-Core Processor) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	FString CpuBrand;

	/** Active Graphics RHI name (e.g. D3D12, Vulkan) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	FString RhiName;

	/** Original viewport pixel resolution at capture time (e.g. 1920x1080, 3840x2160) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	FString ViewportResolution;
};

/**
 * Single GPU pass or custom profiling entry for Top-N load inspection.
 */
USTRUCT(BlueprintType)
struct WCWSNAPJUMP_API FWcwGpuPassEntry
{
	GENERATED_BODY()

	/** Pass or event name (e.g. BasePass, ShadowDepths, PostProcessing) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	FString PassName;

	/** GPU execution time in milliseconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	float DurationMs = 0.0f;

	/** Percentage of total GPU frame time */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	float PercentageOfFrame = 0.0f;
};

/**
 * Frame stat and timing snapshot (`stat unit` equivalent).
 */
USTRUCT(BlueprintType)
struct WCWSNAPJUMP_API FWcwStatSnapshot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	float FPS = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	float FrameTimeMs = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	float GameThreadTimeMs = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	float RenderThreadTimeMs = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	float GpuTimeMs = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	float RhiThreadTimeMs = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	int32 DrawCalls = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	int32 PrimitivesDrawn = 0;
};

/**
 * System & VRAM memory snapshot (`stat memory` / RHI texture memory equivalent).
 */
USTRUCT(BlueprintType)
struct WCWSNAPJUMP_API FWcwMemorySnapshot
{
	GENERATED_BODY()

	/** Process used physical RAM in MB */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	float UsedPhysicalMemoryMB = 0.0f;

	/** Peak physical RAM in MB */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	float PeakPhysicalMemoryMB = 0.0f;

	/** Process used virtual memory in MB */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	float UsedVirtualMemoryMB = 0.0f;

	/** Total available physical memory in MB */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	float AvailablePhysicalMemoryMB = 0.0f;

	/** RHI Texture Pool / Streaming VRAM used in MB */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	float TextureMemoryUsedMB = 0.0f;

	/** RHI RenderTarget VRAM used in MB */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	float RenderTargetMemoryUsedMB = 0.0f;
};

/**
 * Complete snapshot & telemetry payload embedded directly inside a Wcw PNG screenshot (and DataTable row).
 */
USTRUCT(BlueprintType)
struct WCWSNAPJUMP_API FWcwSnapJumpData
{
	GENERATED_BODY()

	/** Unique identifier or user tag for the capture */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	FString CaptureTag;

	/** ISO-8601 capture timestamp */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	FString Timestamp;

	/** Platform, GPU/CPU hardware, RHI, build configuration, and original render resolution */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	FWcwEnvironmentSnapshot Environment;

	/** Persistent level path (e.g. /Game/Maps/MainLevel) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	FString LevelPackageName;

	/** Short display name of the current level */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	FString LevelName;

	/** Names of currently loaded streaming sublevels */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	TArray<FString> LoadedStreamingLevels;

	/** Player Pawn or Character world location */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	FVector CharacterLocation = FVector::ZeroVector;

	/** Player Pawn or Character world rotation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	FRotator CharacterRotation = FRotator::ZeroRotator;

	/** Player Controller control rotation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	FRotator ControlRotation = FRotator::ZeroRotator;

	/** Active Camera world location */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	FVector CameraLocation = FVector::ZeroVector;

	/** Active Camera world rotation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	FRotator CameraRotation = FRotator::ZeroRotator;

	/** Active Camera FOV */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	float CameraFOV = 90.0f;

	/** Frame performance stats (can be customized/replaced by Title) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	FWcwStatSnapshot Stats;

	/** Memory usage stats (can be customized/replaced by Title) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	FWcwMemorySnapshot Memory;

	/** Top N GPU passes sorted descending by DurationMs (can be customized/replaced by Title) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	TArray<FWcwGpuPassEntry> GpuTop10Passes;

	/** Arbitrary game-specific key-value map for Event Progress, Cutscene ID/Time, Weather, Quest Flags, etc. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	TMap<FString, FString> CustomTitleTags;

	/** Optional free-form JSON string for serializing arbitrary Title USTRUCTs */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	FString CustomJsonPayload;

	/** Absolute or project-relative path to the PNG file (populated on capture/load) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	FString SourceFilePath;
};

/**
 * Optional Title-side auto-selection presets applied in Wcw SnapJump Viewer when a snapshot row is selected (v0.2.0+).
 * Leave any field empty / 0 to preserve the user's current UI selection.
 */
USTRUCT(BlueprintType)
struct WCWSNAPJUMP_API FWcwIssueTrackerPresetSelection
{
	GENERATED_BODY()

	/** Tracker name (e.g. "Bug") or numeric ID string (e.g. "1") to pre-select in the Editor UI */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump|IssueTracker")
	FString PreferredTracker;

	/** Issue Category name (e.g. "BG_Downtown") or numeric ID string to pre-select in the Editor UI */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump|IssueTracker")
	FString PreferredCategory;

	/** Assignee display name / login substring or numeric User/Group ID string to pre-select in the Editor UI */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump|IssueTracker")
	FString PreferredAssignee;

	/** Parent Issue ID (> 0) to pre-fill in the Editor UI when selecting this snapshot (0 = keep current) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump|IssueTracker")
	int32 PreferredParentIssueId = 0;
};

/**
 * Widget control type rendered inside Wcw SnapJump Viewer for Title-defined Redmine fields (v0.2.0+).
 */
UENUM(BlueprintType)
enum class EWcwIssueTrackerUiControlType : uint8
{
	/** Editable single-line text input (e.g. Redmine Text / String / Integer Custom Field) */
	TextBox UMETA(DisplayName = "Text Box"),

	/** Single-selection combo box dropdown (e.g. Redmine Priority or List Custom Field) */
	Dropdown UMETA(DisplayName = "Dropdown (Single Select)"),

	/** Checkable multi-value option row (e.g. Redmine List Custom Field with Multiple Values enabled) */
	MultiSelect UMETA(DisplayName = "Multi-Select Checkboxes"),

	/** Boolean toggle checkbox (writes "1"/"0", or "true"/"false" for is_private) */
	CheckBox UMETA(DisplayName = "Boolean CheckBox")
};

/**
 * Destination Redmine JSON payload target for a Title-defined UI control (v0.2.0+).
 */
UENUM(BlueprintType)
enum class EWcwIssueTrackerFieldTarget : uint8
{
	/** Maps to Redmine `issue.custom_fields` by Custom Field Name (e.g. "CustomFieldTest") or numeric ID */
	CustomField UMETA(DisplayName = "Redmine Custom Field (Name or ID)"),

	/** Maps to Redmine `issue.priority_id`, auto-populating choices from GET /enumerations/issue_priorities.json */
	RedminePriority UMETA(DisplayName = "Redmine Priority (priority_id)"),

	/** Maps to a standard Redmine `issue` attribute (e.g. "fixed_version_id", "status_id", "is_private", "estimated_hours") */
	StandardIssueField UMETA(DisplayName = "Redmine Standard Issue Field")
};

/**
 * Single selectable option for Dropdown / MultiSelect Title-defined UI controls in Wcw SnapJump Viewer.
 */
USTRUCT(BlueprintType)
struct WCWSNAPJUMP_API FWcwIssueTrackerUiOption
{
	GENERATED_BODY()

	/** Label shown to the user in the Editor UI dropdown or checkbox */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump|IssueTracker")
	FString DisplayLabel;

	/** Underlying value submitted to Redmine (e.g. "Test_1" for list custom fields, or "3" for priority_id) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump|IssueTracker")
	FString Value;

	FWcwIssueTrackerUiOption() = default;
	FWcwIssueTrackerUiOption(const FString& InDisplayLabel, const FString& InValue)
		: DisplayLabel(InDisplayLabel)
		, Value(InValue)
	{
	}
};

/**
 * Declarative UI descriptor returned by UWcwSnapJumpHandler::GatherIssueTrackerUiFields so any Title can expose
 * interactive Priority, Custom Fields (Text / List / Multi-List / Bool), or Standard Issue controls in Wcw SnapJump Viewer
 * while keeping zero HTTP or Editor Slate dependencies in the Runtime module.
 */
USTRUCT(BlueprintType)
struct WCWSNAPJUMP_API FWcwIssueTrackerUiField
{
	GENERATED_BODY()

	/** Redmine Custom Field Name (e.g. "CustomFieldTest", "CustomFieldTest_List"), numeric ID, or standard key ("priority_id") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump|IssueTracker")
	FString FieldKey;

	/** Human-readable label shown above the widget in Wcw SnapJump Viewer */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump|IssueTracker")
	FString DisplayLabel;

	/** Slate control style to render in Wcw SnapJump Viewer */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump|IssueTracker")
	EWcwIssueTrackerUiControlType ControlType = EWcwIssueTrackerUiControlType::TextBox;

	/** Where the value is written in the Redmine POST /issues.json payload */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump|IssueTracker")
	EWcwIssueTrackerFieldTarget TargetType = EWcwIssueTrackerFieldTarget::CustomField;

	/** Initial value (for MultiSelect, join multiple defaults with "|"; for RedminePriority, leave empty to select Redmine's default priority) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump|IssueTracker")
	FString DefaultValue;

	/** Selectable choices for Dropdown / MultiSelect (auto-populated/synced from Redmine for RedminePriority and List Custom Fields when connected) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump|IssueTracker")
	TArray<FWcwIssueTrackerUiOption> Options;

	/** Hint to serialize `custom_fields.value` as a JSON array `["..."]` (also auto-detected from Redmine `multiple=true` metadata) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump|IssueTracker")
	bool bSendAsMultipleArray = false;

	/** When true and the user leaves the control empty / unselected, omit this field from POST /issues.json */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump|IssueTracker")
	bool bOmitWhenEmpty = true;

	/** Optional tooltip or hint text shown on hover in Wcw SnapJump Viewer */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump|IssueTracker")
	FString TooltipText;
};

/**
 * DataTable Row struct for setting up snapshots + textures in a DataTable and jumping at runtime (including Packaged Builds).
 */
USTRUCT(BlueprintType)
struct WCWSNAPJUMP_API FWcwSnapJumpTableRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Display title for QA or debug menu */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	FText DisplayTitle;

	/** Relative or absolute path to the source PNG screenshot file */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	FString ScreenshotFilePath;

	/** Imported UTexture2D thumbnail asset inside <DataTableFolder>/CustomScreenShot/ for Packaged Builds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	TSoftObjectPtr<class UTexture2D> PrebakedThumbnail;

	/** Embedded snapshot metadata baked into the DataTable row for Packaged Builds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	FWcwSnapJumpData CachedSnapData;
};
