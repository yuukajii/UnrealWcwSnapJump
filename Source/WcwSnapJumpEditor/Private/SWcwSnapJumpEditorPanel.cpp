// Copyright (c) 2026 WinterCrownWORKS. All Rights Reserved.
// Licensed under the MIT License.

#include "SWcwSnapJumpEditorPanel.h"
#include "WcwSnapJumpModule.h"
#include "WcwSnapJumpLibrary.h"
#include "WcwSnapJumpSettings.h"
#include "Editor.h"
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
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SWcwSnapJumpEditorPanel"

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
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(8.0f, 0.0f, 8.0f, 8.0f)
		[
			SNew(SSplitter)
			+ SSplitter::Slot()
			.Value(0.62f)
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
					+ SHeaderRow::Column("Level").DefaultLabel(LOCTEXT("ColLevel", "Level")).FillWidth(0.13f)
					+ SHeaderRow::Column("Platform").DefaultLabel(LOCTEXT("ColPlatform", "Platform / GPU")).FillWidth(0.18f)
					+ SHeaderRow::Column("FPS").DefaultLabel(LOCTEXT("ColFPS", "FPS / Frame")).FillWidth(0.12f)
					+ SHeaderRow::Column("GPU").DefaultLabel(LOCTEXT("ColGPU", "GPU ms")).FillWidth(0.09f)
					+ SHeaderRow::Column("TopPass").DefaultLabel(LOCTEXT("ColTopPass", "GPU #1 Bottleneck")).FillWidth(0.16f)
					+ SHeaderRow::Column("Memory").DefaultLabel(LOCTEXT("ColMemory", "Memory")).FillWidth(0.11f)
					+ SHeaderRow::Column("Timestamp").DefaultLabel(LOCTEXT("ColTime", "Timestamp")).FillWidth(0.09f)
				)
			]
			+ SSplitter::Slot()
			.Value(0.38f)
			[
				SNew(SBorder)
				.Padding(10.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					.Padding(0.0f, 0.0f, 0.0f, 8.0f)
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
							.HeightOverride(166.0f)
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
					.FillHeight(1.0f)
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						[
							SAssignNew(DetailReportText, STextBlock)
							.Text(LOCTEXT("SelectPrompt", "Select a snapshot row on the left (or click its thumbnail to jump) to inspect its preview, full stat, memory, GPU Top 10, and custom Title state."))
						]
					]
				]
			]
		]
	];

	OnRefreshClicked();
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

	// Physically save the .uasset to disk immediately so external Cook processes always find it!
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
		// Write directly into Config/DefaultGame.ini so Packaged / Console builds always receive DefaultSnapJumpDataTable!
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

	// Automatically register TableFolder in ProjectPackagingSettings -> DirectoriesToAlwaysCook (DefaultGame.ini)!
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

	// Physically save the DataTable .uasset to disk immediately so external Cook processes always include all rows!
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
