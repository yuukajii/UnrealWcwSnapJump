// Copyright (c) 2026 WinterCrownWORKS. All Rights Reserved.
// Licensed under the MIT License.

#include "WcwSnapJumpBrowserWidget.h"
#include "WcwSnapJumpModule.h"
#include "WcwSnapJumpLibrary.h"
#include "WcwSnapJumpSettings.h"
#include "WcwSnapJumpSubsystem.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Console.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/GameInstance.h"
#include "InputCoreTypes.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Types/SlateEnums.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Styling/CoreStyle.h"
#include "Blueprint/WidgetTree.h"

UWcwSnapJumpBrowserWidget::UWcwSnapJumpBrowserWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

TSharedRef<SWidget> UWcwSnapJumpBrowserWidget::RebuildWidget()
{
	if (WidgetTree && WidgetTree->RootWidget)
	{
		return Super::RebuildWidget();
	}

	const float Scale = UWcwSnapJumpSubsystem::GetFontScale();
	const FSlateFontInfo HeaderFont = FCoreStyle::GetDefaultFontStyle("Bold", FMath::RoundToInt(11.0f * Scale));
	const FSlateFontInfo BodyFont = FCoreStyle::GetDefaultFontStyle("Regular", FMath::RoundToInt(9.5f * Scale));

	return SNew(SHorizontalBox)
		.Visibility(EVisibility::SelfHitTestInvisible)
		// === Left Half of Screen (50% width): Menu Panel ===
		+ SHorizontalBox::Slot()
		.FillWidth(0.50f)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.Padding(16.0f)
		[
			SNew(SBorder)
			.Visibility(EVisibility::Visible)
			.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			.ColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.96f))
			.Padding(14.0f)
			[
				SNew(SVerticalBox)
				// 1. Header Bar (Title + Refresh + Close)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 8.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SAssignNew(HeaderTitleText, STextBlock)
						.Font(HeaderFont)
						.AutoWrapText(true)
						.Text(FText::FromString(TEXT("Wcw SnapJump  [D-Pad/L-Stick: Select | R-Stick: Scroll | Accept(A): Jump | Back(B): Close]")))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(4.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.IsFocusable(false)
						.Text(FText::FromString(TEXT("Refresh")))
						.OnClicked_Lambda([this]()
						{
							RefreshSnapshotList();
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(4.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.IsFocusable(false)
						.Text(FText::FromString(TEXT("Close")))
						.OnClicked_Lambda([this]()
						{
							CloseBrowserMenu();
							return FReply::Handled();
						})
					]
				]
				// 2. Top Tier (55% Height): Scrollable Thumbnail + Summary List
				+ SVerticalBox::Slot()
				.FillHeight(0.55f)
				.Padding(0.0f, 0.0f, 0.0f, 10.0f)
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.DarkGroupBorder"))
					.Padding(6.0f)
					[
						SAssignNew(LeftListScrollBox, SScrollBox)
						.ScrollBarAlwaysVisible(true)
						.ConsumeMouseWheel(EConsumeMouseWheel::Always)
						+ SScrollBox::Slot()
						[
							SAssignNew(ListContainerBox, SVerticalBox)
						]
					]
				]
				// 3. Bottom Tier (45% Height): Left = Preview & Jump Button | Right = Scrollable Detailed Telemetry
				+ SVerticalBox::Slot()
				.FillHeight(0.45f)
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.DarkGroupBorder"))
					.Padding(8.0f)
					[
						SNew(SHorizontalBox)
						// Bottom-Left: Clickable Aspect-Preserved Preview Image + Jump Button
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.0f, 0.0f, 12.0f, 0.0f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.AutoHeight()
							.HAlign(HAlign_Center)
							[
								SNew(SButton)
								.IsFocusable(false)
								.ContentPadding(FMargin(2.0f))
								.ToolTipText(FText::FromString(TEXT("Click preview to Jump to this location")))
								.OnClicked_Lambda([this]()
								{
									JumpToSelectedSnapshot();
									return FReply::Handled();
								})
								[
									SNew(SBox)
									.WidthOverride(224.0f)
									.HeightOverride(130.0f)
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
							.Padding(0.0f, 8.0f, 0.0f, 0.0f)
							[
								SNew(SButton)
								.IsFocusable(false)
								.HAlign(HAlign_Center)
								.Text(FText::FromString(TEXT("Jump (Accept / Enter)")))
								.OnClicked_Lambda([this]()
								{
									JumpToSelectedSnapshot();
									return FReply::Handled();
								})
							]
						]
						// Bottom-Right: Scrollable Detailed Telemetry Report
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SAssignNew(RightDetailScrollBox, SScrollBox)
							.ScrollBarAlwaysVisible(true)
							.ConsumeMouseWheel(EConsumeMouseWheel::Always)
							+ SScrollBox::Slot()
							[
								SAssignNew(DetailTextBlock, STextBlock)
								.Font(BodyFont)
								.AutoWrapText(true)
								.Text(FText::FromString(TEXT("Select a snapshot above to inspect its full stat, memory, GPU Top 10, and custom Title state.")))
							]
						]
					]
				]
			]
		]
		// === Right Half of Screen (50% width): Transparent Hit-Test Pass-Through ===
		+ SHorizontalBox::Slot()
		.FillWidth(0.50f)
		[
			SNew(SSpacer)
			.Visibility(EVisibility::SelfHitTestInvisible)
		];
}

void UWcwSnapJumpBrowserWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshSnapshotList();
	ForceMenuFocus();
}

void UWcwSnapJumpBrowserWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Continuously enforce focus onto this menu while open (unless UE debug console is open for typing)
	const bool bConsoleOpen = (GEngine && GEngine->GameViewport && GEngine->GameViewport->ViewportConsole && GEngine->GameViewport->ViewportConsole->ConsoleActive());
	if (!bConsoleOpen && IsInViewport())
	{
		if (!HasAnyUserFocus() && !HasFocusedDescendants())
		{
			ForceMenuFocus();
		}
	}
}

void UWcwSnapJumpBrowserWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	LeftListScrollBox.Reset();
	ListContainerBox.Reset();
	RightDetailScrollBox.Reset();
	SelectedPreviewImage.Reset();
	HeaderTitleText.Reset();
	DetailTextBlock.Reset();
	RowThumbnailBrushes.Reset();
	RowWidgets.Reset();
	RowSummaryTexts.Reset();
}

bool UWcwSnapJumpBrowserWidget::ProcessMenuKeyDown(const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();

	// Deduplicate same key within the exact same frame (in case both IInputProcessor and NativeOnKeyDown see it)
	if (LastHandledKeyFrame == GFrameCounter)
	{
		return true;
	}

	if (Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_LeftStick_Up)
	{
		LastHandledKeyFrame = GFrameCounter;
		MoveSelectionByDelta(-1);
		return true;
	}
	if (Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_LeftStick_Down)
	{
		LastHandledKeyFrame = GFrameCounter;
		MoveSelectionByDelta(1);
		return true;
	}
	if (Key == EKeys::PageUp || Key == EKeys::Gamepad_LeftShoulder)
	{
		LastHandledKeyFrame = GFrameCounter;
		MoveSelectionByDelta(-5);
		return true;
	}
	if (Key == EKeys::PageDown || Key == EKeys::Gamepad_RightShoulder)
	{
		LastHandledKeyFrame = GFrameCounter;
		MoveSelectionByDelta(5);
		return true;
	}
	if (Key == EKeys::Home)
	{
		LastHandledKeyFrame = GFrameCounter;
		SelectSnapshotByIndex(0, true);
		return true;
	}
	if (Key == EKeys::End && Entries.Num() > 0)
	{
		LastHandledKeyFrame = GFrameCounter;
		SelectSnapshotByIndex(Entries.Num() - 1, true);
		return true;
	}
	if (Key == EKeys::Gamepad_RightStick_Up)
	{
		LastHandledKeyFrame = GFrameCounter;
		ScrollDetailPaneByDelta(-80.0f);
		return true;
	}
	if (Key == EKeys::Gamepad_RightStick_Down)
	{
		LastHandledKeyFrame = GFrameCounter;
		ScrollDetailPaneByDelta(80.0f);
		return true;
	}

	// Resolve platform physical Accept & Back keys directly via InputCore's Virtual_Accept / Virtual_Gamepad_Accept!
	// Why: In InputCoreTypes.cpp, UE initializes Virtual_Accept to FPlatformInput::GetGamepadAcceptKey()
	// (Gamepad_FaceButton_Right ['A'] on Switch/Switch2, Gamepad_FaceButton_Bottom ['A'/Cross] on Xbox/PS5/PC).
	// Checking Key == GamepadAcceptKey directly works EVEN WHEN the game disables Slate's FNavigationConfig!
#if (ENGINE_MAJOR_VERSION > 5) || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)
	FKey GamepadAcceptKey = EKeys::Virtual_Gamepad_Accept.GetVirtualKey();
	FKey GamepadBackKey   = EKeys::Virtual_Gamepad_Back.GetVirtualKey();
#else
	FKey GamepadAcceptKey = EKeys::Virtual_Accept;
	FKey GamepadBackKey   = EKeys::Virtual_Back;
#endif
	if (!GamepadAcceptKey.IsValid())
	{
		GamepadAcceptKey = EKeys::Gamepad_FaceButton_Bottom;
	}
	if (!GamepadBackKey.IsValid())
	{
		GamepadBackKey = EKeys::Gamepad_FaceButton_Right;
	}

	const EUINavigationAction NavAction = FSlateApplication::Get().GetNavigationActionFromKey(InKeyEvent);

	// Accept / Jump: Platform Accept Key (Switch A / Xbox A), Top Face Button (X/Y backup), or Enter / SpaceBar
	if (Key == GamepadAcceptKey || Key == EKeys::Gamepad_FaceButton_Top || NavAction == EUINavigationAction::Accept || Key == EKeys::Enter || Key == EKeys::SpaceBar)
	{
		LastHandledKeyFrame = GFrameCounter;
		JumpToSelectedSnapshot();
		return true;
	}

	// Back / Close: Platform Back Key (Switch B / Xbox B) or Escape
	if (Key == GamepadBackKey || NavAction == EUINavigationAction::Back || Key == EKeys::Escape)
	{
		LastHandledKeyFrame = GFrameCounter;
		CloseBrowserMenu();
		return true;
	}

	return false;
}

bool UWcwSnapJumpBrowserWidget::ProcessMenuAnalogInput(const FAnalogInputEvent& InAnalogEvent)
{
	const FKey Key = InAnalogEvent.GetKey();
	const float AnalogVal = InAnalogEvent.GetAnalogValue();

	if (Key == EKeys::Gamepad_RightY && FMath::Abs(AnalogVal) > 0.2f)
	{
		ScrollDetailPaneByDelta(-AnalogVal * 18.0f);
		return true;
	}

	if (Key == EKeys::Gamepad_LeftY && FMath::Abs(AnalogVal) > 0.55f)
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - LastAnalogNavTime > 0.16)
		{
			LastAnalogNavTime = Now;
			MoveSelectionByDelta(AnalogVal > 0.0f ? -1 : 1);
		}
		return true;
	}

	if ((Key == EKeys::Gamepad_LeftX || Key == EKeys::Gamepad_RightX) && FMath::Abs(AnalogVal) > 0.2f)
	{
		return true;
	}

	return false;
}

FReply UWcwSnapJumpBrowserWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (ProcessMenuKeyDown(InKeyEvent))
	{
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UWcwSnapJumpBrowserWidget::NativeOnAnalogValueChanged(const FGeometry& InGeometry, const FAnalogInputEvent& InAnalogEvent)
{
	if (ProcessMenuAnalogInput(InAnalogEvent))
	{
		return FReply::Handled();
	}
	return Super::NativeOnAnalogValueChanged(InGeometry, InAnalogEvent);
}

void UWcwSnapJumpBrowserWidget::RefreshFontScale()
{
	const float Scale = UWcwSnapJumpSubsystem::GetFontScale();
	const FSlateFontInfo HeaderFont = FCoreStyle::GetDefaultFontStyle("Bold", FMath::RoundToInt(11.0f * Scale));
	const FSlateFontInfo RowFont = FCoreStyle::GetDefaultFontStyle("Regular", FMath::RoundToInt(9.5f * Scale));
	const FSlateFontInfo DetailFont = FCoreStyle::GetDefaultFontStyle("Regular", FMath::RoundToInt(9.5f * Scale));

	if (HeaderTitleText.IsValid())
	{
		HeaderTitleText->SetFont(HeaderFont);
	}
	if (DetailTextBlock.IsValid())
	{
		DetailTextBlock->SetFont(DetailFont);
	}
	for (const TSharedPtr<STextBlock>& RowText : RowSummaryTexts)
	{
		if (RowText.IsValid())
		{
			RowText->SetFont(RowFont);
		}
	}
}

void UWcwSnapJumpBrowserWidget::RefreshSnapshotList()
{
	Entries.Reset();
	CachedThumbnails.Reset();
	RowThumbnailBrushes.Reset();
	RowWidgets.Reset();
	RowSummaryTexts.Reset();
	SelectedIndex = INDEX_NONE;

	TSet<FString> RegisteredTagsAndTimes;
	TArray<FString> DiagLines;

	// 0. Live Session Captures (in RAM)
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UWcwSnapJumpSubsystem* Subsystem = GI->GetSubsystem<UWcwSnapJumpSubsystem>())
		{
			const TArray<FWcwSnapJumpData>& SessionSnaps = Subsystem->GetSessionCapturedSnaps();
			const TArray<TObjectPtr<UTexture2D>>& SessionThumbs = Subsystem->GetSessionCapturedThumbnails();
			DiagLines.Add(FString::Printf(TEXT("Live Session Captures: %d"), SessionSnaps.Num()));
			for (int32 i = SessionSnaps.Num() - 1; i >= 0; --i)
			{
				Entries.Add(SessionSnaps[i]);
				CachedThumbnails.Add(SessionThumbs.IsValidIndex(i) ? SessionThumbs[i].Get() : nullptr);
				RegisteredTagsAndTimes.Add(SessionSnaps[i].CaptureTag + TEXT("_") + SessionSnaps[i].Timestamp);
			}
		}
	}

	// 1. Collect Candidate UDataTables:
	//    (a) Explicit SourceDataTable on Widget
	//    (b) Project Settings DefaultSnapJumpDataTable
	//    (c) Standard default path /Game/WcwSnapJump/DT_WcwSnapJump.DT_WcwSnapJump
	//    (d) Automatic Runtime AssetRegistry scan for ANY cooked UDataTable using FWcwSnapJumpTableRow!
	TArray<UDataTable*> CandidateTables;
	if (SourceDataTable)
	{
		CandidateTables.AddUnique(SourceDataTable);
	}

	const UWcwSnapJumpSettings* Settings = GetDefault<UWcwSnapJumpSettings>();
	if (Settings)
	{
		const FString ConfiguredPath = Settings->DefaultSnapJumpDataTable.ToString();
		UDataTable* ConfiguredDT = Settings->DefaultSnapJumpDataTable.IsValid()
			? Cast<UDataTable>(Settings->DefaultSnapJumpDataTable.TryLoad())
			: nullptr;
		DiagLines.Add(FString::Printf(TEXT("Settings DefaultSnapJumpDataTable: '%s' -> %s"),
			*ConfiguredPath, ConfiguredDT ? TEXT("Loaded OK") : TEXT("NULL")));
		if (ConfiguredDT)
		{
			CandidateTables.AddUnique(ConfiguredDT);
		}
	}

	if (UDataTable* FallbackDefaultDT = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, TEXT("/Game/WcwSnapJump/DT_WcwSnapJump.DT_WcwSnapJump"), nullptr, LOAD_NoWarn)))
	{
		CandidateTables.AddUnique(FallbackDefaultDT);
	}

	// Runtime AssetRegistry scan: finds ANY cooked DataTable with FWcwSnapJumpTableRow even if .ini wasn't staged!
	if (FModuleManager::Get().IsModuleLoaded("AssetRegistry") || FModuleManager::Get().LoadModule("AssetRegistry") != nullptr)
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		TArray<FAssetData> AllDataTableAssets;
		AssetRegistryModule.Get().GetAssetsByClass(UDataTable::StaticClass()->GetClassPathName(), AllDataTableAssets, true);

		int32 MatchedRegistryTables = 0;
		for (const FAssetData& AssetData : AllDataTableAssets)
		{
			FString RowStructTag;
			const bool bTagMatches = AssetData.GetTagValue(TEXT("RowStructure"), RowStructTag) &&
			                         RowStructTag.Contains(TEXT("WcwSnapJumpTableRow"));
			if (bTagMatches)
			{
				if (UDataTable* LoadedDT = Cast<UDataTable>(AssetData.GetAsset()))
				{
					CandidateTables.AddUnique(LoadedDT);
					++MatchedRegistryTables;
				}
			}
		}
		DiagLines.Add(FString::Printf(TEXT("AssetRegistry UDataTables: %d total, %d matched FWcwSnapJumpTableRow"),
			AllDataTableAssets.Num(), MatchedRegistryTables));
	}

	for (UDataTable* Table : CandidateTables)
	{
		if (!Table)
		{
			continue;
		}
		const UScriptStruct* RowStruct = Table->GetRowStruct();
		const FString StructName = RowStruct ? RowStruct->GetName() : TEXT("None");
		if (RowStruct != FWcwSnapJumpTableRow::StaticStruct())
		{
			DiagLines.Add(FString::Printf(TEXT("Skipped DataTable '%s': RowStruct is '%s' (expected FWcwSnapJumpTableRow)"),
				*Table->GetName(), *StructName));
			continue;
		}

		const TArray<FWcwSnapJumpTableRow> Rows = UWcwSnapJumpLibrary::GetResolvedRowsFromDataTable(Table);
		DiagLines.Add(FString::Printf(TEXT("Loaded DataTable '%s': %d rows"), *Table->GetName(), Rows.Num()));

		for (const FWcwSnapJumpTableRow& Row : Rows)
		{
			const FString DedupKey = Row.CachedSnapData.CaptureTag + TEXT("_") + Row.CachedSnapData.Timestamp;
			if (RegisteredTagsAndTimes.Contains(DedupKey))
			{
				continue;
			}

			UTexture2D* Thumb = Row.PrebakedThumbnail.LoadSynchronous();
			if (!Thumb)
			{
				const FString PathToLoad = !Row.ScreenshotFilePath.IsEmpty() ? Row.ScreenshotFilePath : Row.CachedSnapData.SourceFilePath;
				if (!PathToLoad.IsEmpty())
				{
					bool bUnused = false;
					FWcwSnapJumpData Dummy;
					Thumb = UWcwSnapJumpLibrary::LoadSnapThumbnailAndData(PathToLoad, Dummy, bUnused);
				}
			}
			Entries.Add(Row.CachedSnapData);
			CachedThumbnails.Add(Thumb);
			RegisteredTagsAndTimes.Add(DedupKey);
		}
	}

	// 2. Scan Saved/WcwSnapJump on disk if accessible
	if (bIncludeSavedDirectorySnapshots)
	{
		const TArray<FWcwSnapJumpData> DiskEntries = UWcwSnapJumpLibrary::ScanSnapDirectory();
		DiagLines.Add(FString::Printf(TEXT("Saved Dir ('%s') PNGs: %d"),
			*UWcwSnapJumpSettings::GetResolvedScreenshotDirectory(), DiskEntries.Num()));

		for (const FWcwSnapJumpData& DiskSnap : DiskEntries)
		{
			const FString DedupKey = DiskSnap.CaptureTag + TEXT("_") + DiskSnap.Timestamp;
			if (RegisteredTagsAndTimes.Contains(DedupKey))
			{
				continue;
			}

			bool bUnused = false;
			FWcwSnapJumpData Dummy;
			UTexture2D* Thumb = UWcwSnapJumpLibrary::LoadSnapThumbnailAndData(DiskSnap.SourceFilePath, Dummy, bUnused);
			Entries.Add(DiskSnap);
			CachedThumbnails.Add(Thumb);
			RegisteredTagsAndTimes.Add(DedupKey);
		}
	}

	const float Scale = UWcwSnapJumpSubsystem::GetFontScale();
	const FSlateFontInfo RowFont = FCoreStyle::GetDefaultFontStyle("Regular", FMath::RoundToInt(9.5f * Scale));

	if (ListContainerBox.IsValid())
	{
		ListContainerBox->ClearChildren();
		for (int32 i = 0; i < Entries.Num(); ++i)
		{
			const int32 ItemIndex = i;
			const FString Summary = UWcwSnapJumpLibrary::FormatCompactThumbnailSummary(Entries[i]);

			UTexture2D* Tex = CachedThumbnails.IsValidIndex(i) ? CachedThumbnails[i] : nullptr;
			TSharedPtr<FSlateImageBrush> Brush;
			if (Tex)
			{
				Brush = MakeShared<FSlateImageBrush>(
					Tex,
					FVector2D(static_cast<float>(Tex->GetSizeX()), static_cast<float>(Tex->GetSizeY())));
			}
			RowThumbnailBrushes.Add(Brush);

			const FSlateBrush* BrushToUse = Brush.IsValid() ? Brush.Get() : FCoreStyle::Get().GetDefaultBrush();

			TSharedPtr<STextBlock> SummaryTextWidget;
			TSharedPtr<SWidget> RowBorderWidget = SNew(SBorder)
				.Padding(4.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(SButton)
						.IsFocusable(false)
						.ContentPadding(FMargin(2.0f))
						.ToolTipText(FText::FromString(TEXT("Click Thumbnail to Jump to this location")))
						.OnClicked_Lambda([this, ItemIndex]()
						{
							SelectSnapshotByIndex(ItemIndex, false);
							if (bJumpOnThumbnailClick)
							{
								JumpToSelectedSnapshot();
							}
							return FReply::Handled();
						})
						[
							SNew(SBox)
							.WidthOverride(112.0f)
							.HeightOverride(64.0f)
							[
								SNew(SScaleBox)
								.Stretch(EStretch::ScaleToFit)
								[
									SNew(SImage)
									.Image(BrushToUse)
								]
							]
						]
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SButton)
						.IsFocusable(false)
						.OnClicked_Lambda([this, ItemIndex]()
						{
							SelectSnapshotByIndex(ItemIndex, false);
							return FReply::Handled();
						})
						[
							SAssignNew(SummaryTextWidget, STextBlock)
							.Font(RowFont)
							.AutoWrapText(true)
							.Text(FText::FromString(Summary))
						]
					]
				];

			RowWidgets.Add(RowBorderWidget);
			RowSummaryTexts.Add(SummaryTextWidget);

			ListContainerBox->AddSlot()
				.AutoHeight()
				.Padding(0.0f, 3.0f)
				[
					RowBorderWidget.ToSharedRef()
				];
		}

		if (Entries.Num() > 0)
		{
			SelectSnapshotByIndex(0, true);
		}
		else if (DetailTextBlock.IsValid())
		{
			TArray<FString> Report;
			Report.Add(TEXT("[No Snapshots Loaded - Diagnostic Info]"));
			Report.Append(DiagLines);
			Report.Add(TEXT(""));
			Report.Add(TEXT("Hint: In Editor, open Window -> Wcw SnapJump Viewer, click 'Import Textures & Build DataTable (For Package)', confirm the .uasset files are saved in Content/, then Package!"));
			DetailTextBlock->SetText(FText::FromString(FString::Join(Report, LINE_TERMINATOR)));
		}
	}

	RefreshFontScale();
}

void UWcwSnapJumpBrowserWidget::SelectSnapshotByIndex(int32 Index, bool bScrollIntoView)
{
	if (!Entries.IsValidIndex(Index))
	{
		return;
	}

	SelectedIndex = Index;
	const FWcwSnapJumpData& SnapData = Entries[Index];
	UTexture2D* Thumb = CachedThumbnails.IsValidIndex(Index) ? CachedThumbnails[Index] : nullptr;

	for (int32 i = 0; i < RowWidgets.Num(); ++i)
	{
		if (TSharedPtr<SBorder> Border = StaticCastSharedPtr<SBorder>(RowWidgets[i]))
		{
			Border->SetBorderBackgroundColor(i == SelectedIndex
				? FLinearColor(0.15f, 0.55f, 1.0f, 1.0f)
				: FLinearColor(0.2f, 0.2f, 0.2f, 0.6f));
		}
	}

	if (bScrollIntoView && LeftListScrollBox.IsValid() && RowWidgets.IsValidIndex(Index) && RowWidgets[Index].IsValid())
	{
		LeftListScrollBox->ScrollDescendantIntoView(RowWidgets[Index], true, EDescendantScrollDestination::IntoView);
	}

	if (SelectedPreviewImage.IsValid())
	{
		const FSlateBrush* BrushToUse = (RowThumbnailBrushes.IsValidIndex(Index) && RowThumbnailBrushes[Index].IsValid())
			? RowThumbnailBrushes[Index].Get()
			: FCoreStyle::Get().GetDefaultBrush();
		SelectedPreviewImage->SetImage(BrushToUse);
	}

	if (DetailTextBlock.IsValid())
	{
		DetailTextBlock->SetText(FText::FromString(UWcwSnapJumpLibrary::FormatDetailedTelemetryReport(SnapData)));
	}

	OnSnapshotEntrySelected(SnapData, Thumb);
}

void UWcwSnapJumpBrowserWidget::MoveSelectionByDelta(int32 Delta)
{
	if (Entries.Num() == 0)
	{
		return;
	}

	const int32 BaseIdx = (SelectedIndex == INDEX_NONE) ? 0 : SelectedIndex;
	const int32 NextIdx = FMath::Clamp(BaseIdx + Delta, 0, Entries.Num() - 1);
	SelectSnapshotByIndex(NextIdx, true);
}

void UWcwSnapJumpBrowserWidget::ScrollDetailPaneByDelta(float ScrollDeltaPixels)
{
	if (RightDetailScrollBox.IsValid())
	{
		const float NewOffset = FMath::Max(0.0f, RightDetailScrollBox->GetScrollOffset() + ScrollDeltaPixels);
		RightDetailScrollBox->SetScrollOffset(NewOffset);
	}
}

void UWcwSnapJumpBrowserWidget::ForceMenuFocus()
{
	if (FSlateApplication::IsInitialized())
	{
		TSharedRef<SWidget> SafeWidget = TakeWidget();
		FSlateApplication::Get().SetAllUserFocus(SafeWidget, EFocusCause::SetDirectly);
		FSlateApplication::Get().SetKeyboardFocus(SafeWidget, EFocusCause::SetDirectly);
	}
}

void UWcwSnapJumpBrowserWidget::JumpToSelectedSnapshot()
{
	if (Entries.IsValidIndex(SelectedIndex))
	{
		UWcwSnapJumpLibrary::JumpToSnapData(this, Entries[SelectedIndex], true, true);
	}
}

void UWcwSnapJumpBrowserWidget::CloseBrowserMenu()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UWcwSnapJumpSubsystem* Subsystem = GI->GetSubsystem<UWcwSnapJumpSubsystem>())
		{
			Subsystem->SetBrowserMenuVisible(false);
			return;
		}
	}
	RemoveFromParent();
}
