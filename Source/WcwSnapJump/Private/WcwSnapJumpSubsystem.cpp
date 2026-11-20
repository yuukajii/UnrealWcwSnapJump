// Copyright (c) 2026 WinterCrownWORKS. All Rights Reserved.
// Licensed under the MIT License.

#include "WcwSnapJumpSubsystem.h"
#include "WcwSnapJumpModule.h"
#include "WcwSnapJumpSettings.h"
#include "WcwSnapJumpSerializer.h"
#include "WcwSnapJumpHandler.h"
#include "WcwSnapJumpBrowserWidget.h"
#include "Engine/Engine.h"
#include "Engine/Console.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageUtils.h"
#include "Modules/ModuleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "Kismet/GameplayStatics.h"
#include "TextureResource.h"
#include "UnrealClient.h"

/**
 * Slate Pre-InputProcessor active ONLY while WcwSnapJump menu is visible.
 * Guarantees 100% reliable Gamepad navigation on Switch / Switch 2 / PS5 / Xbox / PC
 * even when UConsole closing steals focus or Gamepad UserIndex differs from Keyboard!
 */
class FWcwSnapJumpInputProcessor : public IInputProcessor
{
public:
	explicit FWcwSnapJumpInputProcessor(TWeakObjectPtr<UWcwSnapJumpBrowserWidget> InWidget)
		: WeakWidget(InWidget)
	{
	}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override
	{
		if (GEngine && GEngine->GameViewport && GEngine->GameViewport->ViewportConsole && GEngine->GameViewport->ViewportConsole->ConsoleActive())
		{
			return;
		}
		if (UWcwSnapJumpBrowserWidget* Widget = WeakWidget.Get())
		{
			if (Widget->IsInViewport() && !Widget->HasAnyUserFocus() && !Widget->HasFocusedDescendants())
			{
				Widget->ForceMenuFocus();
			}
		}
	}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		// Do not intercept keys while the UE debug console (~) itself is open and typing
		if (GEngine && GEngine->GameViewport && GEngine->GameViewport->ViewportConsole && GEngine->GameViewport->ViewportConsole->ConsoleActive())
		{
			return false;
		}

		if (UWcwSnapJumpBrowserWidget* Widget = WeakWidget.Get())
		{
			if (Widget->IsInViewport() && Widget->ProcessMenuKeyDown(InKeyEvent))
			{
				return true;
			}
		}
		return false;
	}

	virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogEvent) override
	{
		if (GEngine && GEngine->GameViewport && GEngine->GameViewport->ViewportConsole && GEngine->GameViewport->ViewportConsole->ConsoleActive())
		{
			return false;
		}

		if (UWcwSnapJumpBrowserWidget* Widget = WeakWidget.Get())
		{
			if (Widget->IsInViewport() && Widget->ProcessMenuAnalogInput(InAnalogEvent))
			{
				return true;
			}
		}
		return false;
	}

private:
	TWeakObjectPtr<UWcwSnapJumpBrowserWidget> WeakWidget;
};

static TAutoConsoleVariable<int32> CVarWcwSnapJumpEnable(
	TEXT("r.Wcw.SnapJump.Enable"),
	1,
	TEXT("Enable/Disable WcwSnapJump custom snapshot & jump system (0: Disabled, 1: Enabled)."),
	ECVF_Default
);

static TAutoConsoleVariable<int32> CVarWcwSnapJumpShowUI(
	TEXT("r.Wcw.SnapJump.ShowUI"),
	0,
	TEXT("Whether Wcw.SnapJump.Shot includes Slate UI in the screenshot (0: Scene only, 1: Include UI)."),
	ECVF_Default
);

static TAutoConsoleVariable<int32> CVarWcwSnapJumpResolution(
	TEXT("r.Wcw.SnapJump.Resolution"),
	0,
	TEXT("Override saved snapshot max bound resolution (e.g. 512 -> fits within 512x512 keeping aspect ratio). If 0, uses Project Settings."),
	ECVF_Default
);

static TAutoConsoleVariable<int32> CVarWcwSnapJumpMenu(
	TEXT("r.Wcw.SnapJump.Menu"),
	0,
	TEXT("Toggle WcwSnapJump runtime browser menu visibility (0: Hidden, 1: Visible). Supports Gamepad/Keyboard/Mouse."),
	ECVF_Default
);

static TAutoConsoleVariable<float> CVarWcwSnapJumpFontScale(
	TEXT("r.Wcw.SnapJump.FontScale"),
	1.0f,
	TEXT("Scale factor for text font size in WcwSnapJump Browser UI (default: 1.0, range: 0.5 - 3.0)."),
	ECVF_Default
);

void UWcwSnapJumpSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	CmdShot = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Wcw.SnapJump.Shot"),
		TEXT("Captures a custom PNG snapshot with embedded WcwSnapJump metadata. Usage: Wcw.SnapJump.Shot [OptionalTag]"),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UWcwSnapJumpSubsystem::HandleConsoleShotCommand),
		ECVF_Default
	);

	CmdJumpFile = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Wcw.SnapJump.JumpFile"),
		TEXT("Reads embedded metadata from the given PNG filename in Saved/WcwSnapJump and jumps to it. Usage: Wcw.SnapJump.JumpFile <FileName.png>"),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UWcwSnapJumpSubsystem::HandleConsoleJumpCommand),
		ECVF_Default
	);

	CmdToggleMenu = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Wcw.SnapJump.ToggleMenu"),
		TEXT("Toggles the runtime WcwSnapJump Browser Menu on/off (Gamepad, Keyboard & Mouse supported)."),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UWcwSnapJumpSubsystem::HandleConsoleToggleMenuCommand),
		ECVF_Default
	);

	CVarWcwSnapJumpMenu.AsVariable()->SetOnChangedCallback(
		FConsoleVariableDelegate::CreateUObject(this, &UWcwSnapJumpSubsystem::OnMenuCVarChanged));

	CVarWcwSnapJumpFontScale.AsVariable()->SetOnChangedCallback(
		FConsoleVariableDelegate::CreateUObject(this, &UWcwSnapJumpSubsystem::OnFontScaleCVarChanged));

	PostLoadMapDelegateHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this, &UWcwSnapJumpSubsystem::OnPostLoadMapWithWorld);
}

void UWcwSnapJumpSubsystem::Deinitialize()
{
	UnregisterMenuInputProcessor();

	if (ScreenshotDelegateHandle.IsValid())
	{
		UGameViewportClient::OnScreenshotCaptured().Remove(ScreenshotDelegateHandle);
		ScreenshotDelegateHandle.Reset();
	}

	if (PostLoadMapDelegateHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapDelegateHandle);
		PostLoadMapDelegateHandle.Reset();
	}

	CVarWcwSnapJumpMenu.AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate());
	CVarWcwSnapJumpFontScale.AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate());

	if (CmdShot)
	{
		IConsoleManager::Get().UnregisterConsoleObject(CmdShot);
		CmdShot = nullptr;
	}

	if (CmdJumpFile)
	{
		IConsoleManager::Get().UnregisterConsoleObject(CmdJumpFile);
		CmdJumpFile = nullptr;
	}

	if (CmdToggleMenu)
	{
		IConsoleManager::Get().UnregisterConsoleObject(CmdToggleMenu);
		CmdToggleMenu = nullptr;
	}

	if (ActiveBrowserWidget)
	{
		ActiveBrowserWidget->RemoveFromParent();
		ActiveBrowserWidget = nullptr;
	}

	SessionCapturedSnaps.Reset();
	SessionCapturedThumbnails.Reset();

	Super::Deinitialize();
}

float UWcwSnapJumpSubsystem::GetFontScale()
{
	return FMath::Clamp(CVarWcwSnapJumpFontScale.GetValueOnGameThread(), 0.5f, 3.0f);
}

void UWcwSnapJumpSubsystem::RegisterMenuInputProcessor()
{
	UnregisterMenuInputProcessor();
	if (FSlateApplication::IsInitialized() && ActiveBrowserWidget)
	{
		MenuInputProcessor = MakeShared<FWcwSnapJumpInputProcessor>(ActiveBrowserWidget);
		FSlateApplication::Get().RegisterInputPreProcessor(MenuInputProcessor, 0);
	}
}

void UWcwSnapJumpSubsystem::UnregisterMenuInputProcessor()
{
	if (MenuInputProcessor.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(MenuInputProcessor);
	}
	MenuInputProcessor.Reset();
}

void UWcwSnapJumpSubsystem::SetBrowserMenuVisible(bool bVisible)
{
	if (bIsBrowserMenuVisible == bVisible && (bVisible == (ActiveBrowserWidget != nullptr && ActiveBrowserWidget->IsInViewport())))
	{
		return;
	}

	bIsBrowserMenuVisible = bVisible;

	if (!bUpdatingMenuCVarGuard)
	{
		bUpdatingMenuCVarGuard = true;
		CVarWcwSnapJumpMenu.AsVariable()->Set(bVisible ? 1 : 0, ECVF_SetByCode);
		bUpdatingMenuCVarGuard = false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APlayerController* PC = World->GetFirstPlayerController();

	if (bVisible)
	{
		if (!ActiveBrowserWidget)
		{
			ActiveBrowserWidget = CreateWidget<UWcwSnapJumpBrowserWidget>(GetGameInstance(), UWcwSnapJumpBrowserWidget::StaticClass());
		}

		if (ActiveBrowserWidget && !ActiveBrowserWidget->IsInViewport())
		{
			ActiveBrowserWidget->AddToViewport(9999);
		}

		if (ActiveBrowserWidget)
		{
			ActiveBrowserWidget->RefreshSnapshotList();
		}

		// Set UIOnly input mode FIRST so PlayerController / Gameplay input is locked out,
		// THEN register our InputProcessor at Index 0 so it sits ahead of CommonUI / AnalogCursor!
		if (PC)
		{
			PC->bShowMouseCursor = true;
			FInputModeUIOnly InputMode;
			if (ActiveBrowserWidget)
			{
				InputMode.SetWidgetToFocus(ActiveBrowserWidget->TakeWidget());
			}
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PC->SetInputMode(InputMode);
		}

		RegisterMenuInputProcessor();

		if (ActiveBrowserWidget)
		{
			ActiveBrowserWidget->ForceMenuFocus();
		}
	}
	else
	{
		UnregisterMenuInputProcessor();

		if (ActiveBrowserWidget && ActiveBrowserWidget->IsInViewport())
		{
			ActiveBrowserWidget->RemoveFromParent();
		}

		if (PC)
		{
			PC->bShowMouseCursor = false;
			FInputModeGameOnly GameOnlyMode;
			PC->SetInputMode(GameOnlyMode);
		}
	}
}

void UWcwSnapJumpSubsystem::ToggleBrowserMenu()
{
	SetBrowserMenuVisible(!bIsBrowserMenuVisible);
}

void UWcwSnapJumpSubsystem::OnMenuCVarChanged(IConsoleVariable* Var)
{
	if (bUpdatingMenuCVarGuard || !Var)
	{
		return;
	}

	bUpdatingMenuCVarGuard = true;
	const bool bShouldShow = (Var->GetInt() != 0);
	SetBrowserMenuVisible(bShouldShow);
	bUpdatingMenuCVarGuard = false;
}

void UWcwSnapJumpSubsystem::OnFontScaleCVarChanged(IConsoleVariable* Var)
{
	if (ActiveBrowserWidget && ActiveBrowserWidget->IsInViewport())
	{
		ActiveBrowserWidget->RefreshFontScale();
	}
}

void UWcwSnapJumpSubsystem::RequestCaptureWithMeta(const FString& OptionalTag)
{
	const UWcwSnapJumpSettings* Settings = GetDefault<UWcwSnapJumpSettings>();
	if ((Settings && !Settings->bEnableWcwSnapJump) || CVarWcwSnapJumpEnable.GetValueOnGameThread() == 0)
	{
		UE_LOG(LogWcwSnapJump, Warning, TEXT("WcwSnapJump is disabled via Project Settings or r.Wcw.SnapJump.Enable."));
		return;
	}

	PendingCaptureTag = OptionalTag.IsEmpty() ? TEXT("Snap") : OptionalTag;
	PendingSnapDataSnapshot = GatherCurrentTelemetry(PendingCaptureTag);
	bPendingWcwCapture = true;

	if (!ScreenshotDelegateHandle.IsValid())
	{
		ScreenshotDelegateHandle = UGameViewportClient::OnScreenshotCaptured().AddUObject(
			this, &UWcwSnapJumpSubsystem::OnViewportScreenshotCaptured);
	}

	const bool bShowUI = (CVarWcwSnapJumpShowUI.GetValueOnGameThread() != 0) ||
	                     (Settings && Settings->bShowUIInScreenshot);

	FScreenshotRequest::RequestScreenshot(bShowUI);
	UE_LOG(LogWcwSnapJump, Log, TEXT("Requested WcwSnapJump capture (Tag: %s)..."), *PendingCaptureTag);
}

void UWcwSnapJumpSubsystem::SetCustomTitleTag(const FString& Key, const FString& Value)
{
	if (!Key.IsEmpty())
	{
		PersistentTitleTags.Add(Key, Value);
	}
}

void UWcwSnapJumpSubsystem::RemoveCustomTitleTag(const FString& Key)
{
	PersistentTitleTags.Remove(Key);
}

void UWcwSnapJumpSubsystem::UpdateCustomGpuPassTimings(const TArray<FWcwGpuPassEntry>& InPasses)
{
	ExternalGpuPassEntries = InPasses;
}

void UWcwSnapJumpSubsystem::SetJumpHandler(UWcwSnapJumpHandler* NewHandler)
{
	ActiveJumpHandler = NewHandler;
}

UWcwSnapJumpHandler* UWcwSnapJumpSubsystem::GetOrCreateJumpHandler()
{
	if (ActiveJumpHandler)
	{
		return ActiveJumpHandler;
	}

	UClass* HandlerClass = UWcwSnapJumpHandler::StaticClass();
	if (const UWcwSnapJumpSettings* Settings = GetDefault<UWcwSnapJumpSettings>())
	{
		if (!Settings->CustomJumpHandlerClass.IsNull())
		{
			if (UClass* LoadedClass = Settings->CustomJumpHandlerClass.LoadSynchronous())
			{
				HandlerClass = LoadedClass;
			}
		}
	}

	ActiveJumpHandler = NewObject<UWcwSnapJumpHandler>(this, HandlerClass);
	return ActiveJumpHandler;
}

void UWcwSnapJumpSubsystem::BindCustomGatherSnapOverride(FWcwCustomGatherSnapDelegate InGatherDelegate)
{
	BlueprintGatherSnapDelegate = InGatherDelegate;
}

void UWcwSnapJumpSubsystem::ClearCustomGatherSnapOverride()
{
	BlueprintGatherSnapDelegate.Unbind();
}

void UWcwSnapJumpSubsystem::BindCustomJumpOverride(FWcwCustomSnapJumpOverrideDelegate InOverrideDelegate)
{
	BlueprintJumpOverrideDelegate = InOverrideDelegate;
}

void UWcwSnapJumpSubsystem::ClearCustomJumpOverride()
{
	BlueprintJumpOverrideDelegate.Unbind();
}

void UWcwSnapJumpSubsystem::QueuePendingPostLoadJump(const FWcwSnapJumpData& SnapData)
{
	PendingPostLoadJumpData = SnapData;
	bHasPendingPostLoadJump = true;
}

void UWcwSnapJumpSubsystem::NotifyTitleLevelLoadCompleted()
{
	if (!bHasPendingPostLoadJump)
	{
		return;
	}
	ApplyPendingPostLoadJump(GetWorld());
}

bool UWcwSnapJumpSubsystem::JumpToSnapLocation(UObject* WorldContextObject, const FWcwSnapJumpData& SnapData, bool bAutoLoadLevelIfDifferent, bool bAutoTeleportPlayer)
{
	if (bIsBrowserMenuVisible)
	{
		SetBrowserMenuVisible(false);
	}

	UObject* EffectiveContext = WorldContextObject ? WorldContextObject : this;
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(EffectiveContext, EGetWorldErrorMode::LogAndReturnNull) : GetWorld();
	if (!World)
	{
		World = GetWorld();
	}

	UWcwSnapJumpHandler* Handler = GetOrCreateJumpHandler();
	const bool bWillTransitionLevel = (World && bAutoLoadLevelIfDifferent && Handler && Handler->ShouldTransitionLevel(World, SnapData));

	if (bWillTransitionLevel)
	{
		QueuePendingPostLoadJump(SnapData);
	}

	bool bHandled = false;

	if (NativeJumpOverrideDelegate.IsBound())
	{
		bHandled = NativeJumpOverrideDelegate.Execute(EffectiveContext, SnapData);
	}

	if (!bHandled && BlueprintJumpOverrideDelegate.IsBound())
	{
		bHandled = BlueprintJumpOverrideDelegate.Execute(EffectiveContext, SnapData);
	}

	if (!bHandled && Handler)
	{
		bHandled = Handler->ExecuteJump(EffectiveContext, SnapData, bAutoLoadLevelIfDifferent, bAutoTeleportPlayer);
	}

	if (!bWillTransitionLevel)
	{
		OnJumpRequested.Broadcast(SnapData);
	}

	return bHandled;
}

void UWcwSnapJumpSubsystem::OnPostLoadMapWithWorld(UWorld* LoadedWorld)
{
	if (!bHasPendingPostLoadJump || !LoadedWorld)
	{
		return;
	}

	if (LoadedWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	UWcwSnapJumpHandler* Handler = GetOrCreateJumpHandler();
	if (Handler && !Handler->bAutoApplyOnEnginePostLoadMap)
	{
		UE_LOG(LogWcwSnapJump, Log, TEXT("Map '%s' loaded; waiting for Title to call NotifyTitleLevelLoadCompleted()."), *LoadedWorld->GetName());
		return;
	}

	ApplyPendingPostLoadJump(LoadedWorld);
}

void UWcwSnapJumpSubsystem::ApplyPendingPostLoadJump(UWorld* TargetWorld)
{
	if (!bHasPendingPostLoadJump || !TargetWorld)
	{
		return;
	}

	bHasPendingPostLoadJump = false;

	if (UWcwSnapJumpHandler* Handler = GetOrCreateJumpHandler())
	{
		Handler->HandleSameLevelTeleport(TargetWorld, PendingPostLoadJumpData);
		Handler->HandlePostJumpSetup(TargetWorld, PendingPostLoadJumpData);
	}

	OnJumpRequested.Broadcast(PendingPostLoadJumpData);
	UE_LOG(LogWcwSnapJump, Log, TEXT("Applied post-load WcwSnapJump teleport, HandlePostJumpSetup, and OnJumpRequested in level '%s'."), *TargetWorld->GetName());
}

void UWcwSnapJumpSubsystem::OnViewportScreenshotCaptured(int32 InSizeX, int32 InSizeY, const TArray<FColor>& InImageData)
{
	if (!bPendingWcwCapture)
	{
		return;
	}
	bPendingWcwCapture = false;

	const UWcwSnapJumpSettings* Settings = GetDefault<UWcwSnapJumpSettings>();
	const int32 CVarRes = CVarWcwSnapJumpResolution.GetValueOnGameThread();
	const int32 MaxBoundWidth = (CVarRes > 0) ? CVarRes : (Settings ? FMath::Max(64, Settings->ScreenshotWidth) : 512);
	const int32 MaxBoundHeight = (CVarRes > 0) ? CVarRes : (Settings ? FMath::Max(64, Settings->ScreenshotHeight) : 512);

	const float ScaleX = static_cast<float>(MaxBoundWidth) / static_cast<float>(FMath::Max(1, InSizeX));
	const float ScaleY = static_cast<float>(MaxBoundHeight) / static_cast<float>(FMath::Max(1, InSizeY));
	const float UniformScale = FMath::Min(ScaleX, ScaleY);

	const int32 TargetWidth = FMath::Max(1, FMath::RoundToInt(static_cast<float>(InSizeX) * UniformScale));
	const int32 TargetHeight = FMath::Max(1, FMath::RoundToInt(static_cast<float>(InSizeY) * UniformScale));

	TArray<FColor> ResizedPixels;
	int32 FinalWidth = InSizeX;
	int32 FinalHeight = InSizeY;

	if (InSizeX != TargetWidth || InSizeY != TargetHeight)
	{
		ResizedPixels.SetNumUninitialized(TargetWidth * TargetHeight);
		FImageUtils::ImageResize(InSizeX, InSizeY, InImageData, TargetWidth, TargetHeight, ResizedPixels, false);
		FinalWidth = TargetWidth;
		FinalHeight = TargetHeight;
	}
	else
	{
		ResizedPixels = InImageData;
	}

	for (FColor& Pixel : ResizedPixels)
	{
		Pixel.A = 255;
	}

	const FString SaveDir = UWcwSnapJumpSettings::GetResolvedScreenshotDirectory();
	IFileManager::Get().MakeDirectory(*SaveDir, true);

	const FString TimestampSlug = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
	const FString SafeTag = FPaths::MakeValidFileName(PendingCaptureTag);
	const FString FileName = FString::Printf(TEXT("WcwSnap_%s_%s_%s.png"),
		*PendingSnapDataSnapshot.LevelName, *SafeTag, *TimestampSlug);
	const FString FullPngPath = SaveDir / FileName;

	PendingSnapDataSnapshot.SourceFilePath = FullPngPath;

	// Cache in-memory texture + metadata immediately so Console sessions ALWAYS show live captures in the menu!
	if (UTexture2D* LiveThumb = UTexture2D::CreateTransient(FinalWidth, FinalHeight, PF_B8G8R8A8))
	{
		if (LiveThumb->GetPlatformData() && LiveThumb->GetPlatformData()->Mips.Num() > 0)
		{
			LiveThumb->SRGB = true;
			LiveThumb->NeverStream = true;
			void* MipData = LiveThumb->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
			FMemory::Memcpy(MipData, ResizedPixels.GetData(), ResizedPixels.Num() * sizeof(FColor));
			LiveThumb->GetPlatformData()->Mips[0].BulkData.Unlock();
			LiveThumb->UpdateResource();

			SessionCapturedSnaps.Add(PendingSnapDataSnapshot);
			SessionCapturedThumbnails.Add(LiveThumb);
		}
	}

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
	if (!ImageWrapper.IsValid() || !ImageWrapper->SetRaw(ResizedPixels.GetData(), ResizedPixels.Num() * sizeof(FColor), FinalWidth, FinalHeight, ERGBFormat::BGRA, 8))
	{
		UE_LOG(LogWcwSnapJump, Error, TEXT("Failed to compress raw viewport pixels into PNG."));
		return;
	}

	const TArray64<uint8>& CompressedData64 = ImageWrapper->GetCompressed(100);
	TArray<uint8> RawPngBytes;
	RawPngBytes.Append(CompressedData64.GetData(), static_cast<int32>(CompressedData64.Num()));

	TArray<uint8> FinalPngBytes;
	if (!FWcwSnapJumpSerializer::InjectMetaIntoPngBytes(RawPngBytes, PendingSnapDataSnapshot, FinalPngBytes))
	{
		FinalPngBytes = MoveTemp(RawPngBytes);
	}

	if (FFileHelper::SaveArrayToFile(FinalPngBytes, *FullPngPath))
	{
		UE_LOG(LogWcwSnapJump, Log, TEXT("Saved WcwSnapJump PNG (%dx%d) with embedded metadata: %s"), FinalWidth, FinalHeight, *FullPngPath);

		if (!Settings || Settings->bWriteJsonSidecar)
		{
			FString PrettyJson;
			if (FWcwSnapJumpSerializer::StructToJsonString(PendingSnapDataSnapshot, PrettyJson, true))
			{
				const FString SidecarPath = FPaths::ChangeExtension(FullPngPath, TEXT("json"));
				FFileHelper::SaveStringToFile(PrettyJson, *SidecarPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
			}
		}

		OnSnapshotCaptured.Broadcast(FullPngPath, PendingSnapDataSnapshot);
	}
	else
	{
		UE_LOG(LogWcwSnapJump, Warning, TEXT("Could not write PNG to disk '%s' (kept in live session memory)."), *FullPngPath);
	}

	if (bIsBrowserMenuVisible && ActiveBrowserWidget && ActiveBrowserWidget->IsInViewport())
	{
		ActiveBrowserWidget->RefreshSnapshotList();
	}
}

FWcwSnapJumpData UWcwSnapJumpSubsystem::GatherCurrentTelemetry(const FString& InTag)
{
	FWcwSnapJumpData SnapData;

	if (UWcwSnapJumpHandler* Handler = GetOrCreateJumpHandler())
	{
		Handler->GatherSnapData(this, InTag, SnapData);
	}

	if (BlueprintGatherSnapDelegate.IsBound())
	{
		BlueprintGatherSnapDelegate.Execute(this, SnapData);
	}

	if (NativeGatherSnapDelegate.IsBound())
	{
		NativeGatherSnapDelegate.Broadcast(this, SnapData);
	}

	return SnapData;
}

void UWcwSnapJumpSubsystem::HandleConsoleShotCommand(const TArray<FString>& Args)
{
	const FString Tag = (Args.Num() > 0) ? Args[0] : TEXT("ConsoleSnap");
	RequestCaptureWithMeta(Tag);
}

void UWcwSnapJumpSubsystem::HandleConsoleJumpCommand(const TArray<FString>& Args)
{
	if (Args.Num() == 0)
	{
		UE_LOG(LogWcwSnapJump, Warning, TEXT("Usage: Wcw.SnapJump.JumpFile <FileName.png>"));
		return;
	}

	FString TargetPath = Args[0];
	if (FPaths::IsRelative(TargetPath))
	{
		TargetPath = UWcwSnapJumpSettings::GetResolvedScreenshotDirectory() / TargetPath;
	}

	FWcwSnapJumpData SnapData;
	if (FWcwSnapJumpSerializer::ReadMetaFromFile(TargetPath, SnapData))
	{
		JumpToSnapLocation(this, SnapData, true, true);
	}
	else
	{
		UE_LOG(LogWcwSnapJump, Warning, TEXT("Failed to read WcwSnapJump data from '%s'"), *TargetPath);
	}
}

void UWcwSnapJumpSubsystem::HandleConsoleToggleMenuCommand(const TArray<FString>& Args)
{
	if (Args.Num() > 0)
	{
		const int32 Val = FCString::Atoi(*Args[0]);
		SetBrowserMenuVisible(Val != 0);
	}
	else
	{
		ToggleBrowserMenu();
	}
}
