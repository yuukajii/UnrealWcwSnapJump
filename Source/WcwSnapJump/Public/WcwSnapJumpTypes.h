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
