// Copyright (c) 2026 WinterCrownWORKS. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "Brushes/SlateImageBrush.h"
#include "WcwSnapJumpTypes.h"
#include "WcwSnapJumpBrowserWidget.generated.h"

class UDataTable;
class SScrollBox;
class STextBlock;
class SImage;
class SButton;

/**
 * Runtime UMG Widget base class displaying a left-half 2-tier scrollable list of Wcw snapshots with thumbnails and metadata.
 * Enforces focus while open and supports Mouse, Keyboard, and Console Gamepad (Switch / Switch 2 / PS5 / Xbox).
 */
UCLASS(BlueprintType, Blueprintable)
class WCWSNAPJUMP_API UWcwSnapJumpBrowserWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UWcwSnapJumpBrowserWidget(const FObjectInitializer& ObjectInitializer);

	/** Optional DataTable asset containing pre-registered snapshots (with imported UTexture2D assets for packaged builds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	TObjectPtr<UDataTable> SourceDataTable;

	/** Whether to also scan Saved/WcwSnapJump on disk at runtime */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	bool bIncludeSavedDirectorySnapshots = true;

	/** If true (default), clicking the snapshot thumbnail directly triggers JumpToSnapData! */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wcw|SnapJump")
	bool bJumpOnThumbnailClick = true;

	/** Refresh the thumbnail list from SourceDataTable, AssetRegistry fallback, Session Captures, and Saved/WcwSnapJump */
	UFUNCTION(BlueprintCallable, Category = "Wcw|SnapJump")
	void RefreshSnapshotList();

	/** Re-apply font size scaling based on `r.Wcw.SnapJump.FontScale` */
	UFUNCTION(BlueprintCallable, Category = "Wcw|SnapJump")
	void RefreshFontScale();

	/** Select an entry by index, auto-scroll it into view, and update the detail preview */
	UFUNCTION(BlueprintCallable, Category = "Wcw|SnapJump")
	void SelectSnapshotByIndex(int32 Index, bool bScrollIntoView = true);

	/** Move selection by Delta (e.g. -1 for Up, +1 for Down, -5 for PageUp/LB, +5 for PageDown/RB) */
	UFUNCTION(BlueprintCallable, Category = "Wcw|SnapJump")
	void MoveSelectionByDelta(int32 Delta);

	/** Scroll the bottom-right detail telemetry pane vertically (for Gamepad Right Stick or Keyboard) */
	UFUNCTION(BlueprintCallable, Category = "Wcw|SnapJump")
	void ScrollDetailPaneByDelta(float ScrollDeltaPixels);

	/** Force all-user focus onto this menu widget */
	void ForceMenuFocus();

	/** Handle KeyDown from either IInputProcessor or Slate focus */
	bool ProcessMenuKeyDown(const FKeyEvent& InKeyEvent);

	/** Handle Analog stick input from either IInputProcessor or Slate focus */
	bool ProcessMenuAnalogInput(const FAnalogInputEvent& InAnalogEvent);

	/** Trigger runtime jump to the currently selected snapshot's metadata */
	UFUNCTION(BlueprintCallable, Category = "Wcw|SnapJump")
	void JumpToSelectedSnapshot();

	/** Close the browser menu via UWcwSnapJumpSubsystem */
	UFUNCTION(BlueprintCallable, Category = "Wcw|SnapJump")
	void CloseBrowserMenu();

	/** Blueprint event fired when a snapshot thumbnail is selected */
	UFUNCTION(BlueprintImplementableEvent, Category = "Wcw|SnapJump")
	void OnSnapshotEntrySelected(const FWcwSnapJumpData& SelectedSnapData, UTexture2D* LoadedThumbnail);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnAnalogValueChanged(const FGeometry& InGeometry, const FAnalogInputEvent& InAnalogEvent) override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	TArray<FWcwSnapJumpData> Entries;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> CachedThumbnails;

	TArray<TSharedPtr<FSlateImageBrush>> RowThumbnailBrushes;
	TArray<TSharedPtr<SWidget>> RowWidgets;
	TArray<TSharedPtr<STextBlock>> RowSummaryTexts;

	int32 SelectedIndex = INDEX_NONE;
	double LastAnalogNavTime = 0.0;
	uint64 LastHandledKeyFrame = 0;

	TSharedPtr<SScrollBox> LeftListScrollBox;
	TSharedPtr<SVerticalBox> ListContainerBox;
	TSharedPtr<SScrollBox> RightDetailScrollBox;
	TSharedPtr<SImage> SelectedPreviewImage;
	TSharedPtr<STextBlock> HeaderTitleText;
	TSharedPtr<STextBlock> DetailTextBlock;
};
