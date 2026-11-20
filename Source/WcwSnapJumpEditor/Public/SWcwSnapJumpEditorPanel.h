// Copyright (c) 2026 WinterCrownWORKS. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Brushes/SlateImageBrush.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/Texture2D.h"
#include "WcwSnapJumpTypes.h"

struct FWcwSnapJumpEditorRowItem
{
	FWcwSnapJumpData SnapData;
	TStrongObjectPtr<UTexture2D> ThumbnailTexture;
	TSharedPtr<FSlateImageBrush> ThumbnailBrush;
};

/**
 * Slate Editor Panel for inspecting WcwSnapJump snapshots with inline clickable thumbnails,
 * comparing GPU/Memory load, jumping in PIE or teleporting the active Editor Viewport camera,
 * and importing snapshots as persistent UTexture2D assets + UDataTable rows for Packaged Builds.
 */
class SWcwSnapJumpEditorPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWcwSnapJumpEditorPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SWcwSnapJumpEditorPanel() override;

	/** Execute jump for the given item: if PIE is active, triggers runtime JumpToSnapData; otherwise moves Editor viewport camera */
	void ExecuteJumpForItem(const TSharedPtr<FWcwSnapJumpEditorRowItem>& Item);

private:
	TSharedRef<ITableRow> OnGenerateRowForList(TSharedPtr<FWcwSnapJumpEditorRowItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnSelectionChanged(TSharedPtr<FWcwSnapJumpEditorRowItem> SelectedItem, ESelectInfo::Type SelectInfo);
	void OnListMouseButtonDoubleClick(TSharedPtr<FWcwSnapJumpEditorRowItem> ClickedItem);

	static UTexture2D* ImportPngAsTextureAsset(const FString& PngFilePath, const FString& TargetPackageFolder, const FString& AssetName);
	static class UDataTable* GetOrCreateTargetDataTable();

	FReply OnRefreshClicked();
	FReply OnMoveEditorCameraClicked();
	FReply OnSyncToDefaultDataTableClicked();

	TArray<TSharedPtr<FWcwSnapJumpEditorRowItem>> SnapshotItems;
	TSharedPtr<FWcwSnapJumpEditorRowItem> CurrentSelectedItem;

	TSharedPtr<SListView<TSharedPtr<FWcwSnapJumpEditorRowItem>>> ListViewWidget;
	TSharedPtr<class SImage> SelectedPreviewImage;
	TSharedPtr<class STextBlock> DetailReportText;
};
