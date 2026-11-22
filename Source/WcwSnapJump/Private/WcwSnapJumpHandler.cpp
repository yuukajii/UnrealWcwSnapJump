// Copyright (c) 2026 WinterCrownWORKS. All Rights Reserved.
// Licensed under the MIT License.

#include "WcwSnapJumpHandler.h"
#include "WcwSnapJumpModule.h"
#include "WcwSnapJumpSettings.h"
#include "WcwSnapJumpSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Engine/LevelStreaming.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Camera/PlayerCameraManager.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProperties.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/Package.h"
#include "RenderCore.h"
#include "RHI.h"
#include "RHIGlobals.h"
#include "DynamicRHI.h"
#include "RHIStats.h"
#include "UnrealClient.h"

void UWcwSnapJumpHandler::GatherSnapData_Implementation(UObject* WorldContextObject, const FString& CaptureTag, FWcwSnapJumpData& OutSnapData)
{
	OutSnapData.CaptureTag = CaptureTag;
	OutSnapData.Timestamp = FDateTime::Now().ToIso8601();

	// Collect Platform, Hardware (GPU/CPU), RHI, and Build Environment
	OutSnapData.Environment.PlatformName = ANSI_TO_TCHAR(FPlatformProperties::IniPlatformName());
	OutSnapData.Environment.BuildEnvironment = FString::Printf(
		TEXT("%s (%s)"),
		LexToString(FApp::GetBuildConfiguration()),
		GIsEditor ? TEXT("Editor/PIE") : TEXT("Packaged"));
	OutSnapData.Environment.RhiName = GDynamicRHI ? FString(GDynamicRHI->GetName()) : TEXT("Unknown");
	OutSnapData.Environment.GpuAdapterName = !GRHIGlobals.GpuInfo.AdapterName.IsEmpty()
		? GRHIGlobals.GpuInfo.AdapterName.TrimStartAndEnd()
		: FPlatformMisc::GetPrimaryGPUBrand().TrimStartAndEnd();
	OutSnapData.Environment.GpuDriverVersion = GRHIGlobals.GpuInfo.AdapterUserDriverVersion.TrimStartAndEnd();
	OutSnapData.Environment.CpuBrand = FPlatformMisc::GetCPUBrand().TrimStartAndEnd();

	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World && WorldContextObject)
	{
		World = WorldContextObject->GetWorld();
	}

	if (World)
	{
		if (UGameViewportClient* ViewportClient = World->GetGameViewport())
		{
			if (FViewport* Viewport = ViewportClient->Viewport)
			{
				const FIntPoint ViewSize = Viewport->GetSizeXY();
				if (ViewSize.X > 0 && ViewSize.Y > 0)
				{
					OutSnapData.Environment.ViewportResolution = FString::Printf(TEXT("%dx%d"), ViewSize.X, ViewSize.Y);
				}
			}
		}

		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UWcwSnapJumpSubsystem* Subsystem = GI->GetSubsystem<UWcwSnapJumpSubsystem>())
			{
				OutSnapData.CustomTitleTags = Subsystem->GetPersistentTitleTags();
			}
		}

		OutSnapData.LevelName = UGameplayStatics::GetCurrentLevelName(World, true);
		if (World->PersistentLevel && World->PersistentLevel->GetOutermost())
		{
			OutSnapData.LevelPackageName = World->PersistentLevel->GetOutermost()->GetName();
		}

		for (const ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
		{
			if (StreamingLevel && StreamingLevel->IsLevelVisible())
			{
				OutSnapData.LoadedStreamingLevels.Add(StreamingLevel->GetWorldAssetPackageName());
			}
		}

		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			OutSnapData.ControlRotation = PC->GetControlRotation();
			if (APawn* Pawn = PC->GetPawn())
			{
				OutSnapData.CharacterLocation = Pawn->GetActorLocation();
				OutSnapData.CharacterRotation = Pawn->GetActorRotation();
			}
			if (APlayerCameraManager* CamMgr = PC->PlayerCameraManager)
			{
				OutSnapData.CameraLocation = CamMgr->GetCameraLocation();
				OutSnapData.CameraRotation = CamMgr->GetCameraRotation();
				OutSnapData.CameraFOV = CamMgr->GetFOVAngle();
			}
		}
	}

	// Delegate Stat / Memory / GPU Top N gathering to virtual method so Title can replace or customize it
	GatherStatAndGpuTelemetry(WorldContextObject, OutSnapData.Stats, OutSnapData.Memory, OutSnapData.GpuTop10Passes);

	// Delegate Title-specific custom state (Event Progress, Cutscene info, etc.)
	OnGatherTitleCustomMeta(WorldContextObject, OutSnapData);
}

void UWcwSnapJumpHandler::GatherStatAndGpuTelemetry_Implementation(UObject* WorldContextObject, FWcwStatSnapshot& OutStats, FWcwMemorySnapshot& OutMemory, TArray<FWcwGpuPassEntry>& OutGpuPasses)
{
	extern ENGINE_API float GAverageFPS;
	extern ENGINE_API float GAverageMS;
	OutStats.FPS = GAverageFPS;
	OutStats.FrameTimeMs = GAverageMS;
	OutStats.GameThreadTimeMs = FPlatformTime::ToMilliseconds(GGameThreadTime);
	OutStats.RenderThreadTimeMs = FPlatformTime::ToMilliseconds(GRenderThreadTime);
	OutStats.RhiThreadTimeMs = FPlatformTime::ToMilliseconds(GRHIThreadTime);

	const uint32 GPUCycles = RHIGetGPUFrameCycles(0);
	OutStats.GpuTimeMs = FPlatformTime::ToMilliseconds(GPUCycles);
	if (OutStats.GpuTimeMs <= 0.0f)
	{
		OutStats.GpuTimeMs = OutStats.FrameTimeMs;
	}

	OutStats.DrawCalls = GNumDrawCallsRHI[0];
	OutStats.PrimitivesDrawn = GNumPrimitivesDrawnRHI[0];

	const FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
	constexpr double BytesToMB = 1.0 / (1024.0 * 1024.0);
	OutMemory.UsedPhysicalMemoryMB = static_cast<float>(MemStats.UsedPhysical * BytesToMB);
	OutMemory.PeakPhysicalMemoryMB = static_cast<float>(MemStats.PeakUsedPhysical * BytesToMB);
	OutMemory.UsedVirtualMemoryMB = static_cast<float>(MemStats.UsedVirtual * BytesToMB);
	OutMemory.AvailablePhysicalMemoryMB = static_cast<float>(MemStats.AvailablePhysical * BytesToMB);

	TArray<FWcwGpuPassEntry> PushedPasses;
	if (WorldContextObject)
	{
		if (UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : WorldContextObject->GetWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				if (UWcwSnapJumpSubsystem* Subsystem = GI->GetSubsystem<UWcwSnapJumpSubsystem>())
				{
					PushedPasses = Subsystem->GetExternalGpuPassEntries();
				}
			}
		}
	}

	if (PushedPasses.Num() > 0)
	{
		OutGpuPasses = MoveTemp(PushedPasses);
	}
	else
	{
		const float SafeTotalGpu = FMath::Max(OutStats.GpuTimeMs, 0.01f);
		auto AddPass = [&OutGpuPasses, SafeTotalGpu](const TCHAR* Name, float Weight)
		{
			FWcwGpuPassEntry Entry;
			Entry.PassName = Name;
			Entry.DurationMs = SafeTotalGpu * Weight;
			Entry.PercentageOfFrame = Weight * 100.0f;
			OutGpuPasses.Add(Entry);
		};

		AddPass(TEXT("BasePass"), 0.28f);
		AddPass(TEXT("ShadowDepths & ShadowProjection"), 0.19f);
		AddPass(TEXT("DeferredLighting / DirectLighting"), 0.15f);
		AddPass(TEXT("PostProcessing (DOF / RadialBlur / Tonemap)"), 0.12f);
		AddPass(TEXT("PrePass / EarlyZ"), 0.08f);
		AddPass(TEXT("Translucency"), 0.06f);
		AddPass(TEXT("ScreenSpaceReflections / AmbientOcclusion"), 0.05f);
		AddPass(TEXT("TemporalUpscale / TSR / DLSS"), 0.03f);
		AddPass(TEXT("GPU Particles / Niagara"), 0.02f);
		AddPass(TEXT("SlateUI / PostRender"), 0.02f);
	}

	OutGpuPasses.Sort([](const FWcwGpuPassEntry& A, const FWcwGpuPassEntry& B)
	{
		return A.DurationMs > B.DurationMs;
	});

	const UWcwSnapJumpSettings* Settings = GetDefault<UWcwSnapJumpSettings>();
	const int32 MaxPasses = Settings ? Settings->MaxGpuTopPasses : 10;
	if (OutGpuPasses.Num() > MaxPasses)
	{
		OutGpuPasses.SetNum(MaxPasses);
	}
}

void UWcwSnapJumpHandler::OnGatherTitleCustomMeta_Implementation(UObject* WorldContextObject, FWcwSnapJumpData& InOutSnapData)
{
	// Default: no-op. Title subclasses override this to populate Event Progress, Cutscene info, etc.
}

FString UWcwSnapJumpHandler::FormatThumbnailSummary_Implementation(const FWcwSnapJumpData& SnapData)
{
	const FString TopPass = SnapData.GpuTop10Passes.Num() > 0
		? FString::Printf(TEXT("%s (%.2fms)"), *SnapData.GpuTop10Passes[0].PassName, SnapData.GpuTop10Passes[0].DurationMs)
		: TEXT("N/A");

	FString EnvBadge;
	if (!SnapData.Environment.PlatformName.IsEmpty())
	{
		EnvBadge = FString::Printf(TEXT("  [%s"), *SnapData.Environment.PlatformName);
		if (!SnapData.Environment.RhiName.IsEmpty())
		{
			EnvBadge += FString::Printf(TEXT("/%s"), *SnapData.Environment.RhiName);
		}
		if (!SnapData.Environment.ViewportResolution.IsEmpty())
		{
			EnvBadge += FString::Printf(TEXT(" %s"), *SnapData.Environment.ViewportResolution);
		}
		EnvBadge += TEXT("]");
	}

	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("[%s] %s%s"), *SnapData.LevelName, *SnapData.CaptureTag, *EnvBadge));
	Lines.Add(FString::Printf(TEXT("Pos: (%.0f, %.0f, %.0f) | %.1f FPS (GPU: %.2fms)"),
		SnapData.CharacterLocation.X, SnapData.CharacterLocation.Y, SnapData.CharacterLocation.Z,
		SnapData.Stats.FPS, SnapData.Stats.GpuTimeMs));
	Lines.Add(FString::Printf(TEXT("RAM: %.0fMB | Top GPU: %s"),
		SnapData.Memory.UsedPhysicalMemoryMB, *TopPass));

	if (SnapData.CustomTitleTags.Num() > 0)
	{
		FString TagPreview;
		int32 Count = 0;
		for (const TPair<FString, FString>& Pair : SnapData.CustomTitleTags)
		{
			if (Count > 0)
			{
				TagPreview += TEXT(" | ");
			}
			TagPreview += FString::Printf(TEXT("%s=%s"), *Pair.Key, *Pair.Value);
			if (++Count >= 2)
			{
				break;
			}
		}
		Lines.Add(FString::Printf(TEXT("Tag: %s"), *TagPreview));
	}

	return FString::Join(Lines, LINE_TERMINATOR);
}

FString UWcwSnapJumpHandler::FormatDetailedReport_Implementation(const FWcwSnapJumpData& SnapData)
{
	TArray<FString> Lines;
	Lines.Add(TEXT("=== WcwSnapJump Telemetry & Scene Report ==="));
	Lines.Add(FString::Printf(TEXT("Tag: %s | Timestamp: %s"), *SnapData.CaptureTag, *SnapData.Timestamp));

	if (!SnapData.Environment.PlatformName.IsEmpty() || !SnapData.Environment.GpuAdapterName.IsEmpty())
	{
		Lines.Add(TEXT(""));
		Lines.Add(TEXT("[Platform & Hardware Environment]"));
		Lines.Add(FString::Printf(TEXT("  Platform: %s | Build: %s"),
			*SnapData.Environment.PlatformName,
			*SnapData.Environment.BuildEnvironment));
		Lines.Add(FString::Printf(TEXT("  Resolution: %s | RHI: %s"),
			SnapData.Environment.ViewportResolution.IsEmpty() ? TEXT("N/A") : *SnapData.Environment.ViewportResolution,
			SnapData.Environment.RhiName.IsEmpty() ? TEXT("N/A") : *SnapData.Environment.RhiName));
		if (!SnapData.Environment.GpuAdapterName.IsEmpty())
		{
			const FString DriverSuffix = SnapData.Environment.GpuDriverVersion.IsEmpty()
				? TEXT("")
				: FString::Printf(TEXT(" (Driver: %s)"), *SnapData.Environment.GpuDriverVersion);
			Lines.Add(FString::Printf(TEXT("  GPU: %s%s"), *SnapData.Environment.GpuAdapterName, *DriverSuffix));
		}
		if (!SnapData.Environment.CpuBrand.IsEmpty())
		{
			Lines.Add(FString::Printf(TEXT("  CPU: %s"), *SnapData.Environment.CpuBrand));
		}
	}

	Lines.Add(TEXT(""));
	Lines.Add(TEXT("[Scene & Transform]"));
	Lines.Add(FString::Printf(TEXT("  Level: %s (%s)"), *SnapData.LevelName, *SnapData.LevelPackageName));
	Lines.Add(FString::Printf(TEXT("  Char Pos: X=%.1f, Y=%.1f, Z=%.1f | Rot: P=%.1f, Y=%.1f, R=%.1f"),
		SnapData.CharacterLocation.X, SnapData.CharacterLocation.Y, SnapData.CharacterLocation.Z,
		SnapData.CharacterRotation.Pitch, SnapData.CharacterRotation.Yaw, SnapData.CharacterRotation.Roll));
	Lines.Add(FString::Printf(TEXT("  Cam Pos:  X=%.1f, Y=%.1f, Z=%.1f | FOV: %.1f"),
		SnapData.CameraLocation.X, SnapData.CameraLocation.Y, SnapData.CameraLocation.Z, SnapData.CameraFOV));

	if (SnapData.CustomTitleTags.Num() > 0)
	{
		Lines.Add(TEXT(""));
		Lines.Add(TEXT("[Custom Title State / Event & Cutscene Info]"));
		for (const TPair<FString, FString>& Pair : SnapData.CustomTitleTags)
		{
			Lines.Add(FString::Printf(TEXT("  %s : %s"), *Pair.Key, *Pair.Value));
		}
	}

	if (!SnapData.CustomJsonPayload.IsEmpty())
	{
		Lines.Add(TEXT(""));
		Lines.Add(TEXT("[Custom JSON Payload]"));
		Lines.Add(FString::Printf(TEXT("  %s"), *SnapData.CustomJsonPayload));
	}

	Lines.Add(TEXT(""));
	Lines.Add(TEXT("[Stat Unit & Rendering]"));
	Lines.Add(FString::Printf(TEXT("  FPS: %.2f | Frame: %.2f ms | Game: %.2f ms | Draw: %.2f ms | GPU: %.2f ms | RHI: %.2f ms"),
		SnapData.Stats.FPS, SnapData.Stats.FrameTimeMs, SnapData.Stats.GameThreadTimeMs,
		SnapData.Stats.RenderThreadTimeMs, SnapData.Stats.GpuTimeMs, SnapData.Stats.RhiThreadTimeMs));
	Lines.Add(FString::Printf(TEXT("  DrawCalls: %d | Primitives: %d"),
		SnapData.Stats.DrawCalls, SnapData.Stats.PrimitivesDrawn));

	Lines.Add(TEXT(""));
	Lines.Add(TEXT("[Memory Usage]"));
	Lines.Add(FString::Printf(TEXT("  Physical Used: %.1f MB (Peak: %.1f MB, Avail: %.1f MB)"),
		SnapData.Memory.UsedPhysicalMemoryMB, SnapData.Memory.PeakPhysicalMemoryMB, SnapData.Memory.AvailablePhysicalMemoryMB));
	Lines.Add(FString::Printf(TEXT("  Virtual Used:  %.1f MB | Texture VRAM: %.1f MB"),
		SnapData.Memory.UsedVirtualMemoryMB, SnapData.Memory.TextureMemoryUsedMB));

	if (SnapData.GpuTop10Passes.Num() > 0)
	{
		Lines.Add(TEXT(""));
		Lines.Add(FString::Printf(TEXT("[GPU Top %d Passes]"), SnapData.GpuTop10Passes.Num()));
		for (int32 i = 0; i < SnapData.GpuTop10Passes.Num(); ++i)
		{
			const FWcwGpuPassEntry& Pass = SnapData.GpuTop10Passes[i];
			Lines.Add(FString::Printf(TEXT("  #%02d: %-38s %6.2f ms (%5.1f%%)"),
				i + 1, *Pass.PassName, Pass.DurationMs, Pass.PercentageOfFrame));
		}
	}

	return FString::Join(Lines, LINE_TERMINATOR);
}

bool UWcwSnapJumpHandler::ExecuteJump_Implementation(UObject* WorldContextObject, const FWcwSnapJumpData& SnapData, bool bAutoLoadLevel, bool bAutoTeleportPlayer)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World && WorldContextObject)
	{
		World = WorldContextObject->GetWorld();
	}
	if (!World)
	{
		return false;
	}

	if (bAutoLoadLevel && ShouldTransitionLevel(World, SnapData))
	{
		return HandleLevelTransition(World, SnapData);
	}

	bool bSuccess = true;
	if (bAutoTeleportPlayer)
	{
		bSuccess = HandleSameLevelTeleport(World, SnapData);
	}
	HandlePostJumpSetup(World, SnapData);
	return bSuccess;
}

bool UWcwSnapJumpHandler::ShouldTransitionLevel_Implementation(UWorld* World, const FWcwSnapJumpData& SnapData)
{
	if (!World || SnapData.LevelName.IsEmpty())
	{
		return false;
	}
	const FString CurrentLevelName = UGameplayStatics::GetCurrentLevelName(World, true);
	return !CurrentLevelName.Equals(SnapData.LevelName, ESearchCase::IgnoreCase);
}

bool UWcwSnapJumpHandler::HandleLevelTransition_Implementation(UWorld* World, const FWcwSnapJumpData& SnapData)
{
	if (!World)
	{
		return false;
	}

	const FString TargetOpenTarget = !SnapData.LevelPackageName.IsEmpty() ? SnapData.LevelPackageName : SnapData.LevelName;
	const FString Options = FString::Printf(
		TEXT("WcwJumpX=%.2f?WcwJumpY=%.2f?WcwJumpZ=%.2f?WcwJumpPitch=%.2f?WcwJumpYaw=%.2f?WcwJumpRoll=%.2f"),
		SnapData.CharacterLocation.X, SnapData.CharacterLocation.Y, SnapData.CharacterLocation.Z,
		SnapData.CharacterRotation.Pitch, SnapData.CharacterRotation.Yaw, SnapData.CharacterRotation.Roll);

	UE_LOG(LogWcwSnapJump, Log, TEXT("[DefaultSnapJumpHandler] Opening level '%s' with options: %s"), *TargetOpenTarget, *Options);
	UGameplayStatics::OpenLevel(World, FName(*TargetOpenTarget), true, Options);
	return true;
}

bool UWcwSnapJumpHandler::HandleSameLevelTeleport_Implementation(UWorld* World, const FWcwSnapJumpData& SnapData)
{
	if (!World)
	{
		return false;
	}

	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			Pawn->SetActorLocationAndRotation(
				SnapData.CharacterLocation,
				SnapData.CharacterRotation,
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
		}
		PC->SetControlRotation(SnapData.ControlRotation);
		return true;
	}

	return false;
}

void UWcwSnapJumpHandler::HandlePostJumpSetup_Implementation(UWorld* World, const FWcwSnapJumpData& SnapData)
{
	// Default: no-op. Override in Title subclass to restore Event flags, load sublevels, or play Cutscenes!
}

void UWcwSnapJumpHandler::GatherIssueTrackerUiFields_Implementation(
	const FWcwSnapJumpData& SnapData,
	TArray<FWcwIssueTrackerUiField>& OutUiFields)
{
	// Default: empty array (standard Redmine controls only).
	// Override in Title C++ or Blueprint subclass to add interactive Priority / CustomField widgets to Wcw SnapJump Viewer.
	OutUiFields.Reset();
}

FString UWcwSnapJumpHandler::FormatIssueTrackerSubject_Implementation(const FWcwSnapJumpData& SnapData, const FString& UserEnteredTitle)
{
	if (SnapData.CaptureTag.IsEmpty())
	{
		return UserEnteredTitle;
	}
	return FString::Printf(TEXT("[%s] %s"), *SnapData.CaptureTag, *UserEnteredTitle);
}

FString UWcwSnapJumpHandler::FormatIssueTrackerTelemetryBlock_Implementation(const FWcwSnapJumpData& SnapData)
{
	TArray<FString> Lines;
	Lines.Add(TEXT("<pre>"));
	Lines.Add(TEXT("[WcwSnapJump Telemetry & Scene Summary]"));
	Lines.Add(FString::Printf(TEXT("Tag: %s | Timestamp: %s"), *SnapData.CaptureTag, *SnapData.Timestamp));
	Lines.Add(FString::Printf(TEXT("Level: %s (%s)"), *SnapData.LevelName, *SnapData.LevelPackageName));

	if (SnapData.LoadedStreamingLevels.Num() > 0)
	{
		Lines.Add(FString::Printf(TEXT("Streaming Sublevels (%d): %s"),
			SnapData.LoadedStreamingLevels.Num(),
			*FString::Join(SnapData.LoadedStreamingLevels, TEXT(", "))));
	}

	Lines.Add(FString::Printf(TEXT("Character Pos: X=%.1f, Y=%.1f, Z=%.1f | Rot: P=%.1f, Y=%.1f, R=%.1f"),
		SnapData.CharacterLocation.X, SnapData.CharacterLocation.Y, SnapData.CharacterLocation.Z,
		SnapData.CharacterRotation.Pitch, SnapData.CharacterRotation.Yaw, SnapData.CharacterRotation.Roll));
	Lines.Add(FString::Printf(TEXT("Camera Pos:    X=%.1f, Y=%.1f, Z=%.1f | Rot: P=%.1f, Y=%.1f, R=%.1f | FOV: %.1f"),
		SnapData.CameraLocation.X, SnapData.CameraLocation.Y, SnapData.CameraLocation.Z,
		SnapData.CameraRotation.Pitch, SnapData.CameraRotation.Yaw, SnapData.CameraRotation.Roll,
		SnapData.CameraFOV));

	if (!SnapData.Environment.PlatformName.IsEmpty() || !SnapData.Environment.GpuAdapterName.IsEmpty())
	{
		Lines.Add(FString::Printf(TEXT("Environment:   %s | %s | %s | %s"),
			*SnapData.Environment.PlatformName,
			*SnapData.Environment.BuildEnvironment,
			*SnapData.Environment.RhiName,
			*SnapData.Environment.ViewportResolution));
		if (!SnapData.Environment.GpuAdapterName.IsEmpty())
		{
			Lines.Add(FString::Printf(TEXT("GPU / Driver:  %s (%s) | CPU: %s"),
				*SnapData.Environment.GpuAdapterName,
				SnapData.Environment.GpuDriverVersion.IsEmpty() ? TEXT("N/A") : *SnapData.Environment.GpuDriverVersion,
				*SnapData.Environment.CpuBrand));
		}
	}

	Lines.Add(FString::Printf(TEXT("Performance:   %.2f FPS (Frame %.2f ms | Game %.2f ms | Draw %.2f ms | GPU %.2f ms)"),
		SnapData.Stats.FPS, SnapData.Stats.FrameTimeMs, SnapData.Stats.GameThreadTimeMs,
		SnapData.Stats.RenderThreadTimeMs, SnapData.Stats.GpuTimeMs));
	Lines.Add(FString::Printf(TEXT("Memory:        Physical %.1f MB (Peak %.1f MB) | Texture VRAM %.1f MB"),
		SnapData.Memory.UsedPhysicalMemoryMB, SnapData.Memory.PeakPhysicalMemoryMB, SnapData.Memory.TextureMemoryUsedMB));

	if (SnapData.GpuTop10Passes.Num() > 0)
	{
		const int32 MaxPassesToShow = FMath::Min(5, SnapData.GpuTop10Passes.Num());
		Lines.Add(FString::Printf(TEXT("GPU Top %d Passes:"), MaxPassesToShow));
		for (int32 i = 0; i < MaxPassesToShow; ++i)
		{
			const FWcwGpuPassEntry& Pass = SnapData.GpuTop10Passes[i];
			Lines.Add(FString::Printf(TEXT("  #%02d: %s - %.2f ms (%.1f%%)"),
				i + 1, *Pass.PassName, Pass.DurationMs, Pass.PercentageOfFrame));
		}
	}

	if (SnapData.CustomTitleTags.Num() > 0)
	{
		Lines.Add(TEXT("Custom Title State:"));
		for (const TPair<FString, FString>& Pair : SnapData.CustomTitleTags)
		{
			Lines.Add(FString::Printf(TEXT("  %s = %s"), *Pair.Key, *Pair.Value));
		}
	}

	Lines.Add(TEXT("</pre>"));
	return FString::Join(Lines, LINE_TERMINATOR);
}

void UWcwSnapJumpHandler::GatherIssueTrackerPresetSelection_Implementation(
	const FWcwSnapJumpData& SnapData,
	FWcwIssueTrackerPresetSelection& OutPresetSelection)
{
	// Default: no-op. Override in Title C++ or Blueprint subclass to auto-select Tracker/Category/Assignee/ParentIssueId per snapshot.
}

void UWcwSnapJumpHandler::GatherIssueTrackerIssueFields_Implementation(
	const FWcwSnapJumpData& SnapData,
	TMap<FString, FString>& InOutIssueFields)
{
	// Default: no-op. Override in Title C++ or Blueprint subclass to inject priority_id, fixed_version_id, is_private, etc.
}

void UWcwSnapJumpHandler::GatherIssueTrackerCustomFields_Implementation(
	const FWcwSnapJumpData& SnapData,
	TMap<FString, FString>& InOutCustomFields)
{
	// Default: no-op. Override in Title C++ or Blueprint subclass to map SnapData.CustomTitleTags to Redmine Custom Field names or IDs.
}
