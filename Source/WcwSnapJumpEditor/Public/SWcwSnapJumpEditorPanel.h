// Copyright (c) 2026 WinterCrownWORKS. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SComboBox.h"
#include "Brushes/SlateImageBrush.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/Texture2D.h"
#include "WcwSnapJumpTypes.h"
#include "WcwRedmineClient.h"

struct FWcwSnapJumpEditorRowItem
{
	FWcwSnapJumpData SnapData;
	TStrongObjectPtr<UTexture2D> ThumbnailTexture;
	TSharedPtr<FSlateImageBrush> ThumbnailBrush;

	/** Non-zero when this snapshot has already been posted to Redmine (read from local <BaseName>.json) */
	int32 PostedIssueId = 0;
	FString PostedIssueUrl;
	FString PostedAt;
};

/**
 * Runtime UI state for a single interactive Title-defined field rendered in Wcw SnapJump Viewer.
 */
struct FWcwDynamicUiFieldState
{
	FWcwIssueTrackerUiField Def;
	FString CurrentValue;
	bool bModifiedByUser = false;

	TArray<TSharedPtr<FWcwIssueTrackerUiOption>> ComboOptions;
	TSharedPtr<FWcwIssueTrackerUiOption> SelectedComboOption;
};

/**
 * Slate Editor Panel for inspecting WcwSnapJump snapshots, jumping in PIE / Editor Viewport,
 * baking DataTables for Packaged Builds, and posting tickets directly to Redmine (v0.2.0+).
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

	/** Opens the posted Redmine ticket URL in the user's default web browser */
	void OpenPostedIssueInBrowser(const TSharedPtr<FWcwSnapJumpEditorRowItem>& Item) const;

private:
	EActiveTimerReturnType HandlePostConstructInit(double InCurrentTime, float InDeltaTime);

	TSharedRef<ITableRow> OnGenerateRowForList(TSharedPtr<FWcwSnapJumpEditorRowItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnSelectionChanged(TSharedPtr<FWcwSnapJumpEditorRowItem> SelectedItem, ESelectInfo::Type SelectInfo);
	void OnListMouseButtonDoubleClick(TSharedPtr<FWcwSnapJumpEditorRowItem> ClickedItem);

	static UTexture2D* ImportPngAsTextureAsset(const FString& PngFilePath, const FString& TargetPackageFolder, const FString& AssetName);
	static class UDataTable* GetOrCreateTargetDataTable();

	FReply OnRefreshClicked();
	FReply OnMoveEditorCameraClicked();
	FReply OnSyncToDefaultDataTableClicked();
	FReply OnOpenRedmineProjectSettingsClicked();

	// --- Redmine Integration UI & Actions ---
	TSharedRef<SWidget> BuildRedminePostTabWidget();
	void RebuildParentPresetOptions();
	void RebuildDynamicHandlerUiFields(bool bPreserveUserEdits);
	TSharedRef<SWidget> BuildSingleDynamicFieldWidget(const TSharedPtr<FWcwDynamicUiFieldState>& State);
	void ApplyDynamicUiFieldsToMaps(
		TMap<FString, FString>& InOutExtraIssueFields,
		TMap<FString, FString>& InOutCustomFields,
		TSet<FString>* OutMultiValueKeys = nullptr) const;
	void ApplyHandlerPresetSelectionForCurrentSnap();
	void SyncSubjectPrefixForSelection(bool bForceResetIfUnedited);
	void StartVerifyConnectionAndFetchMeta();
	void TriggerVerifyParentTicket(int32 IssueIdToVerify);

	FReply OnSaveApiKeyAndConnectClicked();
	FReply OnAssignToMeClicked();
	FReply OnPostIssueToRedmineClicked();

	FString GetSelectedAttachmentSummaryText() const;

private:
	TArray<TSharedPtr<FWcwSnapJumpEditorRowItem>> SnapshotItems;
	TSharedPtr<FWcwSnapJumpEditorRowItem> CurrentSelectedItem;

	TSharedPtr<SListView<TSharedPtr<FWcwSnapJumpEditorRowItem>>> ListViewWidget;
	TSharedPtr<class SImage> SelectedPreviewImage;
	TSharedPtr<class STextBlock> DetailReportText;
	TSharedPtr<class SWidgetSwitcher> RightTabSwitcher;

	// Redmine UI state & widgets
	int32 ActiveRightTabIndex = 0; // 0 = Telemetry Report, 1 = Redmine Ticket Post
	bool bShowApiKeyEditorInline = false;
	bool bIsRedmineConnected = false;
	bool bIsHttpBusy = false;

	int32 CurrentRedmineUserId = 0;
	FString ConnectedUserSummary;
	FString RedmineFeedbackMessage;
	bool bRedmineFeedbackIsError = false;
	FString LastCreatedIssueUrl;
	int32 LastCreatedIssueId = 0;

	bool bPrefixSubjectWithTag = true;
	bool bAppendTelemetryToDescription = true;
	FString LastAutoSubjectPrefix;

	FString ParentVerifyStatusText;
	bool bParentVerifyIsError = false;

	TArray<FWcwRedmineOptionItem> CachedPriorities;
	int32 CachedDefaultPriorityId = 0;
	TArray<FWcwRedmineOptionItem> CachedIssueCustomFields;

	TSharedPtr<class SVerticalBox> DynamicHandlerFieldsContainer;
	TArray<TSharedPtr<FWcwDynamicUiFieldState>> DynamicUiFieldStates;

	TArray<TSharedPtr<FWcwRedmineOptionItem>> TrackerOptions;
	TSharedPtr<FWcwRedmineOptionItem> SelectedTrackerOption;
	TSharedPtr<SComboBox<TSharedPtr<FWcwRedmineOptionItem>>> TrackerComboBox;

	TArray<TSharedPtr<FWcwRedmineOptionItem>> CategoryOptions;
	TSharedPtr<FWcwRedmineOptionItem> SelectedCategoryOption;
	TSharedPtr<SComboBox<TSharedPtr<FWcwRedmineOptionItem>>> CategoryComboBox;

	TArray<TSharedPtr<FWcwRedmineOptionItem>> AssigneeOptions;
	TSharedPtr<FWcwRedmineOptionItem> SelectedAssigneeOption;
	TSharedPtr<SComboBox<TSharedPtr<FWcwRedmineOptionItem>>> AssigneeComboBox;

	TArray<TSharedPtr<FWcwRedmineOptionItem>> ParentPresetOptions;
	TSharedPtr<FWcwRedmineOptionItem> SelectedParentPresetOption;
	TSharedPtr<SComboBox<TSharedPtr<FWcwRedmineOptionItem>>> ParentPresetComboBox;

	TSharedPtr<class SEditableTextBox> ApiKeyInputBox;
	TSharedPtr<class SEditableTextBox> ParentIssueIdInputBox;
	TSharedPtr<class SEditableTextBox> SubjectInputBox;
	TSharedPtr<class SMultiLineEditableTextBox> DescriptionInputBox;
};
