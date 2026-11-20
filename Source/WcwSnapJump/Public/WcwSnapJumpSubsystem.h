// Copyright (c) 2026 WinterCrownWORKS. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WcwSnapJumpTypes.h"
#include "WcwSnapJumpSubsystem.generated.h"

class UWcwSnapJumpHandler;
class UWcwSnapJumpBrowserWidget;
class UTexture2D;
class IInputProcessor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWcwOnSnapCaptured, const FString&, SavedFilePath, const FWcwSnapJumpData&, SnapData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWcwOnSnapJumpRequested, const FWcwSnapJumpData&, SnapData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FWcwCustomGatherSnapDelegate, UObject*, WorldContextObject, FWcwSnapJumpData&, InOutSnapData);
DECLARE_MULTICAST_DELEGATE_TwoParams(FWcwNativeGatherSnapDelegate, UObject* /*WorldContextObject*/, FWcwSnapJumpData& /*InOutSnapData*/);
DECLARE_DYNAMIC_DELEGATE_RetVal_TwoParams(bool, FWcwCustomSnapJumpOverrideDelegate, UObject*, WorldContextObject, const FWcwSnapJumpData&, SnapData);
DECLARE_DELEGATE_RetVal_TwoParams(bool, FWcwNativeSnapJumpOverrideDelegate, UObject* /*WorldContextObject*/, const FWcwSnapJumpData& /*SnapData*/);

/**
 * Runtime Subsystem handling Wcw custom snapshot capture, pluggable Stat/GPU/Title metadata collection,
 * console commands (`Wcw.SnapJump.Shot`, `Wcw.SnapJump.JumpFile`, `Wcw.SnapJump.ToggleMenu`),
 * console variables (`r.Wcw.SnapJump.Menu`, `r.Wcw.SnapJump.FontScale`), Gamepad IInputProcessor, and runtime jump coordination.
 */
UCLASS()
class WCWSNAPJUMP_API UWcwSnapJumpSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Wcw|SnapJump")
	void RequestCaptureWithMeta(const FString& OptionalTag = TEXT(""));

	UFUNCTION(BlueprintCallable, Category = "Wcw|SnapJump|UI")
	void SetBrowserMenuVisible(bool bVisible);

	UFUNCTION(BlueprintCallable, Category = "Wcw|SnapJump|UI")
	void ToggleBrowserMenu();

	UFUNCTION(BlueprintPure, Category = "Wcw|SnapJump|UI")
	bool IsBrowserMenuVisible() const { return bIsBrowserMenuVisible; }

	UFUNCTION(BlueprintCallable, Category = "Wcw|SnapJump")
	void SetCustomTitleTag(const FString& Key, const FString& Value);

	UFUNCTION(BlueprintCallable, Category = "Wcw|SnapJump")
	void RemoveCustomTitleTag(const FString& Key);

	const TMap<FString, FString>& GetPersistentTitleTags() const { return PersistentTitleTags; }

	UFUNCTION(BlueprintCallable, Category = "Wcw|SnapJump")
	void UpdateCustomGpuPassTimings(const TArray<FWcwGpuPassEntry>& InPasses);

	const TArray<FWcwGpuPassEntry>& GetExternalGpuPassEntries() const { return ExternalGpuPassEntries; }

	const TArray<FWcwSnapJumpData>& GetSessionCapturedSnaps() const { return SessionCapturedSnaps; }
	const TArray<TObjectPtr<UTexture2D>>& GetSessionCapturedThumbnails() const { return SessionCapturedThumbnails; }

	UFUNCTION(BlueprintCallable, Category = "Wcw|SnapJump", meta = (WorldContext = "WorldContextObject"))
	bool JumpToSnapLocation(UObject* WorldContextObject, const FWcwSnapJumpData& SnapData, bool bAutoLoadLevelIfDifferent = true, bool bAutoTeleportPlayer = true);

	UFUNCTION(BlueprintCallable, Category = "Wcw|SnapJump|Jump")
	void SetJumpHandler(UWcwSnapJumpHandler* NewHandler);

	UFUNCTION(BlueprintCallable, Category = "Wcw|SnapJump|Jump")
	UWcwSnapJumpHandler* GetOrCreateJumpHandler();

	UFUNCTION(BlueprintCallable, Category = "Wcw|SnapJump|Gather")
	void BindCustomGatherSnapOverride(FWcwCustomGatherSnapDelegate InGatherDelegate);

	UFUNCTION(BlueprintCallable, Category = "Wcw|SnapJump|Gather")
	void ClearCustomGatherSnapOverride();

	FWcwNativeGatherSnapDelegate& OnNativeGatherSnap() { return NativeGatherSnapDelegate; }

	UFUNCTION(BlueprintCallable, Category = "Wcw|SnapJump|Jump")
	void BindCustomJumpOverride(FWcwCustomSnapJumpOverrideDelegate InOverrideDelegate);

	UFUNCTION(BlueprintCallable, Category = "Wcw|SnapJump|Jump")
	void ClearCustomJumpOverride();

	FWcwNativeSnapJumpOverrideDelegate& OnNativeJumpOverride() { return NativeJumpOverrideDelegate; }

	UFUNCTION(BlueprintCallable, Category = "Wcw|SnapJump|Jump")
	void QueuePendingPostLoadJump(const FWcwSnapJumpData& SnapData);

	UFUNCTION(BlueprintCallable, Category = "Wcw|SnapJump|Jump")
	void NotifyTitleLevelLoadCompleted();

	UFUNCTION(BlueprintPure, Category = "Wcw|SnapJump|UI")
	static float GetFontScale();

	UPROPERTY(BlueprintAssignable, Category = "Wcw|SnapJump")
	FWcwOnSnapCaptured OnSnapshotCaptured;

	UPROPERTY(BlueprintAssignable, Category = "Wcw|SnapJump")
	FWcwOnSnapJumpRequested OnJumpRequested;

private:
	void HandleConsoleShotCommand(const TArray<FString>& Args);
	void HandleConsoleJumpCommand(const TArray<FString>& Args);
	void HandleConsoleToggleMenuCommand(const TArray<FString>& Args);
	void OnMenuCVarChanged(IConsoleVariable* Var);
	void OnFontScaleCVarChanged(IConsoleVariable* Var);

	void RegisterMenuInputProcessor();
	void UnregisterMenuInputProcessor();

	void OnViewportScreenshotCaptured(int32 InSizeX, int32 InSizeY, const TArray<FColor>& InImageData);
	void OnPostLoadMapWithWorld(UWorld* LoadedWorld);
	void ApplyPendingPostLoadJump(UWorld* TargetWorld);

	FWcwSnapJumpData GatherCurrentTelemetry(const FString& InTag);

	bool bPendingWcwCapture = false;
	FString PendingCaptureTag;
	FWcwSnapJumpData PendingSnapDataSnapshot;

	bool bHasPendingPostLoadJump = false;
	FWcwSnapJumpData PendingPostLoadJumpData;

	bool bIsBrowserMenuVisible = false;
	bool bUpdatingMenuCVarGuard = false;

	UPROPERTY(Transient)
	TObjectPtr<UWcwSnapJumpBrowserWidget> ActiveBrowserWidget;

	UPROPERTY(Transient)
	TObjectPtr<UWcwSnapJumpHandler> ActiveJumpHandler;

	UPROPERTY(Transient)
	TArray<FWcwSnapJumpData> SessionCapturedSnaps;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> SessionCapturedThumbnails;

	TSharedPtr<IInputProcessor> MenuInputProcessor;

	FWcwCustomGatherSnapDelegate BlueprintGatherSnapDelegate;
	FWcwNativeGatherSnapDelegate NativeGatherSnapDelegate;

	FWcwCustomSnapJumpOverrideDelegate BlueprintJumpOverrideDelegate;
	FWcwNativeSnapJumpOverrideDelegate NativeJumpOverrideDelegate;

	TMap<FString, FString> PersistentTitleTags;
	TArray<FWcwGpuPassEntry> ExternalGpuPassEntries;

	FDelegateHandle ScreenshotDelegateHandle;
	FDelegateHandle PostLoadMapDelegateHandle;
	IConsoleObject* CmdShot = nullptr;
	IConsoleObject* CmdJumpFile = nullptr;
	IConsoleObject* CmdToggleMenu = nullptr;
};
