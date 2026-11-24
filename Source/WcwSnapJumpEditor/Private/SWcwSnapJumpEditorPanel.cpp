// Copyright (c) 2026 WinterCrownWORKS. All Rights Reserved.
// Licensed under the MIT License.

#include "SWcwSnapJumpEditorPanel.h"
#include "WcwSnapJumpModule.h"
#include "WcwSnapJumpLibrary.h"
#include "WcwSnapJumpSettings.h"
#include "WcwSnapJumpRedmineSettings.h"
#include "Editor.h"
#include "EditorLevelUtils.h"
#include "Engine/LevelStreaming.h"
#include "ISettingsModule.h"
#include "LevelEditorViewport.h"
#include "Settings/ProjectPackagingSettings.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Factories/DataTableFactory.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "Types/SlateEnums.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SWcwSnapJumpEditorPanel"

namespace WcwRedminePanelInternal
{
	static TSharedPtr<FWcwRedmineOptionItem> FindOptionByIdOrName(
		const TArray<TSharedPtr<FWcwRedmineOptionItem>>& Options,
		const FString& InQuery)
	{
		const FString Query = InQuery.TrimStartAndEnd();
		if (Query.IsEmpty())
		{
			return nullptr;
		}

		FString DigitsCandidate = Query;
		DigitsCandidate.RemoveFromStart(TEXT("#"));
		if (!DigitsCandidate.IsEmpty() && DigitsCandidate.IsNumeric())
		{
			const int32 TargetId = FCString::Atoi(*DigitsCandidate);
			for (const TSharedPtr<FWcwRedmineOptionItem>& Opt : Options)
			{
				if (Opt.IsValid() && Opt->Id == TargetId)
				{
					return Opt;
				}
			}
		}

		for (const TSharedPtr<FWcwRedmineOptionItem>& Opt : Options)
		{
			if (Opt.IsValid() && Opt->Name.Equals(Query, ESearchCase::IgnoreCase))
			{
				return Opt;
			}
		}

		for (const TSharedPtr<FWcwRedmineOptionItem>& Opt : Options)
		{
			if (Opt.IsValid() && Opt->Id > 0 && Opt->Name.Contains(Query, ESearchCase::IgnoreCase))
			{
				return Opt;
			}
		}

		return nullptr;
	}

	static const FWcwRedmineOptionItem* FindCustomFieldDef(
		const TArray<FWcwRedmineOptionItem>& Defs,
		const FString& InFieldKey)
	{
		const FString Key = InFieldKey.TrimStartAndEnd();
		if (Key.IsEmpty())
		{
			return nullptr;
		}

		for (const FWcwRedmineOptionItem& Def : Defs)
		{
			if (Def.Id > 0 && Def.Name.Equals(Key, ESearchCase::CaseSensitive))
			{
				return &Def;
			}
		}
		for (const FWcwRedmineOptionItem& Def : Defs)
		{
			if (Def.Id > 0 && Def.Name.Equals(Key, ESearchCase::IgnoreCase))
			{
				return &Def;
			}
		}

		FString Digits = Key;
		Digits.RemoveFromStart(TEXT("#"));
		if (!Digits.IsEmpty() && Digits.IsNumeric())
		{
			const int32 TargetId = FCString::Atoi(*Digits);
			for (const FWcwRedmineOptionItem& Def : Defs)
			{
				if (Def.Id == TargetId)
				{
					return &Def;
				}
			}
		}
		return nullptr;
	}

	static TSharedPtr<FWcwIssueTrackerUiOption> FindUiOption(
		const TArray<TSharedPtr<FWcwIssueTrackerUiOption>>& Options,
		const FString& InDesired)
	{
		for (const TSharedPtr<FWcwIssueTrackerUiOption>& Opt : Options)
		{
			if (Opt.IsValid() && Opt->Value.Equals(InDesired, ESearchCase::CaseSensitive))
			{
				return Opt;
			}
		}
		for (const TSharedPtr<FWcwIssueTrackerUiOption>& Opt : Options)
		{
			if (Opt.IsValid() && Opt->Value.Equals(InDesired, ESearchCase::IgnoreCase))
			{
				return Opt;
			}
		}
		if (!InDesired.IsEmpty())
		{
			for (const TSharedPtr<FWcwIssueTrackerUiOption>& Opt : Options)
			{
				if (Opt.IsValid() && Opt->DisplayLabel.Equals(InDesired, ESearchCase::IgnoreCase))
				{
					return Opt;
				}
			}
			for (const TSharedPtr<FWcwIssueTrackerUiOption>& Opt : Options)
			{
				if (Opt.IsValid() && !Opt->Value.IsEmpty() && Opt->DisplayLabel.Contains(InDesired, ESearchCase::IgnoreCase))
				{
					return Opt;
				}
			}
		}
		return nullptr;
	}

	static bool IsTokenInPipeList(const FString& PipeSeparated, const FString& Token)
	{
		TArray<FString> Parts;
		PipeSeparated.ParseIntoArray(Parts, TEXT("|"), true);
		for (const FString& Part : Parts)
		{
			if (Part.TrimStartAndEnd().Equals(Token, ESearchCase::CaseSensitive))
			{
				return true;
			}
		}
		return false;
	}

	static FString ToggleTokenInPipeList(const FString& PipeSeparated, const FString& Token, bool bChecked)
	{
		TArray<FString> Parts;
		PipeSeparated.ParseIntoArray(Parts, TEXT("|"), true);
		TArray<FString> CleanParts;
		bool bAlreadyPresent = false;
		for (const FString& Part : Parts)
		{
			const FString Trimmed = Part.TrimStartAndEnd();
			if (Trimmed.IsEmpty())
			{
				continue;
			}
			if (Trimmed.Equals(Token, ESearchCase::CaseSensitive))
			{
				bAlreadyPresent = true;
				if (bChecked)
				{
					CleanParts.Add(Trimmed);
				}
			}
			else
			{
				CleanParts.Add(Trimmed);
			}
		}
		if (bChecked && !bAlreadyPresent && !Token.IsEmpty())
		{
			CleanParts.Add(Token);
		}
		return FString::Join(CleanParts, TEXT("|"));
	}
}

class SWcwSnapJumpListRow : public SMultiColumnTableRow<TSharedPtr<FWcwSnapJumpEditorRowItem>>
{
public:
	SLATE_BEGIN_ARGS(SWcwSnapJumpListRow) {}
		SLATE_ARGUMENT(TSharedPtr<FWcwSnapJumpEditorRowItem>, Item)
		SLATE_ARGUMENT(SWcwSnapJumpEditorPanel*, OwnerPanel)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		Item = InArgs._Item;
		OwnerPanel = InArgs._OwnerPanel;
		SMultiColumnTableRow<TSharedPtr<FWcwSnapJumpEditorRowItem>>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		if (!Item.IsValid())
		{
			return SNullWidget::NullWidget;
		}

		if (ColumnName == "Thumb")
		{
			const FSlateBrush* BrushToRender = Item->ThumbnailBrush.IsValid()
				? Item->ThumbnailBrush.Get()
				: FCoreStyle::Get().GetDefaultBrush();

			return SNew(SBox)
				.Padding(FMargin(4.0f, 2.0f))
				.WidthOverride(92.0f)
				.HeightOverride(54.0f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ContentPadding(FMargin(1.0f))
					.ToolTipText(LOCTEXT("ClickThumbToJumpTooltip", "Click Thumbnail to Jump (PIE: Runtime Jump / Editor: Move Camera)"))
					.OnClicked_Lambda([this]()
					{
						if (OwnerPanel && Item.IsValid())
						{
							OwnerPanel->ExecuteJumpForItem(Item);
						}
						return FReply::Handled();
					})
					[
						SNew(SScaleBox)
						.Stretch(EStretch::ScaleToFit)
						[
							SNew(SImage)
							.Image(BrushToRender)
						]
					]
				];
		}

		if (ColumnName == "Redmine")
		{
			if (Item->PostedIssueId > 0)
			{
				return SNew(SBox)
					.Padding(FMargin(4.0f, 2.0f))
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(SButton)
						.ContentPadding(FMargin(6.0f, 2.0f))
						.ToolTipText(FText::FromString(FString::Printf(TEXT("Open posted Redmine ticket in browser:\n%s"), *Item->PostedIssueUrl)))
						.OnClicked_Lambda([this]()
						{
							if (OwnerPanel && Item.IsValid())
							{
								OwnerPanel->OpenPostedIssueInBrowser(Item);
							}
							return FReply::Handled();
						})
						[
							SNew(STextBlock)
							.Text(FText::FromString(FString::Printf(TEXT("#%d"), Item->PostedIssueId)))
							.ColorAndOpacity(FLinearColor(0.35f, 0.78f, 1.0f))
						]
					];
			}

			return SNew(SBox)
				.Padding(FMargin(6.0f, 3.0f))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("-")))
					.ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f))
				];
		}

		const FWcwSnapJumpData& Snap = Item->SnapData;
		FString CellText;
		if (ColumnName == "Tag")
		{
			CellText = Snap.CaptureTag;
		}
		else if (ColumnName == "Level")
		{
			CellText = Snap.LevelName;
		}
		else if (ColumnName == "Platform")
		{
			if (!Snap.Environment.PlatformName.IsEmpty())
			{
				CellText = Snap.Environment.GpuAdapterName.IsEmpty()
					? Snap.Environment.PlatformName
					: FString::Printf(TEXT("%s (%s)"), *Snap.Environment.PlatformName, *Snap.Environment.GpuAdapterName);
			}
			else
			{
				CellText = TEXT("-");
			}
		}
		else if (ColumnName == "FPS")
		{
			CellText = FString::Printf(TEXT("%.1f (%.2f ms)"), Snap.Stats.FPS, Snap.Stats.FrameTimeMs);
		}
		else if (ColumnName == "GPU")
		{
			CellText = FString::Printf(TEXT("%.2f ms"), Snap.Stats.GpuTimeMs);
		}
		else if (ColumnName == "TopPass")
		{
			CellText = Snap.GpuTop10Passes.Num() > 0
				? FString::Printf(TEXT("%s (%.2fms)"), *Snap.GpuTop10Passes[0].PassName, Snap.GpuTop10Passes[0].DurationMs)
				: TEXT("-");
		}
		else if (ColumnName == "Memory")
		{
			CellText = FString::Printf(TEXT("RAM %.0fMB / VRAM %.0fMB"), Snap.Memory.UsedPhysicalMemoryMB, Snap.Memory.TextureMemoryUsedMB);
		}
		else if (ColumnName == "Timestamp")
		{
			CellText = Snap.Timestamp;
		}

		return SNew(SBox)
			.Padding(FMargin(6.0f, 3.0f))
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(CellText))
			];
	}

private:
	TSharedPtr<FWcwSnapJumpEditorRowItem> Item;
	SWcwSnapJumpEditorPanel* OwnerPanel = nullptr;
};

SWcwSnapJumpEditorPanel::~SWcwSnapJumpEditorPanel()
{
	SnapshotItems.Reset();
}

void SWcwSnapJumpEditorPanel::Construct(const FArguments& InArgs)
{
	const UWcwSnapJumpRedmineSettings* RedmineProjSettings = GetDefault<UWcwSnapJumpRedmineSettings>();
	const UWcwSnapJumpRedmineUserSettings* RedmineUserSettings = GetDefault<UWcwSnapJumpRedmineUserSettings>();

	if (RedmineProjSettings)
	{
		bPrefixSubjectWithTag = RedmineProjSettings->bPrefixSubjectWithCaptureTag;
		bAppendTelemetryToDescription = RedmineProjSettings->bAppendTelemetrySummaryToDescription;
	}
	bShowApiKeyEditorInline = (!RedmineUserSettings || RedmineUserSettings->ApiAccessKey.TrimStartAndEnd().IsEmpty());

	CategoryOptions.Add(MakeShared<FWcwRedmineOptionItem>(0, TEXT("(None)")));
	SelectedCategoryOption = CategoryOptions[0];

	AssigneeOptions.Add(MakeShared<FWcwRedmineOptionItem>(0, TEXT("(Unassigned)")));
	SelectedAssigneeOption = AssigneeOptions[0];

	RebuildParentPresetOptions();

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("RefreshBtn", "Refresh Snapshot List"))
				.OnClicked(this, &SWcwSnapJumpEditorPanel::OnRefreshClicked)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("MoveCamBtn", "Jump / Move Camera to Selected"))
				.OnClicked(this, &SWcwSnapJumpEditorPanel::OnMoveEditorCameraClicked)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SButton)
				.ToolTipText(LOCTEXT("SyncTableTooltip", "Imports PNGs as UTexture2D assets, populates the DataTable, saves DefaultSnapJumpDataTable to DefaultGame.ini, and registers the folder in DirectoriesToAlwaysCook."))
				.Text(LOCTEXT("SyncTableBtn", "Import Textures & Build DataTable (For Package)"))
				.OnClicked(this, &SWcwSnapJumpEditorPanel::OnSyncToDefaultDataTableClicked)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNullWidget::NullWidget
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(STextBlock)
				.Visibility_Lambda([]()
				{
					const UWcwSnapJumpRedmineSettings* S = GetDefault<UWcwSnapJumpRedmineSettings>();
					return (S && S->bEnableRedmineIntegration) ? EVisibility::Visible : EVisibility::Collapsed;
				})
				.Text_Lambda([this]()
				{
					const UWcwSnapJumpRedmineSettings* S = GetDefault<UWcwSnapJumpRedmineSettings>();
					const UWcwSnapJumpRedmineUserSettings* U = GetDefault<UWcwSnapJumpRedmineUserSettings>();
					if (!S || S->GetNormalizedBaseUrl().IsEmpty() || S->ProjectIdentifier.TrimStartAndEnd().IsEmpty())
					{
						return LOCTEXT("RedmineUnconfigured", "Redmine: URL/Project Unset");
					}
					if (!U || U->ApiAccessKey.TrimStartAndEnd().IsEmpty())
					{
						return LOCTEXT("RedmineNoKey", "Redmine: API Key Unset");
					}
					if (bIsRedmineConnected)
					{
						return FText::FromString(FString::Printf(TEXT("Redmine Connected: %s"), *ConnectedUserSummary));
					}
					return LOCTEXT("RedmineStandby", "Redmine: Standby");
				})
				.ColorAndOpacity_Lambda([this]()
				{
					if (bIsRedmineConnected)
					{
						return FSlateColor(FLinearColor(0.30f, 0.85f, 0.45f));
					}
					return FSlateColor(FLinearColor(0.95f, 0.72f, 0.25f));
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Visibility_Lambda([]()
				{
					const UWcwSnapJumpRedmineSettings* S = GetDefault<UWcwSnapJumpRedmineSettings>();
					return (S && S->bEnableRedmineIntegration) ? EVisibility::Visible : EVisibility::Collapsed;
				})
				.Text(LOCTEXT("OpenRedmineSettingsBtn", "Redmine Settings..."))
				.ToolTipText(LOCTEXT("OpenRedmineSettingsTooltip", "Open Project Settings -> Extension -> Wcw Snap Jump (Redmine Project)"))
				.OnClicked(this, &SWcwSnapJumpEditorPanel::OnOpenRedmineProjectSettingsClicked)
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(8.0f, 0.0f, 8.0f, 8.0f)
		[
			SNew(SSplitter)
			+ SSplitter::Slot()
			.Value(0.60f)
			[
				SAssignNew(ListViewWidget, SListView<TSharedPtr<FWcwSnapJumpEditorRowItem>>)
				.ListItemsSource(&SnapshotItems)
				.OnGenerateRow(this, &SWcwSnapJumpEditorPanel::OnGenerateRowForList)
				.OnSelectionChanged(this, &SWcwSnapJumpEditorPanel::OnSelectionChanged)
				.OnMouseButtonDoubleClick(this, &SWcwSnapJumpEditorPanel::OnListMouseButtonDoubleClick)
				.HeaderRow
				(
					SNew(SHeaderRow)
					+ SHeaderRow::Column("Thumb").DefaultLabel(LOCTEXT("ColThumb", "Thumb (Click=Jump)")).FixedWidth(100.0f)
					+ SHeaderRow::Column("Tag").DefaultLabel(LOCTEXT("ColTag", "Tag")).FillWidth(0.12f)
					+ SHeaderRow::Column("Level").DefaultLabel(LOCTEXT("ColLevel", "Level")).FillWidth(0.12f)
					+ SHeaderRow::Column("Platform").DefaultLabel(LOCTEXT("ColPlatform", "Platform / GPU")).FillWidth(0.17f)
					+ SHeaderRow::Column("FPS").DefaultLabel(LOCTEXT("ColFPS", "FPS / Frame")).FillWidth(0.12f)
					+ SHeaderRow::Column("GPU").DefaultLabel(LOCTEXT("ColGPU", "GPU ms")).FillWidth(0.08f)
					+ SHeaderRow::Column("TopPass").DefaultLabel(LOCTEXT("ColTopPass", "GPU #1 Bottleneck")).FillWidth(0.16f)
					+ SHeaderRow::Column("Memory").DefaultLabel(LOCTEXT("ColMemory", "Memory")).FillWidth(0.11f)
					+ SHeaderRow::Column("Timestamp").DefaultLabel(LOCTEXT("ColTime", "Timestamp")).FillWidth(0.09f)
					+ SHeaderRow::Column("Redmine").DefaultLabel(LOCTEXT("ColRedmine", "Redmine")).FixedWidth(78.0f)
				)
			]
			+ SSplitter::Slot()
			.Value(0.40f)
			[
				SNew(SBorder)
				.Padding(10.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					.Padding(0.0f, 0.0f, 0.0f, 6.0f)
					[
						SNew(SButton)
						.ContentPadding(FMargin(2.0f))
						.ToolTipText(LOCTEXT("ClickPreviewToJumpTooltip", "Click preview image to Jump to this location"))
						.OnClicked_Lambda([this]()
						{
							if (CurrentSelectedItem.IsValid())
							{
								ExecuteJumpForItem(CurrentSelectedItem);
							}
							return FReply::Handled();
						})
						[
							SNew(SBox)
							.WidthOverride(288.0f)
							.HeightOverride(162.0f)
							[
								SNew(SScaleBox)
								.Stretch(EStretch::ScaleToFit)
								[
									SAssignNew(SelectedPreviewImage, SImage)
									.Image(FCoreStyle::Get().GetDefaultBrush())
								]
							]
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 6.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text_Lambda([this]()
							{
								if (!CurrentSelectedItem.IsValid())
								{
									return LOCTEXT("NoSnapSelected", "No snapshot selected");
								}
								return FText::FromString(FString::Printf(
									TEXT("Selected: %s (%s)"),
									*CurrentSelectedItem->SnapData.CaptureTag,
									*CurrentSelectedItem->SnapData.LevelName));
							})
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SButton)
							.Visibility_Lambda([this]()
							{
								return (CurrentSelectedItem.IsValid() && CurrentSelectedItem->PostedIssueId > 0)
									? EVisibility::Visible
									: EVisibility::Collapsed;
							})
							.Text_Lambda([this]()
							{
								const int32 Id = CurrentSelectedItem.IsValid() ? CurrentSelectedItem->PostedIssueId : 0;
								return FText::FromString(FString::Printf(TEXT("Posted #%d (Open Browser)"), Id));
							})
							.OnClicked_Lambda([this]()
							{
								if (CurrentSelectedItem.IsValid())
								{
									OpenPostedIssueInBrowser(CurrentSelectedItem);
								}
								return FReply::Handled();
							})
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 8.0f)
					[
						SNew(SHorizontalBox)
						.Visibility_Lambda([]()
						{
							const UWcwSnapJumpRedmineSettings* S = GetDefault<UWcwSnapJumpRedmineSettings>();
							return (S && S->bEnableRedmineIntegration) ? EVisibility::Visible : EVisibility::Collapsed;
						})
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.Padding(0.0f, 0.0f, 4.0f, 0.0f)
						[
							SNew(SButton)
							.HAlign(HAlign_Center)
							.Text(LOCTEXT("TabTelemetryBtn", "Telemetry Report"))
							.ButtonColorAndOpacity_Lambda([this]()
							{
								return ActiveRightTabIndex == 0
									? FLinearColor(0.20f, 0.48f, 0.85f, 1.0f)
									: FLinearColor(0.22f, 0.22f, 0.22f, 1.0f);
							})
							.OnClicked_Lambda([this]()
							{
								ActiveRightTabIndex = 0;
								return FReply::Handled();
							})
						]
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.Padding(4.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SButton)
							.HAlign(HAlign_Center)
							.Text(LOCTEXT("TabRedmineBtn", "Redmine Ticket Post"))
							.ButtonColorAndOpacity_Lambda([this]()
							{
								return ActiveRightTabIndex == 1
									? FLinearColor(0.20f, 0.48f, 0.85f, 1.0f)
									: FLinearColor(0.22f, 0.22f, 0.22f, 1.0f);
							})
							.OnClicked_Lambda([this]()
							{
								ActiveRightTabIndex = 1;
								RebuildParentPresetOptions();
								RebuildDynamicHandlerUiFields(true);
								if (!bIsRedmineConnected && !bIsHttpBusy)
								{
									StartVerifyConnectionAndFetchMeta();
								}
								return FReply::Handled();
							})
						]
					]
					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						SAssignNew(RightTabSwitcher, SWidgetSwitcher)
						.WidgetIndex_Lambda([this]()
						{
							const UWcwSnapJumpRedmineSettings* S = GetDefault<UWcwSnapJumpRedmineSettings>();
							return (S && S->bEnableRedmineIntegration) ? ActiveRightTabIndex : 0;
						})
						+ SWidgetSwitcher::Slot()
						[
							SNew(SScrollBox)
							+ SScrollBox::Slot()
							[
								SAssignNew(DetailReportText, STextBlock)
								.Text(LOCTEXT("SelectPrompt", "Select a snapshot row on the left (or click its thumbnail to jump) to inspect its preview, full stat, memory, GPU Top 10, and custom Title state."))
							]
						]
						+ SWidgetSwitcher::Slot()
						[
							BuildRedminePostTabWidget()
						]
					]
				]
			]
		]
	];

	OnRefreshClicked();
	RebuildDynamicHandlerUiFields(false);

	RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateLambda([this](double InTime, float InDelta)
	{
		return HandlePostConstructInit(InTime, InDelta);
	}));
}

EActiveTimerReturnType SWcwSnapJumpEditorPanel::HandlePostConstructInit(double InCurrentTime, float InDeltaTime)
{
	const UWcwSnapJumpRedmineSettings* ProjSettings = GetDefault<UWcwSnapJumpRedmineSettings>();
	const UWcwSnapJumpRedmineUserSettings* UserSettings = GetDefault<UWcwSnapJumpRedmineUserSettings>();
	if (ProjSettings && ProjSettings->bEnableRedmineIntegration &&
	    !ProjSettings->GetNormalizedBaseUrl().IsEmpty() &&
	    !ProjSettings->ProjectIdentifier.TrimStartAndEnd().IsEmpty() &&
	    UserSettings && !UserSettings->ApiAccessKey.TrimStartAndEnd().IsEmpty())
	{
		StartVerifyConnectionAndFetchMeta();
	}
	return EActiveTimerReturnType::Stop;
}

TSharedRef<SWidget> SWcwSnapJumpEditorPanel::BuildRedminePostTabWidget()
{
	const UWcwSnapJumpRedmineUserSettings* UserSettings = GetDefault<UWcwSnapJumpRedmineUserSettings>();
	const FString InitialApiKey = UserSettings ? UserSettings->ApiAccessKey : FString();
	const FString InitialParentIdStr = (UserSettings && UserSettings->LastUsedParentIssueId > 0)
		? FString::FromInt(UserSettings->LastUsedParentIssueId)
		: FString();

	return SNew(SScrollBox)
		+ SScrollBox::Slot()
		.Padding(2.0f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(SBorder)
				.Padding(8.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.AutoWrapText(true)
							.Text_Lambda([]()
							{
								const UWcwSnapJumpRedmineSettings* S = GetDefault<UWcwSnapJumpRedmineSettings>();
								if (!S || S->GetNormalizedBaseUrl().IsEmpty() || S->ProjectIdentifier.TrimStartAndEnd().IsEmpty())
								{
									return LOCTEXT("TargetNotSetLabel", "Target Redmine URL / ProjectIdentifier not set (click 'Redmine Settings...').");
								}
								return FText::FromString(FString::Printf(
									TEXT("Target: %s  |  Project: %s"),
									*S->GetNormalizedBaseUrl(),
									*S->ProjectIdentifier.TrimStartAndEnd()));
							})
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(4.0f, 0.0f, 4.0f, 0.0f)
						[
							SNew(SButton)
							.Text(LOCTEXT("SyncMetaBtn", "Sync Meta"))
							.ToolTipText(LOCTEXT("SyncMetaTooltip", "Fetch latest Trackers, Categories, Priorities, Assignees & Custom Fields from Redmine"))
							.IsEnabled_Lambda([this]() { return !bIsHttpBusy; })
							.OnClicked_Lambda([this]()
							{
								RebuildParentPresetOptions();
								RebuildDynamicHandlerUiFields(true);
								StartVerifyConnectionAndFetchMeta();
								return FReply::Handled();
							})
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SButton)
							.Text(LOCTEXT("ToggleApiKeyBtn", "API Key"))
							.ToolTipText(LOCTEXT("ToggleApiKeyTooltip", "Show/Hide local Redmine API Access Key setup (stored in Saved/Config)"))
							.OnClicked_Lambda([this]()
							{
								bShowApiKeyEditorInline = !bShowApiKeyEditorInline;
								return FReply::Handled();
							})
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 8.0f, 0.0f, 0.0f)
					[
						SNew(SVerticalBox)
						.Visibility_Lambda([this]()
						{
							const UWcwSnapJumpRedmineUserSettings* U = GetDefault<UWcwSnapJumpRedmineUserSettings>();
							const bool bEmptyKey = (!U || U->ApiAccessKey.TrimStartAndEnd().IsEmpty());
							return (bShowApiKeyEditorInline || bEmptyKey) ? EVisibility::Visible : EVisibility::Collapsed;
						})
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 0.0f, 0.0f, 4.0f)
						[
							SNew(STextBlock)
							.AutoWrapText(true)
							.Text(LOCTEXT("ApiKeyHelpText", "Personal API Access Key (from Redmine '/my/account'). Saved strictly to local Saved/Config/ (never committed to Git/P4; works with 2FA):"))
							.ColorAndOpacity(FLinearColor(0.75f, 0.75f, 0.75f))
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.Padding(0.0f, 0.0f, 6.0f, 0.0f)
							[
								SAssignNew(ApiKeyInputBox, SEditableTextBox)
								.IsPassword(true)
								.Text(FText::FromString(InitialApiKey))
								.HintText(LOCTEXT("ApiKeyHint", "Paste Redmine API Access Key..."))
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SButton)
								.Text(LOCTEXT("SaveKeyConnectBtn", "Test & Save Local"))
								.IsEnabled_Lambda([this]() { return !bIsHttpBusy; })
								.OnClicked(this, &SWcwSnapJumpEditorPanel::OnSaveApiKeyAndConnectClicked)
							]
						]
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 3.0f)
					[
						SNew(STextBlock).Text(LOCTEXT("TrackerLabel", "Tracker * (Dynamic)"))
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SAssignNew(TrackerComboBox, SComboBox<TSharedPtr<FWcwRedmineOptionItem>>)
						.OptionsSource(&TrackerOptions)
						.OnGenerateWidget_Lambda([](TSharedPtr<FWcwRedmineOptionItem> Item)
						{
							return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? Item->Name : TEXT("-")));
						})
						.OnSelectionChanged_Lambda([this](TSharedPtr<FWcwRedmineOptionItem> NewItem, ESelectInfo::Type)
						{
							SelectedTrackerOption = NewItem;
							if (NewItem.IsValid())
							{
								if (UWcwSnapJumpRedmineUserSettings* U = GetMutableDefault<UWcwSnapJumpRedmineUserSettings>())
								{
									U->LastSelectedTrackerId = NewItem->Id;
									U->SaveUserConfig();
								}
							}
						})
						[
							SNew(STextBlock)
							.Text_Lambda([this]()
							{
								return FText::FromString(SelectedTrackerOption.IsValid() ? SelectedTrackerOption->Name : TEXT("(Click Sync Meta)"));
							})
						]
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(6.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 3.0f)
					[
						SNew(STextBlock).Text(LOCTEXT("CategoryLabel", "Category (Dynamic)"))
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SAssignNew(CategoryComboBox, SComboBox<TSharedPtr<FWcwRedmineOptionItem>>)
						.OptionsSource(&CategoryOptions)
						.OnGenerateWidget_Lambda([](TSharedPtr<FWcwRedmineOptionItem> Item)
						{
							return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? Item->Name : TEXT("-")));
						})
						.OnSelectionChanged_Lambda([this](TSharedPtr<FWcwRedmineOptionItem> NewItem, ESelectInfo::Type)
						{
							SelectedCategoryOption = NewItem;
							if (NewItem.IsValid())
							{
								if (UWcwSnapJumpRedmineUserSettings* U = GetMutableDefault<UWcwSnapJumpRedmineUserSettings>())
								{
									U->LastSelectedCategoryId = NewItem->Id;
									U->SaveUserConfig();
								}
							}
						})
						[
							SNew(STextBlock)
							.Text_Lambda([this]()
							{
								return FText::FromString(SelectedCategoryOption.IsValid() ? SelectedCategoryOption->Name : TEXT("(None)"));
							})
						]
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 3.0f)
				[
					SNew(STextBlock).Text(LOCTEXT("AssigneeLabel", "Assignee (Dynamic)"))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SAssignNew(AssigneeComboBox, SComboBox<TSharedPtr<FWcwRedmineOptionItem>>)
						.OptionsSource(&AssigneeOptions)
						.OnGenerateWidget_Lambda([](TSharedPtr<FWcwRedmineOptionItem> Item)
						{
							return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? Item->Name : TEXT("-")));
						})
						.OnSelectionChanged_Lambda([this](TSharedPtr<FWcwRedmineOptionItem> NewItem, ESelectInfo::Type)
						{
							SelectedAssigneeOption = NewItem;
							if (NewItem.IsValid())
							{
								if (UWcwSnapJumpRedmineUserSettings* U = GetMutableDefault<UWcwSnapJumpRedmineUserSettings>())
								{
									U->LastSelectedAssigneeId = NewItem->Id;
									U->SaveUserConfig();
								}
							}
						})
						[
							SNew(STextBlock)
							.Text_Lambda([this]()
							{
								return FText::FromString(SelectedAssigneeOption.IsValid() ? SelectedAssigneeOption->Name : TEXT("(Unassigned)"));
							})
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("AssignToMeBtn", "Assign to Me"))
						.ToolTipText(LOCTEXT("AssignToMeTooltip", "Select your own Redmine user account as Assignee"))
						.IsEnabled_Lambda([this]() { return CurrentRedmineUserId > 0; })
						.OnClicked(this, &SWcwSnapJumpEditorPanel::OnAssignToMeClicked)
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 3.0f)
				[
					SNew(STextBlock).Text(LOCTEXT("ParentIssueLabel", "Parent Issue (Preset or #ID for Child Ticket)"))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(0.65f)
					.Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SAssignNew(ParentPresetComboBox, SComboBox<TSharedPtr<FWcwRedmineOptionItem>>)
						.OptionsSource(&ParentPresetOptions)
						.OnGenerateWidget_Lambda([](TSharedPtr<FWcwRedmineOptionItem> Item)
						{
							return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? Item->Name : TEXT("-")));
						})
						.OnSelectionChanged_Lambda([this](TSharedPtr<FWcwRedmineOptionItem> NewItem, ESelectInfo::Type SelectType)
						{
							SelectedParentPresetOption = NewItem;
							if (!NewItem.IsValid() || SelectType == ESelectInfo::Direct)
							{
								return;
							}
							if (NewItem->Id == 0)
							{
								if (ParentIssueIdInputBox.IsValid())
								{
									ParentIssueIdInputBox->SetText(FText::GetEmpty());
								}
								ParentVerifyStatusText.Reset();
								if (UWcwSnapJumpRedmineUserSettings* U = GetMutableDefault<UWcwSnapJumpRedmineUserSettings>())
								{
									U->LastUsedParentIssueId = 0;
									U->SaveUserConfig();
								}
							}
							else if (NewItem->Id > 0)
							{
								if (ParentIssueIdInputBox.IsValid())
								{
									ParentIssueIdInputBox->SetText(FText::FromString(FString::FromInt(NewItem->Id)));
								}
								if (UWcwSnapJumpRedmineUserSettings* U = GetMutableDefault<UWcwSnapJumpRedmineUserSettings>())
								{
									U->LastUsedParentIssueId = NewItem->Id;
									U->SaveUserConfig();
								}
								TriggerVerifyParentTicket(NewItem->Id);
							}
						})
						[
							SNew(STextBlock)
							.Text_Lambda([this]()
							{
								return FText::FromString(SelectedParentPresetOption.IsValid() ? SelectedParentPresetOption->Name : TEXT("(No Parent)"));
							})
						]
					]
					+ SHorizontalBox::Slot()
					.FillWidth(0.35f)
					[
						SAssignNew(ParentIssueIdInputBox, SEditableTextBox)
						.Text(FText::FromString(InitialParentIdStr))
						.HintText(LOCTEXT("ParentIdHint", "#ID (e.g. 2)"))
						.OnTextCommitted_Lambda([this](const FText& NewText, ETextCommit::Type)
						{
							FString Clean = NewText.ToString().TrimStartAndEnd();
							Clean.RemoveFromStart(TEXT("#"));
							const int32 ParsedId = Clean.IsEmpty() ? 0 : FCString::Atoi(*Clean);
							if (UWcwSnapJumpRedmineUserSettings* U = GetMutableDefault<UWcwSnapJumpRedmineUserSettings>())
							{
								U->LastUsedParentIssueId = FMath::Max(0, ParsedId);
								U->SaveUserConfig();
							}
							if (ParsedId > 0)
							{
								TriggerVerifyParentTicket(ParsedId);
							}
							else
							{
								ParentVerifyStatusText.Reset();
							}
						})
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2.0f, 3.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Visibility_Lambda([this]() { return ParentVerifyStatusText.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
					.Text_Lambda([this]() { return FText::FromString(ParentVerifyStatusText); })
					.ColorAndOpacity_Lambda([this]()
					{
						return bParentVerifyIsError
							? FSlateColor(FLinearColor(1.0f, 0.42f, 0.38f))
							: FSlateColor(FLinearColor(0.35f, 0.85f, 0.50f));
					})
				]
			]

			// --- Dynamic Title-Defined UI Controls (Priority, Custom Fields Text/List/Multi/Bool) ---
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(SBorder)
				.Padding(8.0f)
				.Visibility_Lambda([this]()
				{
					return DynamicUiFieldStates.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed;
				})
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 6.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("TitleCustomUiSectionHeader", "Title Custom Fields (Handler Override)"))
							.ColorAndOpacity(FLinearColor(0.45f, 0.80f, 1.0f))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SButton)
							.ContentPadding(FMargin(6.0f, 1.5f))
							.Text(LOCTEXT("ResetHandlerUiBtn", "Reset"))
							.ToolTipText(LOCTEXT("ResetHandlerUiTooltip", "Reset Title override controls to Handler / Redmine defaults"))
							.OnClicked_Lambda([this]()
							{
								RebuildDynamicHandlerUiFields(false);
								return FReply::Handled();
							})
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SAssignNew(DynamicHandlerFieldsContainer, SVerticalBox)
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 3.0f)
				[
					SNew(STextBlock).Text(LOCTEXT("SubjectLabel", "Subject *"))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SAssignNew(SubjectInputBox, SEditableTextBox)
					.HintText(LOCTEXT("SubjectHint", "Enter brief issue title..."))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([this]() { return bPrefixSubjectWithTag ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
					{
						bPrefixSubjectWithTag = (NewState == ECheckBoxState::Checked);
						SyncSubjectPrefixForSelection(false);
					})
					[
						SNew(STextBlock)
						.Text(LOCTEXT("PrefixTagCheck", "Auto-prefix subject with [CaptureTag]"))
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 3.0f)
				[
					SNew(STextBlock).Text(LOCTEXT("DescLabel", "Description"))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBox)
					.MinDesiredHeight(84.0f)
					.MaxDesiredHeight(140.0f)
					[
						SAssignNew(DescriptionInputBox, SMultiLineEditableTextBox)
						.AutoWrapText(true)
						.HintText(LOCTEXT("DescHint", "Describe the visual glitch, repro condition, or performance spike..."))
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([this]() { return bAppendTelemetryToDescription ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
					{
						bAppendTelemetryToDescription = (NewState == ECheckBoxState::Checked);
					})
					[
						SNew(STextBlock)
						.Text(LOCTEXT("AppendTelemetryCheck", "Append Environment, Transform & GPU Top Passes summary to description"))
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(SBorder)
				.Padding(6.0f)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Text_Lambda([this]() { return FText::FromString(GetSelectedAttachmentSummaryText()); })
					.ColorAndOpacity(FLinearColor(0.78f, 0.78f, 0.78f))
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(SBorder)
				.Padding(8.0f)
				.Visibility_Lambda([this]() { return RedmineFeedbackMessage.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.AutoWrapText(true)
						.Text_Lambda([this]() { return FText::FromString(RedmineFeedbackMessage); })
						.ColorAndOpacity_Lambda([this]()
						{
							return bRedmineFeedbackIsError
								? FSlateColor(FLinearColor(1.0f, 0.42f, 0.38f))
								: FSlateColor(FLinearColor(0.35f, 0.88f, 0.50f));
						})
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Right)
					.Padding(0.0f, 6.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.Visibility_Lambda([this]()
						{
							return (!bRedmineFeedbackIsError && !LastCreatedIssueUrl.IsEmpty())
								? EVisibility::Visible
								: EVisibility::Collapsed;
						})
						.Text_Lambda([this]()
						{
							return FText::FromString(FString::Printf(TEXT("Open Created Ticket #%d in Browser"), LastCreatedIssueId));
						})
						.OnClicked_Lambda([this]()
						{
							if (!LastCreatedIssueUrl.IsEmpty())
							{
								FPlatformProcess::LaunchURL(*LastCreatedIssueUrl, nullptr, nullptr);
							}
							return FReply::Handled();
						})
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			[
				SNew(SButton)
				.ContentPadding(FMargin(14.0f, 6.0f))
				.IsEnabled_Lambda([this]()
				{
					return !bIsHttpBusy && CurrentSelectedItem.IsValid();
				})
				.Text_Lambda([this]()
				{
					if (bIsHttpBusy)
					{
						return LOCTEXT("PostingInProgressBtn", "Uploading & Posting...");
					}
					if (CurrentSelectedItem.IsValid() && CurrentSelectedItem->PostedIssueId > 0)
					{
						return LOCTEXT("RepostBtn", "Post Again as New Ticket");
					}
					return LOCTEXT("PostIssueBtn", "Post Ticket to Redmine");
				})
				.OnClicked(this, &SWcwSnapJumpEditorPanel::OnPostIssueToRedmineClicked)
			]
		];
}

void SWcwSnapJumpEditorPanel::RebuildDynamicHandlerUiFields(bool bPreserveUserEdits)
{
	const FWcwSnapJumpData ActiveSnap = CurrentSelectedItem.IsValid() ? CurrentSelectedItem->SnapData : FWcwSnapJumpData();

	TArray<FWcwIssueTrackerUiField> GatheredFields;
	UWcwSnapJumpLibrary::GatherIssueTrackerUiFields(ActiveSnap, GatheredFields);

	TMap<FString, TPair<FString, bool>> PreviousUserValues;
	if (bPreserveUserEdits)
	{
		for (const TSharedPtr<FWcwDynamicUiFieldState>& OldState : DynamicUiFieldStates)
		{
			if (OldState.IsValid() && OldState->bModifiedByUser)
			{
				const FString MapKey = FString::Printf(
					TEXT("%d:%s"),
					static_cast<int32>(OldState->Def.TargetType),
					*OldState->Def.FieldKey.TrimStartAndEnd());
				PreviousUserValues.Add(MapKey, TPair<FString, bool>(OldState->CurrentValue, true));
			}
		}
	}

	DynamicUiFieldStates.Reset();

	const UWcwSnapJumpRedmineSettings* ProjSettings = GetDefault<UWcwSnapJumpRedmineSettings>();

	for (const FWcwIssueTrackerUiField& FieldDef : GatheredFields)
	{
		if (FieldDef.FieldKey.TrimStartAndEnd().IsEmpty() &&
		    FieldDef.TargetType != EWcwIssueTrackerFieldTarget::RedminePriority)
		{
			continue;
		}

		TSharedPtr<FWcwDynamicUiFieldState> State = MakeShared<FWcwDynamicUiFieldState>();
		State->Def = FieldDef;
		if (State->Def.TargetType == EWcwIssueTrackerFieldTarget::RedminePriority &&
		    State->Def.FieldKey.TrimStartAndEnd().IsEmpty())
		{
			State->Def.FieldKey = TEXT("priority_id");
		}
		if (State->Def.DisplayLabel.TrimStartAndEnd().IsEmpty())
		{
			State->Def.DisplayLabel = State->Def.FieldKey;
		}

		const FString MapKey = FString::Printf(
			TEXT("%d:%s"),
			static_cast<int32>(State->Def.TargetType),
			*State->Def.FieldKey.TrimStartAndEnd());

		const FWcwRedmineOptionItem* MatchedServerCf = (State->Def.TargetType == EWcwIssueTrackerFieldTarget::CustomField)
			? WcwRedminePanelInternal::FindCustomFieldDef(CachedIssueCustomFields, State->Def.FieldKey)
			: nullptr;

		// Build options list for Dropdown / MultiSelect
		if (State->Def.ControlType == EWcwIssueTrackerUiControlType::Dropdown ||
		    State->Def.ControlType == EWcwIssueTrackerUiControlType::MultiSelect)
		{
			if (State->Def.TargetType == EWcwIssueTrackerFieldTarget::RedminePriority)
			{
				if (CachedPriorities.Num() > 0)
				{
					for (const FWcwRedmineOptionItem& P : CachedPriorities)
					{
						const FString Label = P.bIsDefault
							? FString::Printf(TEXT("%s (Default)"), *P.Name)
							: P.Name;
						State->ComboOptions.Add(MakeShared<FWcwIssueTrackerUiOption>(Label, FString::FromInt(P.Id)));
					}
				}
				else if (State->Def.Options.Num() > 0)
				{
					for (const FWcwIssueTrackerUiOption& Opt : State->Def.Options)
					{
						State->ComboOptions.Add(MakeShared<FWcwIssueTrackerUiOption>(Opt.DisplayLabel, Opt.Value));
					}
				}
				else
				{
					State->ComboOptions.Add(MakeShared<FWcwIssueTrackerUiOption>(TEXT("(Server Default)"), TEXT("")));
				}
			}
			else if (State->Def.TargetType == EWcwIssueTrackerFieldTarget::CustomField)
			{
				const bool bNeedEmptySentinel =
					(State->Def.ControlType == EWcwIssueTrackerUiControlType::Dropdown && State->Def.bOmitWhenEmpty);

				if (MatchedServerCf && MatchedServerCf->PossibleValues.Num() > 0)
				{
					if (bNeedEmptySentinel)
					{
						State->ComboOptions.Add(MakeShared<FWcwIssueTrackerUiOption>(TEXT("(Unselected)"), TEXT("")));
					}
					for (const FWcwIssueTrackerUiOption& Pv : MatchedServerCf->PossibleValues)
					{
						State->ComboOptions.Add(MakeShared<FWcwIssueTrackerUiOption>(Pv.DisplayLabel, Pv.Value));
					}
				}
				else
				{
					bool bHasEmptyOpt = false;
					for (const FWcwIssueTrackerUiOption& Opt : State->Def.Options)
					{
						if (Opt.Value.IsEmpty())
						{
							bHasEmptyOpt = true;
							break;
						}
					}
					if (bNeedEmptySentinel && !bHasEmptyOpt)
					{
						State->ComboOptions.Add(MakeShared<FWcwIssueTrackerUiOption>(TEXT("(Unselected)"), TEXT("")));
					}
					for (const FWcwIssueTrackerUiOption& Opt : State->Def.Options)
					{
						State->ComboOptions.Add(MakeShared<FWcwIssueTrackerUiOption>(Opt.DisplayLabel, Opt.Value));
					}
				}
			}
			else
			{
				for (const FWcwIssueTrackerUiOption& Opt : State->Def.Options)
				{
					State->ComboOptions.Add(MakeShared<FWcwIssueTrackerUiOption>(Opt.DisplayLabel, Opt.Value));
				}
			}
		}

		// Determine initial / preserved value
		if (const TPair<FString, bool>* Prev = PreviousUserValues.Find(MapKey))
		{
			State->CurrentValue = Prev->Key;
			State->bModifiedByUser = Prev->Value;
		}
		else
		{
			FString DesiredDefault = State->Def.DefaultValue;

			if (State->Def.TargetType == EWcwIssueTrackerFieldTarget::RedminePriority)
			{
				if (DesiredDefault.IsEmpty() && CachedDefaultPriorityId > 0)
				{
					DesiredDefault = FString::FromInt(CachedDefaultPriorityId);
				}
			}
			else if (State->Def.TargetType == EWcwIssueTrackerFieldTarget::CustomField)
			{
				// If connected to a Redmine project that does NOT enable this optional custom field, default to empty so posting succeeds cleanly
				if (bIsRedmineConnected && MatchedServerCf == nullptr && State->Def.bOmitWhenEmpty)
				{
					DesiredDefault.Reset();
				}
				else if (DesiredDefault.IsEmpty())
				{
					if (ProjSettings)
					{
						if (const FString* ProjDefault = ProjSettings->DefaultCustomFields.Find(State->Def.FieldKey.TrimStartAndEnd()))
						{
							DesiredDefault = *ProjDefault;
						}
					}
					if (DesiredDefault.IsEmpty() && MatchedServerCf && !MatchedServerCf->DefaultValue.IsEmpty())
					{
						DesiredDefault = MatchedServerCf->DefaultValue;
					}
				}
			}

			State->CurrentValue = DesiredDefault;
		}

		if (State->Def.ControlType == EWcwIssueTrackerUiControlType::Dropdown && State->ComboOptions.Num() > 0)
		{
			TSharedPtr<FWcwIssueTrackerUiOption> MatchedOpt = WcwRedminePanelInternal::FindUiOption(State->ComboOptions, State->CurrentValue);
			if (!MatchedOpt.IsValid())
			{
				MatchedOpt = State->ComboOptions[0];
			}
			State->SelectedComboOption = MatchedOpt;
			State->CurrentValue = MatchedOpt.IsValid() ? MatchedOpt->Value : FString();
		}

		DynamicUiFieldStates.Add(MoveTemp(State));
	}

	if (!DynamicHandlerFieldsContainer.IsValid())
	{
		return;
	}

	DynamicHandlerFieldsContainer->ClearChildren();

	// Lay out compact controls (TextBox / Dropdown pair up 2 per row; MultiSelect / CheckBox take a full row)
	int32 Index = 0;
	while (Index < DynamicUiFieldStates.Num())
	{
		const TSharedPtr<FWcwDynamicUiFieldState>& First = DynamicUiFieldStates[Index];
		const bool bFirstCanPair =
			(First->Def.ControlType == EWcwIssueTrackerUiControlType::TextBox ||
			 First->Def.ControlType == EWcwIssueTrackerUiControlType::Dropdown);

		if (bFirstCanPair && (Index + 1) < DynamicUiFieldStates.Num())
		{
			const TSharedPtr<FWcwDynamicUiFieldState>& Second = DynamicUiFieldStates[Index + 1];
			const bool bSecondCanPair =
				(Second->Def.ControlType == EWcwIssueTrackerUiControlType::TextBox ||
				 Second->Def.ControlType == EWcwIssueTrackerUiControlType::Dropdown);

			if (bSecondCanPair)
			{
				const float BottomPad = (Index + 2 < DynamicUiFieldStates.Num()) ? 6.0f : 0.0f;
				DynamicHandlerFieldsContainer->AddSlot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, BottomPad)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						BuildSingleDynamicFieldWidget(First)
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.Padding(6.0f, 0.0f, 0.0f, 0.0f)
					[
						BuildSingleDynamicFieldWidget(Second)
					]
				];
				Index += 2;
				continue;
			}
		}

		const float BottomPad = (Index + 1 < DynamicUiFieldStates.Num()) ? 6.0f : 0.0f;
		DynamicHandlerFieldsContainer->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, BottomPad)
		[
			BuildSingleDynamicFieldWidget(First)
		];
		++Index;
	}
}

TSharedRef<SWidget> SWcwSnapJumpEditorPanel::BuildSingleDynamicFieldWidget(const TSharedPtr<FWcwDynamicUiFieldState>& State)
{
	if (!State.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	const FText Tooltip = State->Def.TooltipText.IsEmpty()
		? FText::FromString(State->Def.FieldKey)
		: FText::FromString(State->Def.TooltipText);

	if (State->Def.ControlType == EWcwIssueTrackerUiControlType::TextBox)
	{
		return SNew(SVerticalBox)
			.ToolTipText(Tooltip)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 3.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(State->Def.DisplayLabel))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SEditableTextBox)
				.Text(FText::FromString(State->CurrentValue))
				.HintText(FText::FromString(State->Def.bOmitWhenEmpty ? TEXT("(Optional - leave empty to omit)") : TEXT("Enter value...")))
				.OnTextChanged_Lambda([State](const FText& NewText)
				{
					State->CurrentValue = NewText.ToString();
					State->bModifiedByUser = true;
				})
			];
	}

	if (State->Def.ControlType == EWcwIssueTrackerUiControlType::Dropdown)
	{
		return SNew(SVerticalBox)
			.ToolTipText(Tooltip)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 3.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(State->Def.DisplayLabel))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SComboBox<TSharedPtr<FWcwIssueTrackerUiOption>>)
				.OptionsSource(&State->ComboOptions)
				.InitiallySelectedItem(State->SelectedComboOption)
				.OnGenerateWidget_Lambda([](TSharedPtr<FWcwIssueTrackerUiOption> Item)
				{
					return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? Item->DisplayLabel : TEXT("-")));
				})
				.OnSelectionChanged_Lambda([State](TSharedPtr<FWcwIssueTrackerUiOption> NewItem, ESelectInfo::Type SelectInfo)
				{
					State->SelectedComboOption = NewItem;
					State->CurrentValue = NewItem.IsValid() ? NewItem->Value : FString();
					if (SelectInfo != ESelectInfo::Direct)
					{
						State->bModifiedByUser = true;
					}
				})
				[
					SNew(STextBlock)
					.Text_Lambda([State]()
					{
						if (State->SelectedComboOption.IsValid())
						{
							return FText::FromString(State->SelectedComboOption->DisplayLabel);
						}
						return FText::FromString(State->CurrentValue.IsEmpty() ? TEXT("(Unselected)") : State->CurrentValue);
					})
				]
			];
	}

	if (State->Def.ControlType == EWcwIssueTrackerUiControlType::MultiSelect)
	{
		TSharedRef<SWrapBox> WrapBox = SNew(SWrapBox).UseAllottedSize(true);
		for (const TSharedPtr<FWcwIssueTrackerUiOption>& Opt : State->ComboOptions)
		{
			if (!Opt.IsValid() || Opt->Value.IsEmpty())
			{
				continue;
			}
			const FString OptVal = Opt->Value;
			const FString OptLabel = Opt->DisplayLabel;

			WrapBox->AddSlot()
			.Padding(0.0f, 2.0f, 10.0f, 2.0f)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([State, OptVal]()
				{
					return WcwRedminePanelInternal::IsTokenInPipeList(State->CurrentValue, OptVal)
						? ECheckBoxState::Checked
						: ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([State, OptVal](ECheckBoxState NewState)
				{
					State->CurrentValue = WcwRedminePanelInternal::ToggleTokenInPipeList(
						State->CurrentValue,
						OptVal,
						NewState == ECheckBoxState::Checked);
					State->bModifiedByUser = true;
				})
				[
					SNew(STextBlock)
					.Text(FText::FromString(OptLabel))
				]
			];
		}

		return SNew(SVerticalBox)
			.ToolTipText(Tooltip)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 3.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(State->Def.DisplayLabel))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				WrapBox
			];
	}

	// CheckBox (Boolean)
	const bool bUseTrueFalseLiteral =
		(State->Def.TargetType == EWcwIssueTrackerFieldTarget::StandardIssueField &&
		 State->Def.FieldKey.Equals(TEXT("is_private"), ESearchCase::IgnoreCase));

	return SNew(SBox)
		.ToolTipText(Tooltip)
		.Padding(0.0f, 2.0f)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([State]()
			{
				const FString V = State->CurrentValue.TrimStartAndEnd();
				const bool bOn = (V.Equals(TEXT("1")) || V.Equals(TEXT("true"), ESearchCase::IgnoreCase));
				return bOn ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([State, bUseTrueFalseLiteral](ECheckBoxState NewState)
			{
				const bool bOn = (NewState == ECheckBoxState::Checked);
				State->CurrentValue = bUseTrueFalseLiteral
					? (bOn ? TEXT("true") : TEXT("false"))
					: (bOn ? TEXT("1") : TEXT("0"));
				State->bModifiedByUser = true;
			})
			[
				SNew(STextBlock)
				.Text(FText::FromString(State->Def.DisplayLabel))
			]
		];
}

void SWcwSnapJumpEditorPanel::ApplyDynamicUiFieldsToMaps(
	TMap<FString, FString>& InOutExtraIssueFields,
	TMap<FString, FString>& InOutCustomFields,
	TSet<FString>* OutMultiValueKeys) const
{
	for (const TSharedPtr<FWcwDynamicUiFieldState>& State : DynamicUiFieldStates)
	{
		if (!State.IsValid())
		{
			continue;
		}

		const FString Key = State->Def.FieldKey.TrimStartAndEnd();
		if (Key.IsEmpty())
		{
			continue;
		}

		const FString Val = State->CurrentValue.TrimStartAndEnd();

		if (State->Def.TargetType == EWcwIssueTrackerFieldTarget::CustomField)
		{
			if (Val.IsEmpty() && State->Def.bOmitWhenEmpty)
			{
				InOutCustomFields.Remove(Key);
				continue;
			}
			InOutCustomFields.Add(Key, State->CurrentValue);
			if (OutMultiValueKeys &&
			    (State->Def.bSendAsMultipleArray || State->Def.ControlType == EWcwIssueTrackerUiControlType::MultiSelect))
			{
				OutMultiValueKeys->Add(Key);
			}
		}
		else
		{
			const FString TargetKey = (State->Def.TargetType == EWcwIssueTrackerFieldTarget::RedminePriority)
				? FString(TEXT("priority_id"))
				: Key;

			if (Val.IsEmpty() && State->Def.bOmitWhenEmpty)
			{
				InOutExtraIssueFields.Remove(TargetKey);
				continue;
			}
			InOutExtraIssueFields.Add(TargetKey, Val);
		}
	}
}

void SWcwSnapJumpEditorPanel::RebuildParentPresetOptions()
{
	ParentPresetOptions.Reset();
	ParentPresetOptions.Add(MakeShared<FWcwRedmineOptionItem>(0, TEXT("(No Parent)")));

	const UWcwSnapJumpRedmineSettings* ProjSettings = GetDefault<UWcwSnapJumpRedmineSettings>();
	if (ProjSettings)
	{
		for (const FWcwRedmineParentPreset& Preset : ProjSettings->ParentTicketPresets)
		{
			if (Preset.IssueId > 0)
			{
				const FString Display = Preset.Label.IsEmpty()
					? FString::Printf(TEXT("#%d"), Preset.IssueId)
					: FString::Printf(TEXT("%s (#%d)"), *Preset.Label, Preset.IssueId);
				ParentPresetOptions.Add(MakeShared<FWcwRedmineOptionItem>(Preset.IssueId, Display));
			}
		}
	}
	ParentPresetOptions.Add(MakeShared<FWcwRedmineOptionItem>(-1, TEXT("(Custom #ID)")));

	const UWcwSnapJumpRedmineUserSettings* UserSettings = GetDefault<UWcwSnapJumpRedmineUserSettings>();
	const int32 DesiredId = UserSettings ? UserSettings->LastUsedParentIssueId : 0;

	SelectedParentPresetOption = ParentPresetOptions[0];
	if (DesiredId > 0)
	{
		bool bMatchedPreset = false;
		for (const TSharedPtr<FWcwRedmineOptionItem>& Opt : ParentPresetOptions)
		{
			if (Opt.IsValid() && Opt->Id == DesiredId)
			{
				SelectedParentPresetOption = Opt;
				bMatchedPreset = true;
				break;
			}
		}
		if (!bMatchedPreset)
		{
			SelectedParentPresetOption = ParentPresetOptions.Last();
		}
	}

	if (ParentPresetComboBox.IsValid())
	{
		ParentPresetComboBox->RefreshOptions();
		ParentPresetComboBox->SetSelectedItem(SelectedParentPresetOption);
	}
}

void SWcwSnapJumpEditorPanel::ApplyHandlerPresetSelectionForCurrentSnap()
{
	if (!CurrentSelectedItem.IsValid())
	{
		return;
	}

	FWcwIssueTrackerPresetSelection Preset;
	UWcwSnapJumpLibrary::GatherIssueTrackerPresetSelection(CurrentSelectedItem->SnapData, Preset);

	if (!Preset.PreferredTracker.TrimStartAndEnd().IsEmpty())
	{
		if (TSharedPtr<FWcwRedmineOptionItem> Found = WcwRedminePanelInternal::FindOptionByIdOrName(TrackerOptions, Preset.PreferredTracker))
		{
			SelectedTrackerOption = Found;
			if (TrackerComboBox.IsValid())
			{
				TrackerComboBox->SetSelectedItem(Found);
			}
		}
	}

	if (!Preset.PreferredCategory.TrimStartAndEnd().IsEmpty())
	{
		if (TSharedPtr<FWcwRedmineOptionItem> Found = WcwRedminePanelInternal::FindOptionByIdOrName(CategoryOptions, Preset.PreferredCategory))
		{
			SelectedCategoryOption = Found;
			if (CategoryComboBox.IsValid())
			{
				CategoryComboBox->SetSelectedItem(Found);
			}
		}
	}

	if (!Preset.PreferredAssignee.TrimStartAndEnd().IsEmpty())
	{
		if (TSharedPtr<FWcwRedmineOptionItem> Found = WcwRedminePanelInternal::FindOptionByIdOrName(AssigneeOptions, Preset.PreferredAssignee))
		{
			SelectedAssigneeOption = Found;
			if (AssigneeComboBox.IsValid())
			{
				AssigneeComboBox->SetSelectedItem(Found);
			}
		}
	}

	if (Preset.PreferredParentIssueId > 0)
	{
		if (ParentIssueIdInputBox.IsValid())
		{
			ParentIssueIdInputBox->SetText(FText::FromString(FString::FromInt(Preset.PreferredParentIssueId)));
		}
		bool bMatchedPreset = false;
		for (const TSharedPtr<FWcwRedmineOptionItem>& Opt : ParentPresetOptions)
		{
			if (Opt.IsValid() && Opt->Id == Preset.PreferredParentIssueId)
			{
				SelectedParentPresetOption = Opt;
				bMatchedPreset = true;
				break;
			}
		}
		if (!bMatchedPreset && ParentPresetOptions.Num() > 0)
		{
			SelectedParentPresetOption = ParentPresetOptions.Last();
		}
		if (ParentPresetComboBox.IsValid())
		{
			ParentPresetComboBox->SetSelectedItem(SelectedParentPresetOption);
		}
		TriggerVerifyParentTicket(Preset.PreferredParentIssueId);
	}
}

void SWcwSnapJumpEditorPanel::SyncSubjectPrefixForSelection(bool bForceResetIfUnedited)
{
	if (!SubjectInputBox.IsValid() || !CurrentSelectedItem.IsValid())
	{
		return;
	}

	const FString NewPrefix = UWcwSnapJumpLibrary::FormatIssueTrackerSubject(CurrentSelectedItem->SnapData, TEXT(""));
	FString CurrentText = SubjectInputBox->GetText().ToString();

	if (!LastAutoSubjectPrefix.IsEmpty() && CurrentText.StartsWith(LastAutoSubjectPrefix))
	{
		CurrentText.RightChopInline(LastAutoSubjectPrefix.Len(), EAllowShrinking::No);
	}

	if (bPrefixSubjectWithTag)
	{
		SubjectInputBox->SetText(FText::FromString(NewPrefix + CurrentText));
		LastAutoSubjectPrefix = NewPrefix;
	}
	else
	{
		SubjectInputBox->SetText(FText::FromString(CurrentText));
		LastAutoSubjectPrefix.Reset();
	}
}

void SWcwSnapJumpEditorPanel::StartVerifyConnectionAndFetchMeta()
{
	const UWcwSnapJumpRedmineSettings* ProjSettings = GetDefault<UWcwSnapJumpRedmineSettings>();
	const UWcwSnapJumpRedmineUserSettings* UserSettings = GetDefault<UWcwSnapJumpRedmineUserSettings>();
	if (!ProjSettings || !UserSettings)
	{
		return;
	}

	const FString BaseUrl = ProjSettings->GetNormalizedBaseUrl();
	const FString ProjectId = ProjSettings->ProjectIdentifier.TrimStartAndEnd();
	const FString ApiKey = UserSettings->ApiAccessKey.TrimStartAndEnd();
	if (BaseUrl.IsEmpty() || ProjectId.IsEmpty() || ApiKey.IsEmpty())
	{
		bIsRedmineConnected = false;
		return;
	}

	bIsHttpBusy = true;
	TWeakPtr<SWcwSnapJumpEditorPanel> WeakSelf = StaticCastSharedRef<SWcwSnapJumpEditorPanel>(AsShared());

	FWcwRedmineClient::VerifyAndFetchProjectMetaAsync(
		BaseUrl,
		ProjectId,
		ApiKey,
		[WeakSelf](bool bSuccess, const FWcwRedmineProjectMeta& Meta, const FString& ErrorMsg)
		{
			TSharedPtr<SWcwSnapJumpEditorPanel> Self = WeakSelf.Pin();
			if (!Self.IsValid())
			{
				return;
			}

			Self->bIsHttpBusy = false;
			Self->bIsRedmineConnected = bSuccess;

			if (!bSuccess)
			{
				Self->CurrentRedmineUserId = 0;
				Self->ConnectedUserSummary.Reset();
				Self->bRedmineFeedbackIsError = true;
				Self->RedmineFeedbackMessage = ErrorMsg;
				return;
			}

			Self->CurrentRedmineUserId = Meta.CurrentUserId;
			Self->ConnectedUserSummary = Meta.CurrentUserDisplayName;
			Self->CachedPriorities = Meta.Priorities;
			Self->CachedDefaultPriorityId = Meta.DefaultPriorityId;
			Self->CachedIssueCustomFields = Meta.IssueCustomFields;
			Self->bShowApiKeyEditorInline = false;
			if (Self->bRedmineFeedbackIsError)
			{
				Self->RedmineFeedbackMessage.Reset();
			}

			const UWcwSnapJumpRedmineSettings* PSettings = GetDefault<UWcwSnapJumpRedmineSettings>();
			const UWcwSnapJumpRedmineUserSettings* USettings = GetDefault<UWcwSnapJumpRedmineUserSettings>();

			// 1. Trackers
			Self->TrackerOptions.Reset();
			for (const FWcwRedmineOptionItem& T : Meta.Trackers)
			{
				Self->TrackerOptions.Add(MakeShared<FWcwRedmineOptionItem>(T.Id, FString::Printf(TEXT("%s (ID:%d)"), *T.Name, T.Id)));
			}

			Self->SelectedTrackerOption.Reset();
			if (Self->TrackerOptions.Num() > 0)
			{
				const int32 SavedTrackerId = USettings ? USettings->LastSelectedTrackerId : 0;
				const FString PreferredName = PSettings ? PSettings->DefaultTrackerName.TrimStartAndEnd() : FString(TEXT("\u30D0\u30B0"));

				for (int32 i = 0; i < Meta.Trackers.Num(); ++i)
				{
					if (SavedTrackerId > 0 && Meta.Trackers[i].Id == SavedTrackerId)
					{
						Self->SelectedTrackerOption = Self->TrackerOptions[i];
						break;
					}
				}
				if (!Self->SelectedTrackerOption.IsValid())
				{
					for (int32 i = 0; i < Meta.Trackers.Num(); ++i)
					{
						if (Meta.Trackers[i].Name.Equals(PreferredName, ESearchCase::IgnoreCase) ||
						    Meta.Trackers[i].Name.Equals(TEXT("Bug"), ESearchCase::IgnoreCase))
						{
							Self->SelectedTrackerOption = Self->TrackerOptions[i];
							break;
						}
					}
				}
				if (!Self->SelectedTrackerOption.IsValid())
				{
					Self->SelectedTrackerOption = Self->TrackerOptions[0];
				}
			}
			if (Self->TrackerComboBox.IsValid())
			{
				Self->TrackerComboBox->RefreshOptions();
				Self->TrackerComboBox->SetSelectedItem(Self->SelectedTrackerOption);
			}

			// 2. Categories
			Self->CategoryOptions.Reset();
			Self->CategoryOptions.Add(MakeShared<FWcwRedmineOptionItem>(0, TEXT("(None)")));
			for (const FWcwRedmineOptionItem& C : Meta.Categories)
			{
				Self->CategoryOptions.Add(MakeShared<FWcwRedmineOptionItem>(C.Id, C.Name));
			}

			Self->SelectedCategoryOption = Self->CategoryOptions[0];
			const int32 SavedCatId = USettings ? USettings->LastSelectedCategoryId : 0;
			if (SavedCatId > 0)
			{
				for (const TSharedPtr<FWcwRedmineOptionItem>& Opt : Self->CategoryOptions)
				{
					if (Opt.IsValid() && Opt->Id == SavedCatId)
					{
						Self->SelectedCategoryOption = Opt;
						break;
					}
				}
			}
			if (Self->CategoryComboBox.IsValid())
			{
				Self->CategoryComboBox->RefreshOptions();
				Self->CategoryComboBox->SetSelectedItem(Self->SelectedCategoryOption);
			}

			// 3. Assignees
			Self->AssigneeOptions.Reset();
			Self->AssigneeOptions.Add(MakeShared<FWcwRedmineOptionItem>(0, TEXT("(Unassigned)")));
			for (const FWcwRedmineOptionItem& A : Meta.Assignees)
			{
				Self->AssigneeOptions.Add(MakeShared<FWcwRedmineOptionItem>(A.Id, A.Name));
			}

			Self->SelectedAssigneeOption = Self->AssigneeOptions[0];
			const int32 SavedAssigneeId = USettings ? USettings->LastSelectedAssigneeId : 0;
			if (SavedAssigneeId > 0)
			{
				for (const TSharedPtr<FWcwRedmineOptionItem>& Opt : Self->AssigneeOptions)
				{
					if (Opt.IsValid() && Opt->Id == SavedAssigneeId)
					{
						Self->SelectedAssigneeOption = Opt;
						break;
					}
				}
			}
			if (Self->AssigneeComboBox.IsValid())
			{
				Self->AssigneeComboBox->RefreshOptions();
				Self->AssigneeComboBox->SetSelectedItem(Self->SelectedAssigneeOption);
			}

			// 4. Refresh Title-defined UI controls with live Priorities & Custom Field PossibleValues
			Self->RebuildDynamicHandlerUiFields(true);

			// 5. Apply Title-side preset overrides if a snapshot is already selected, otherwise verify saved parent
			if (USettings && USettings->LastUsedParentIssueId > 0)
			{
				Self->TriggerVerifyParentTicket(USettings->LastUsedParentIssueId);
			}
			Self->ApplyHandlerPresetSelectionForCurrentSnap();
		});
}

void SWcwSnapJumpEditorPanel::TriggerVerifyParentTicket(int32 IssueIdToVerify)
{
	const UWcwSnapJumpRedmineSettings* ProjSettings = GetDefault<UWcwSnapJumpRedmineSettings>();
	const UWcwSnapJumpRedmineUserSettings* UserSettings = GetDefault<UWcwSnapJumpRedmineUserSettings>();
	if (!ProjSettings || !UserSettings || IssueIdToVerify <= 0)
	{
		ParentVerifyStatusText.Reset();
		return;
	}

	const FString BaseUrl = ProjSettings->GetNormalizedBaseUrl();
	const FString ApiKey = UserSettings->ApiAccessKey.TrimStartAndEnd();
	if (BaseUrl.IsEmpty() || ApiKey.IsEmpty())
	{
		return;
	}

	TWeakPtr<SWcwSnapJumpEditorPanel> WeakSelf = StaticCastSharedRef<SWcwSnapJumpEditorPanel>(AsShared());
	FWcwRedmineClient::VerifyParentIssueAsync(
		BaseUrl,
		ApiKey,
		IssueIdToVerify,
		[WeakSelf](bool bSuccess, int32 CheckedId, const FString& Summary, const FString& ErrMsg)
		{
			if (TSharedPtr<SWcwSnapJumpEditorPanel> Self = WeakSelf.Pin())
			{
				Self->bParentVerifyIsError = !bSuccess;
				Self->ParentVerifyStatusText = bSuccess
					? FString::Printf(TEXT("Parent Verified -> %s"), *Summary)
					: ErrMsg;
			}
		});
}

FReply SWcwSnapJumpEditorPanel::OnSaveApiKeyAndConnectClicked()
{
	if (ApiKeyInputBox.IsValid())
	{
		if (UWcwSnapJumpRedmineUserSettings* UserSettings = GetMutableDefault<UWcwSnapJumpRedmineUserSettings>())
		{
			UserSettings->ApiAccessKey = ApiKeyInputBox->GetText().ToString().TrimStartAndEnd();
			UserSettings->SaveUserConfig();
		}
	}
	StartVerifyConnectionAndFetchMeta();
	return FReply::Handled();
}

FReply SWcwSnapJumpEditorPanel::OnAssignToMeClicked()
{
	if (CurrentRedmineUserId <= 0)
	{
		return FReply::Handled();
	}

	for (const TSharedPtr<FWcwRedmineOptionItem>& Opt : AssigneeOptions)
	{
		if (Opt.IsValid() && Opt->Id == CurrentRedmineUserId)
		{
			SelectedAssigneeOption = Opt;
			if (AssigneeComboBox.IsValid())
			{
				AssigneeComboBox->SetSelectedItem(Opt);
			}
			if (UWcwSnapJumpRedmineUserSettings* U = GetMutableDefault<UWcwSnapJumpRedmineUserSettings>())
			{
				U->LastSelectedAssigneeId = CurrentRedmineUserId;
				U->SaveUserConfig();
			}
			break;
		}
	}

	return FReply::Handled();
}

FReply SWcwSnapJumpEditorPanel::OnOpenRedmineProjectSettingsClicked()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		const UWcwSnapJumpRedmineSettings* RedmineSettings = GetDefault<UWcwSnapJumpRedmineSettings>();
		if (RedmineSettings)
		{
			SettingsModule->ShowViewer(
				RedmineSettings->GetContainerName(),
				RedmineSettings->GetCategoryName(),
				RedmineSettings->GetSectionName());
		}
	}
	return FReply::Handled();
}

FString SWcwSnapJumpEditorPanel::GetSelectedAttachmentSummaryText() const
{
	if (!CurrentSelectedItem.IsValid())
	{
		return TEXT("Auto-Attach: (Select a snapshot on the left)");
	}

	FString PngPath = CurrentSelectedItem->SnapData.SourceFilePath;
	if (FPaths::IsRelative(PngPath))
	{
		PngPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), PngPath);
	}
	const FString JsonPath = FPaths::ChangeExtension(PngPath, TEXT("json"));

	const int64 PngSize = IFileManager::Get().FileSize(*PngPath);
	const int64 JsonSize = IFileManager::Get().FileSize(*JsonPath);

	const double PngKB = PngSize > 0 ? static_cast<double>(PngSize) / 1024.0 : 0.0;
	const double JsonKB = JsonSize > 0 ? static_cast<double>(JsonSize) / 1024.0 : 2.5;
	const double TotalMB = (PngKB + JsonKB) / 1024.0;

	FString SummaryLine = FString::Printf(
		TEXT("Auto-Attach: %s (%.0f KB) + %s (%.1f KB)  [Total: %.2f MB]"),
		*FPaths::GetCleanFilename(PngPath),
		PngKB,
		*FPaths::GetCleanFilename(JsonPath),
		JsonKB,
		TotalMB);

	TMap<FString, FString> PreviewCustomFields;
	TMap<FString, FString> PreviewExtraFields;
	if (const UWcwSnapJumpRedmineSettings* ProjSettings = GetDefault<UWcwSnapJumpRedmineSettings>())
	{
		PreviewCustomFields = ProjSettings->DefaultCustomFields;
		PreviewExtraFields = ProjSettings->DefaultExtraIssueFields;
	}
	UWcwSnapJumpLibrary::GatherIssueTrackerCustomFields(CurrentSelectedItem->SnapData, PreviewCustomFields);
	UWcwSnapJumpLibrary::GatherIssueTrackerIssueFields(CurrentSelectedItem->SnapData, PreviewExtraFields);
	ApplyDynamicUiFieldsToMaps(PreviewExtraFields, PreviewCustomFields, nullptr);

	if (PreviewCustomFields.Num() > 0)
	{
		TArray<FString> CfPairs;
		for (const TPair<FString, FString>& Pair : PreviewCustomFields)
		{
			if (!Pair.Key.TrimStartAndEnd().IsEmpty())
			{
				CfPairs.Add(FString::Printf(TEXT("%s=\"%s\""), *Pair.Key.TrimStartAndEnd(), *Pair.Value));
			}
		}
		if (CfPairs.Num() > 0)
		{
			SummaryLine += LINE_TERMINATOR;
			SummaryLine += FString::Printf(TEXT("Custom Fields (%d): %s"), CfPairs.Num(), *FString::Join(CfPairs, TEXT(", ")));
		}
	}

	if (PreviewExtraFields.Num() > 0)
	{
		TArray<FString> ExtraPairs;
		for (const TPair<FString, FString>& Pair : PreviewExtraFields)
		{
			const FString K = Pair.Key.TrimStartAndEnd();
			if (K.IsEmpty())
			{
				continue;
			}
			FString DisplayVal = Pair.Value;
			if (K.Equals(TEXT("priority_id"), ESearchCase::IgnoreCase) && Pair.Value.TrimStartAndEnd().IsNumeric())
			{
				const int32 PrioId = FCString::Atoi(*Pair.Value.TrimStartAndEnd());
				for (const FWcwRedmineOptionItem& P : CachedPriorities)
				{
					if (P.Id == PrioId)
					{
						DisplayVal = FString::Printf(TEXT("%d (%s)"), PrioId, *P.Name);
						break;
					}
				}
			}
			ExtraPairs.Add(FString::Printf(TEXT("%s=%s"), *K, *DisplayVal));
		}
		if (ExtraPairs.Num() > 0)
		{
			SummaryLine += LINE_TERMINATOR;
			SummaryLine += FString::Printf(TEXT("Issue Overrides (%d): %s"), ExtraPairs.Num(), *FString::Join(ExtraPairs, TEXT(", ")));
		}
	}

	return SummaryLine;
}

FReply SWcwSnapJumpEditorPanel::OnPostIssueToRedmineClicked()
{
	if (!CurrentSelectedItem.IsValid() || bIsHttpBusy)
	{
		return FReply::Handled();
	}

	const UWcwSnapJumpRedmineSettings* ProjSettings = GetDefault<UWcwSnapJumpRedmineSettings>();
	const UWcwSnapJumpRedmineUserSettings* UserSettings = GetDefault<UWcwSnapJumpRedmineUserSettings>();
	if (!ProjSettings || !UserSettings)
	{
		return FReply::Handled();
	}

	FWcwRedmineTicketRequest TicketReq;
	TicketReq.BaseUrl = ProjSettings->GetNormalizedBaseUrl();
	TicketReq.ProjectIdentifier = ProjSettings->ProjectIdentifier.TrimStartAndEnd();
	TicketReq.ApiKey = UserSettings->ApiAccessKey.TrimStartAndEnd();
	TicketReq.TrackerId = SelectedTrackerOption.IsValid() ? SelectedTrackerOption->Id : 0;
	TicketReq.CategoryId = SelectedCategoryOption.IsValid() ? SelectedCategoryOption->Id : 0;
	TicketReq.AssignedToId = SelectedAssigneeOption.IsValid() ? SelectedAssigneeOption->Id : 0;

	if (ParentIssueIdInputBox.IsValid())
	{
		FString ParentStr = ParentIssueIdInputBox->GetText().ToString().TrimStartAndEnd();
		ParentStr.RemoveFromStart(TEXT("#"));
		TicketReq.ParentIssueId = ParentStr.IsEmpty() ? 0 : FCString::Atoi(*ParentStr);
	}

	TicketReq.Subject = SubjectInputBox.IsValid() ? SubjectInputBox->GetText().ToString().TrimStartAndEnd() : FString();

	FString BodyText = DescriptionInputBox.IsValid() ? DescriptionInputBox->GetText().ToString() : FString();
	if (bAppendTelemetryToDescription)
	{
		const FString TelemetryBlock = UWcwSnapJumpLibrary::FormatIssueTrackerTelemetryBlock(CurrentSelectedItem->SnapData);
		if (!TelemetryBlock.IsEmpty())
		{
			if (!BodyText.IsEmpty())
			{
				BodyText += LINE_TERMINATOR;
				BodyText += LINE_TERMINATOR;
			}
			BodyText += TelemetryBlock;
		}
	}
	TicketReq.Description = BodyText;

	TicketReq.ExtraIssueFields = ProjSettings->DefaultExtraIssueFields;
	UWcwSnapJumpLibrary::GatherIssueTrackerIssueFields(CurrentSelectedItem->SnapData, TicketReq.ExtraIssueFields);

	TicketReq.CustomFields = ProjSettings->DefaultCustomFields;
	UWcwSnapJumpLibrary::GatherIssueTrackerCustomFields(CurrentSelectedItem->SnapData, TicketReq.CustomFields);

	// Interactive UI controls take top priority over static defaults & background hooks
	ApplyDynamicUiFieldsToMaps(TicketReq.ExtraIssueFields, TicketReq.CustomFields, &TicketReq.MultiValueCustomFieldKeys);

	TicketReq.KnownCustomFields = CachedIssueCustomFields;
	TicketReq.KnownPriorities = CachedPriorities;

	TicketReq.PngFilePath = CurrentSelectedItem->SnapData.SourceFilePath;
	TicketReq.SnapData = CurrentSelectedItem->SnapData;

	if (TicketReq.Subject.IsEmpty())
	{
		bRedmineFeedbackIsError = true;
		RedmineFeedbackMessage = TEXT("Please enter a Subject before posting.");
		return FReply::Handled();
	}

	bIsHttpBusy = true;
	bRedmineFeedbackIsError = false;
	LastCreatedIssueUrl.Reset();
	LastCreatedIssueId = 0;
	RedmineFeedbackMessage = TEXT("Uploading snapshot PNG + JSON and creating Redmine ticket...");

	TSharedPtr<FWcwSnapJumpEditorRowItem> TargetRowItem = CurrentSelectedItem;
	TWeakPtr<SWcwSnapJumpEditorPanel> WeakSelf = StaticCastSharedRef<SWcwSnapJumpEditorPanel>(AsShared());

	FWcwRedmineClient::PostIssueWithAttachmentsAsync(
		TicketReq,
		[WeakSelf, TargetRowItem](bool bSuccess, int32 CreatedIssueId, const FString& CreatedUrl, const FString& ErrorMsg)
		{
			TSharedPtr<SWcwSnapJumpEditorPanel> Self = WeakSelf.Pin();
			if (!Self.IsValid())
			{
				return;
			}

			Self->bIsHttpBusy = false;
			if (!bSuccess)
			{
				Self->bRedmineFeedbackIsError = true;
				Self->RedmineFeedbackMessage = FString::Printf(TEXT("Failed to create Redmine ticket:\n%s"), *ErrorMsg);
				return;
			}

			if (TargetRowItem.IsValid() && CreatedIssueId > 0)
			{
				FWcwRedmineClient::WritePostedIssueToSidecar(
					TargetRowItem->SnapData.SourceFilePath,
					TargetRowItem->SnapData,
					CreatedIssueId,
					CreatedUrl);

				TargetRowItem->PostedIssueId = CreatedIssueId;
				TargetRowItem->PostedIssueUrl = CreatedUrl;
				TargetRowItem->PostedAt = FDateTime::Now().ToIso8601();
			}

			if (Self->ListViewWidget.IsValid())
			{
				Self->ListViewWidget->RequestListRefresh();
			}

			Self->bRedmineFeedbackIsError = false;
			Self->LastCreatedIssueId = CreatedIssueId;
			Self->LastCreatedIssueUrl = CreatedUrl;
			Self->RedmineFeedbackMessage = FString::Printf(
				TEXT("Successfully created Redmine ticket #%d with .png and .json attached!"),
				CreatedIssueId);
		});

	return FReply::Handled();
}

void SWcwSnapJumpEditorPanel::OpenPostedIssueInBrowser(const TSharedPtr<FWcwSnapJumpEditorRowItem>& Item) const
{
	if (!Item.IsValid() || Item->PostedIssueId <= 0)
	{
		return;
	}

	FString TargetUrl = Item->PostedIssueUrl;
	if (TargetUrl.IsEmpty())
	{
		if (const UWcwSnapJumpRedmineSettings* S = GetDefault<UWcwSnapJumpRedmineSettings>())
		{
			const FString Base = S->GetNormalizedBaseUrl();
			if (!Base.IsEmpty())
			{
				TargetUrl = FString::Printf(TEXT("%s/issues/%d"), *Base, Item->PostedIssueId);
			}
		}
	}

	if (!TargetUrl.IsEmpty())
	{
		FPlatformProcess::LaunchURL(*TargetUrl, nullptr, nullptr);
	}
}

TSharedRef<ITableRow> SWcwSnapJumpEditorPanel::OnGenerateRowForList(TSharedPtr<FWcwSnapJumpEditorRowItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SWcwSnapJumpListRow, OwnerTable)
		.Item(Item)
		.OwnerPanel(this);
}

void SWcwSnapJumpEditorPanel::OnSelectionChanged(TSharedPtr<FWcwSnapJumpEditorRowItem> SelectedItem, ESelectInfo::Type SelectInfo)
{
	CurrentSelectedItem = SelectedItem;
	if (CurrentSelectedItem.IsValid())
	{
		if (SelectedPreviewImage.IsValid())
		{
			SelectedPreviewImage->SetImage(
				CurrentSelectedItem->ThumbnailBrush.IsValid()
				? CurrentSelectedItem->ThumbnailBrush.Get()
				: FCoreStyle::Get().GetDefaultBrush()
			);
		}

		if (DetailReportText.IsValid())
		{
			const FString FullReport = UWcwSnapJumpLibrary::FormatDetailedTelemetryReport(CurrentSelectedItem->SnapData);
			DetailReportText->SetText(FText::FromString(FullReport));
		}

		RebuildDynamicHandlerUiFields(true);
		ApplyHandlerPresetSelectionForCurrentSnap();
		SyncSubjectPrefixForSelection(false);
	}
}

void SWcwSnapJumpEditorPanel::OnListMouseButtonDoubleClick(TSharedPtr<FWcwSnapJumpEditorRowItem> ClickedItem)
{
	if (ClickedItem.IsValid())
	{
		ExecuteJumpForItem(ClickedItem);
	}
}

void SWcwSnapJumpEditorPanel::ExecuteJumpForItem(const TSharedPtr<FWcwSnapJumpEditorRowItem>& Item)
{
	if (!Item.IsValid())
	{
		return;
	}

	CurrentSelectedItem = Item;
	if (ListViewWidget.IsValid())
	{
		ListViewWidget->SetSelection(Item);
	}

	if (GEditor && GEditor->PlayWorld)
	{
		UWcwSnapJumpLibrary::JumpToSnapData(GEditor->PlayWorld, Item->SnapData, true, true);
		return;
	}

	if (GEditor)
	{
		const FWcwSnapJumpData& Snap = Item->SnapData;
		const UWcwSnapJumpSettings* Settings = GetDefault<UWcwSnapJumpSettings>();
		if (Settings && Settings->bAutoRestoreStreamingLevels && Settings->bAutoShowStreamingLevelsInEditorJump)
		{
			if (UWorld* EditorWorld = GEditor->GetEditorWorldContext().World())
			{
				TArray<FString> TargetPackages = Snap.LoadedStreamingLevels;
				if (!Snap.LevelPackageName.IsEmpty())
				{
					TargetPackages.AddUnique(Snap.LevelPackageName);
				}
				else if (!Snap.LevelName.IsEmpty())
				{
					TargetPackages.AddUnique(Snap.LevelName);
				}

				TArray<ULevel*> LevelsToShow;
				TArray<bool> VisibilityFlags;
				for (ULevelStreaming* StreamingLevel : EditorWorld->GetStreamingLevels())
				{
					ULevel* LoadedSubLevel = StreamingLevel ? StreamingLevel->GetLoadedLevel() : nullptr;
					if (!LoadedSubLevel || LoadedSubLevel->bIsVisible)
					{
						continue;
					}
					for (const FString& TargetPkg : TargetPackages)
					{
						if (UWcwSnapJumpLibrary::DoesStreamingLevelMatchPackage(StreamingLevel, TargetPkg))
						{
							LevelsToShow.Add(LoadedSubLevel);
							VisibilityFlags.Add(true);
							break;
						}
					}
				}

				if (LevelsToShow.Num() > 0)
				{
					UEditorLevelUtils::SetLevelsVisibility(
						LevelsToShow,
						VisibilityFlags,
						true,
						ELevelVisibilityDirtyMode::DontModify);
				}
			}
		}

		for (FLevelEditorViewportClient* LevelVC : GEditor->GetLevelViewportClients())
		{
			if (LevelVC && LevelVC->IsPerspective())
			{
				const FVector TargetPos = !Snap.CameraLocation.IsNearlyZero()
					? Snap.CameraLocation
					: Snap.CharacterLocation;
				const FRotator TargetRot = !Snap.CameraRotation.IsNearlyZero()
					? Snap.CameraRotation
					: Snap.CharacterRotation;

				LevelVC->SetViewLocation(TargetPos);
				LevelVC->SetViewRotation(TargetRot);
				LevelVC->Invalidate();
				break;
			}
		}
	}
}

FReply SWcwSnapJumpEditorPanel::OnRefreshClicked()
{
	SnapshotItems.Reset();
	const TArray<FWcwSnapJumpData> FoundSnaps = UWcwSnapJumpLibrary::ScanSnapDirectory();
	for (const FWcwSnapJumpData& Snap : FoundSnaps)
	{
		TSharedPtr<FWcwSnapJumpEditorRowItem> RowItem = MakeShared<FWcwSnapJumpEditorRowItem>();
		RowItem->SnapData = Snap;

		FWcwRedmineClient::ReadPostedIssueFromSidecar(
			Snap.SourceFilePath,
			RowItem->PostedIssueId,
			RowItem->PostedIssueUrl,
			RowItem->PostedAt);

		bool bDummyValid = false;
		FWcwSnapJumpData DummySnap;
		if (UTexture2D* LoadedTex = UWcwSnapJumpLibrary::LoadSnapThumbnailAndData(Snap.SourceFilePath, DummySnap, bDummyValid))
		{
			RowItem->ThumbnailTexture = TStrongObjectPtr<UTexture2D>(LoadedTex);
			RowItem->ThumbnailBrush = MakeShared<FSlateImageBrush>(
				LoadedTex,
				FVector2D(static_cast<float>(LoadedTex->GetSizeX()), static_cast<float>(LoadedTex->GetSizeY())));
		}

		SnapshotItems.Add(RowItem);
	}

	if (ListViewWidget.IsValid())
	{
		ListViewWidget->RequestListRefresh();
		if (SnapshotItems.Num() > 0)
		{
			ListViewWidget->SetSelection(SnapshotItems[0]);
		}
		else
		{
			RebuildDynamicHandlerUiFields(true);
		}
	}
	return FReply::Handled();
}

FReply SWcwSnapJumpEditorPanel::OnMoveEditorCameraClicked()
{
	if (CurrentSelectedItem.IsValid())
	{
		ExecuteJumpForItem(CurrentSelectedItem);
	}
	return FReply::Handled();
}

UTexture2D* SWcwSnapJumpEditorPanel::ImportPngAsTextureAsset(const FString& PngFilePath, const FString& TargetPackageFolder, const FString& AssetName)
{
	TArray<uint8> FileBytes;
	if (!FFileHelper::LoadFileToArray(FileBytes, *PngFilePath))
	{
		return nullptr;
	}

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
	if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(FileBytes.GetData(), FileBytes.Num()))
	{
		return nullptr;
	}

	TArray64<uint8> UncompressedBGRA;
	if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, UncompressedBGRA))
	{
		return nullptr;
	}

	const int32 Width = ImageWrapper->GetWidth();
	const int32 Height = ImageWrapper->GetHeight();
	if (Width <= 0 || Height <= 0)
	{
		return nullptr;
	}

	uint8* RawBytes = UncompressedBGRA.GetData();
	const int64 TotalBytes = UncompressedBGRA.Num();
	for (int64 Idx = 3; Idx < TotalBytes; Idx += 4)
	{
		RawBytes[Idx] = 255;
	}

	const FString SafeAssetName = ObjectTools::SanitizeObjectName(AssetName);
	const FString PackageName = TargetPackageFolder / SafeAssetName;

	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		return nullptr;
	}
	Package->FullyLoad();

	UTexture2D* NewTexture = FindObject<UTexture2D>(Package, *SafeAssetName);
	if (!NewTexture)
	{
		NewTexture = NewObject<UTexture2D>(Package, *SafeAssetName, RF_Public | RF_Standalone | RF_Transactional);
	}

	NewTexture->Source.Init(Width, Height, 1, 1, ETextureSourceFormat::TSF_BGRA8, UncompressedBGRA.GetData());
	NewTexture->SRGB = true;
	NewTexture->LODGroup = TEXTUREGROUP_UI;
	NewTexture->MipGenSettings = TMGS_NoMipmaps;
	NewTexture->CompressionSettings = TC_Default;
	NewTexture->UpdateResource();
	NewTexture->PostEditChange();

	FAssetRegistryModule::AssetCreated(NewTexture);
	Package->MarkPackageDirty();

	const FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(PackageFileName), true);
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::SavePackage(Package, NewTexture, *PackageFileName, SaveArgs);

	return NewTexture;
}

UDataTable* SWcwSnapJumpEditorPanel::GetOrCreateTargetDataTable()
{
	UWcwSnapJumpSettings* MutableSettings = GetMutableDefault<UWcwSnapJumpSettings>();
	UDataTable* DataTable = nullptr;

	if (MutableSettings && MutableSettings->DefaultSnapJumpDataTable.IsValid())
	{
		DataTable = Cast<UDataTable>(MutableSettings->DefaultSnapJumpDataTable.TryLoad());
	}

	if (!DataTable)
	{
		const FString DefaultTablePackagePath = TEXT("/Game/WcwSnapJump/DT_WcwSnapJump");
		const FString DefaultTableAssetName = TEXT("DT_WcwSnapJump");

		UPackage* Package = CreatePackage(*DefaultTablePackagePath);
		if (!Package)
		{
			return nullptr;
		}
		Package->FullyLoad();

		DataTable = FindObject<UDataTable>(Package, *DefaultTableAssetName);
		if (!DataTable)
		{
			UDataTableFactory* Factory = NewObject<UDataTableFactory>();
			Factory->Struct = FWcwSnapJumpTableRow::StaticStruct();
			DataTable = Cast<UDataTable>(Factory->FactoryCreateNew(
				UDataTable::StaticClass(),
				Package,
				*DefaultTableAssetName,
				RF_Public | RF_Standalone | RF_Transactional,
				nullptr,
				GWarn));

			if (DataTable)
			{
				FAssetRegistryModule::AssetCreated(DataTable);
				Package->MarkPackageDirty();
			}
		}
	}

	if (DataTable && MutableSettings)
	{
		MutableSettings->DefaultSnapJumpDataTable = FSoftObjectPath(DataTable);
		MutableSettings->SaveConfig();
		MutableSettings->TryUpdateDefaultConfigFile();
	}

	return DataTable;
}

FReply SWcwSnapJumpEditorPanel::OnSyncToDefaultDataTableClicked()
{
	UDataTable* DataTable = GetOrCreateTargetDataTable();
	if (!DataTable || DataTable->GetRowStruct() != FWcwSnapJumpTableRow::StaticStruct())
	{
		UE_LOG(LogWcwSnapJump, Error, TEXT("Target DataTable is invalid or does not use FWcwSnapJumpTableRow."));
		return FReply::Handled();
	}

	const UWcwSnapJumpSettings* Settings = GetDefault<UWcwSnapJumpSettings>();
	const FString SubfolderName = (Settings && !Settings->ImportedTextureSubfolderName.IsEmpty())
		? Settings->ImportedTextureSubfolderName
		: TEXT("CustomScreenShot");

	const FString TablePackageName = DataTable->GetOutermost()->GetName();
	const FString TableFolder = FPackageName::GetLongPackagePath(TablePackageName);
	const FString CustomScreenShotFolder = TableFolder / SubfolderName;

	if (UProjectPackagingSettings* PackSettings = GetMutableDefault<UProjectPackagingSettings>())
	{
		const bool bAlreadyAdded = PackSettings->DirectoriesToAlwaysCook.ContainsByPredicate(
			[&TableFolder](const FDirectoryPath& Dir)
			{
				return Dir.Path.Equals(TableFolder, ESearchCase::IgnoreCase);
			});

		if (!bAlreadyAdded)
		{
			FDirectoryPath NewCookDir;
			NewCookDir.Path = TableFolder;
			PackSettings->DirectoriesToAlwaysCook.Add(NewCookDir);
			PackSettings->SaveConfig();
			PackSettings->TryUpdateDefaultConfigFile();
			UE_LOG(LogWcwSnapJump, Log, TEXT("Added '%s' to ProjectPackagingSettings::DirectoriesToAlwaysCook in DefaultGame.ini."), *TableFolder);
		}
	}

	DataTable->Modify();

	int32 ImportedCount = 0;
	for (const TSharedPtr<FWcwSnapJumpEditorRowItem>& Item : SnapshotItems)
	{
		if (!Item.IsValid())
		{
			continue;
		}

		const FString BaseName = FPaths::GetBaseFilename(Item->SnapData.SourceFilePath);
		const FString TextureAssetName = FString::Printf(TEXT("T_%s"), *BaseName);

		UTexture2D* ImportedTexture = ImportPngAsTextureAsset(
			Item->SnapData.SourceFilePath,
			CustomScreenShotFolder,
			TextureAssetName);

		FWcwSnapJumpTableRow NewRow;
		NewRow.DisplayTitle = FText::FromString(FString::Printf(TEXT("[%s] %s"), *Item->SnapData.LevelName, *Item->SnapData.CaptureTag));
		NewRow.ScreenshotFilePath = Item->SnapData.SourceFilePath;
		if (ImportedTexture)
		{
			NewRow.PrebakedThumbnail = TSoftObjectPtr<UTexture2D>(ImportedTexture);
		}
		NewRow.CachedSnapData = Item->SnapData;

		DataTable->AddRow(FName(*BaseName), NewRow);
		++ImportedCount;
	}

	DataTable->MarkPackageDirty();

	if (UPackage* DTPackage = DataTable->GetOutermost())
	{
		const FString DTFileName = FPackageName::LongPackageNameToFilename(TablePackageName, FPackageName::GetAssetPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(DTFileName), true);
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		UPackage::SavePackage(DTPackage, DataTable, *DTFileName, SaveArgs);
	}

	UE_LOG(LogWcwSnapJump, Log, TEXT("Imported & saved %d snapshot textures into '%s' and saved DataTable '%s' to disk."),
		ImportedCount, *CustomScreenShotFolder, *TablePackageName);

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
