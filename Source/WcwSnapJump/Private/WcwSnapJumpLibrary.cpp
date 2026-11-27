// Copyright (c) 2026 WinterCrownWORKS. All Rights Reserved.
// Licensed under the MIT License.

#include "WcwSnapJumpLibrary.h"
#include "WcwSnapJumpSerializer.h"
#include "WcwSnapJumpSettings.h"
#include "WcwSnapJumpSubsystem.h"
#include "WcwSnapJumpHandler.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LevelStreaming.h"
#include "Engine/Texture2D.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageUtils.h"
#include "Modules/ModuleManager.h"
#include "HAL/FileManager.h"
#include "Misc/Char.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "TextureResource.h"

namespace WcwSnapJumpInternal
{
	static UWcwSnapJumpHandler* ResolveFormattingHandler()
	{
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
		return GetMutableDefault<UWcwSnapJumpHandler>(HandlerClass);
	}

	static FString StripPiePrefixFromLeafSegment(const FString& InSegment)
	{
		const FString PiePrefix = TEXT("UEDPIE_");
		if (!InSegment.StartsWith(PiePrefix, ESearchCase::IgnoreCase))
		{
			return InSegment;
		}

		const int32 Cursor = PiePrefix.Len();
		int32 DigitEnd = Cursor;
		while (DigitEnd < InSegment.Len() && FChar::IsDigit(InSegment[DigitEnd]))
		{
			++DigitEnd;
		}

		if (DigitEnd > Cursor && DigitEnd < InSegment.Len() && InSegment[DigitEnd] == TEXT('_'))
		{
			return InSegment.Mid(DigitEnd + 1);
		}
		return InSegment.Mid(Cursor);
	}
}

void UWcwSnapJumpLibrary::CaptureWcwSnap(UObject* WorldContextObject, const FString& CaptureTag)
{
	if (!GEngine || !WorldContextObject)
	{
		return;
	}

	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UWcwSnapJumpSubsystem* Subsystem = GI->GetSubsystem<UWcwSnapJumpSubsystem>())
			{
				Subsystem->RequestCaptureWithMeta(CaptureTag);
			}
		}
	}
}

void UWcwSnapJumpLibrary::SetWcwCustomTitleTag(UObject* WorldContextObject, const FString& Key, const FString& Value)
{
	if (!GEngine || !WorldContextObject)
	{
		return;
	}

	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UWcwSnapJumpSubsystem* Subsystem = GI->GetSubsystem<UWcwSnapJumpSubsystem>())
			{
				Subsystem->SetCustomTitleTag(Key, Value);
			}
		}
	}
}

bool UWcwSnapJumpLibrary::GetWcwCustomTitleTag(const FWcwSnapJumpData& SnapData, const FString& Key, FString& OutValue)
{
	if (const FString* Found = SnapData.CustomTitleTags.Find(Key))
	{
		OutValue = *Found;
		return true;
	}
	OutValue.Reset();
	return false;
}

void UWcwSnapJumpLibrary::NotifyWcwLevelLoadCompleted(UObject* WorldContextObject)
{
	if (!GEngine || !WorldContextObject)
	{
		return;
	}

	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UWcwSnapJumpSubsystem* Subsystem = GI->GetSubsystem<UWcwSnapJumpSubsystem>())
			{
				Subsystem->NotifyTitleLevelLoadCompleted();
			}
		}
	}
}

FString UWcwSnapJumpLibrary::NormalizeLevelPackagePath(const FString& InLevelOrPackagePath)
{
	FString Result = InLevelOrPackagePath.TrimStartAndEnd();
	if (Result.IsEmpty() || Result.Equals(TEXT("None"), ESearchCase::IgnoreCase))
	{
		return FString();
	}

	if (Result.StartsWith(TEXT("/")))
	{
		Result = FPackageName::ObjectPathToPackageName(Result);
	}
	else
	{
		int32 DotIndex = INDEX_NONE;
		if (Result.FindLastChar(TEXT('.'), DotIndex) && DotIndex > 0)
		{
			Result = Result.Left(DotIndex);
		}
	}

	Result = UWorld::RemovePIEPrefix(Result);

	int32 LastSlash = INDEX_NONE;
	if (Result.FindLastChar(TEXT('/'), LastSlash))
	{
		const FString DirPart = Result.Left(LastSlash + 1);
		const FString LeafPart = WcwSnapJumpInternal::StripPiePrefixFromLeafSegment(Result.Mid(LastSlash + 1));
		Result = DirPart + LeafPart;
	}
	else
	{
		Result = WcwSnapJumpInternal::StripPiePrefixFromLeafSegment(Result);
	}

	return Result;
}

bool UWcwSnapJumpLibrary::IsTransientStreamingLevelPackage(const FString& InPackagePath)
{
	const FString Trimmed = InPackagePath.TrimStartAndEnd();
	if (Trimmed.IsEmpty() || Trimmed.Equals(TEXT("None"), ESearchCase::IgnoreCase))
	{
		return true;
	}

	if (Trimmed.StartsWith(TEXT("/Memory/"), ESearchCase::IgnoreCase) ||
		Trimmed.StartsWith(TEXT("/Temp/"), ESearchCase::IgnoreCase) ||
		Trimmed.StartsWith(TEXT("/Engine/Transient"), ESearchCase::IgnoreCase) ||
		Trimmed.Contains(TEXT("/_Generated_/"), ESearchCase::IgnoreCase))
	{
		return true;
	}

	return false;
}

bool UWcwSnapJumpLibrary::DoesStreamingLevelMatchPackage(const ULevelStreaming* StreamingLevel, const FString& TargetLevelOrPackage)
{
	if (!StreamingLevel)
	{
		return false;
	}

	const FString NormTarget = NormalizeLevelPackagePath(TargetLevelOrPackage);
	if (NormTarget.IsEmpty() || IsTransientStreamingLevelPackage(NormTarget))
	{
		return false;
	}

	const FString NormWorldAsset = NormalizeLevelPackagePath(StreamingLevel->GetWorldAssetPackageName());
	const FString NormLoadPkg = !StreamingLevel->PackageNameToLoad.IsNone()
		? NormalizeLevelPackagePath(StreamingLevel->PackageNameToLoad.ToString())
		: FString();

	if (NormTarget.StartsWith(TEXT("/")))
	{
		if (!NormWorldAsset.IsEmpty() && NormWorldAsset.Equals(NormTarget, ESearchCase::IgnoreCase))
		{
			return true;
		}
		if (!NormLoadPkg.IsEmpty() && NormLoadPkg.Equals(NormTarget, ESearchCase::IgnoreCase))
		{
			return true;
		}
		return false;
	}

	if (!NormWorldAsset.IsEmpty() && FPackageName::GetShortName(NormWorldAsset).Equals(NormTarget, ESearchCase::IgnoreCase))
	{
		return true;
	}
	if (!NormLoadPkg.IsEmpty() && FPackageName::GetShortName(NormLoadPkg).Equals(NormTarget, ESearchCase::IgnoreCase))
	{
		return true;
	}
	return false;
}

bool UWcwSnapJumpLibrary::ReadSnapDataFromPngFile(const FString& PngFilePath, FWcwSnapJumpData& OutSnapData)
{
	return FWcwSnapJumpSerializer::ReadMetaFromFile(PngFilePath, OutSnapData);
}

UTexture2D* UWcwSnapJumpLibrary::LoadSnapThumbnailAndData(const FString& PngFilePath, FWcwSnapJumpData& OutSnapData, bool& bOutMetaValid)
{
	bOutMetaValid = false;

	FString ResolvedPath = PngFilePath;
#if PLATFORM_DESKTOP || WITH_EDITOR
	if (FPaths::IsRelative(ResolvedPath))
	{
		ResolvedPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), ResolvedPath);
	}
#endif

	TArray<uint8> FileBytes;
	if (!FFileHelper::LoadFileToArray(FileBytes, *ResolvedPath))
	{
		return nullptr;
	}

	bOutMetaValid = FWcwSnapJumpSerializer::ExtractMetaFromPngBytes(FileBytes, OutSnapData);
	if (!bOutMetaValid)
	{
		bOutMetaValid = FWcwSnapJumpSerializer::ReadMetaFromFile(ResolvedPath, OutSnapData);
	}
	else
	{
		OutSnapData.SourceFilePath = ResolvedPath;
	}

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

	if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(FileBytes.GetData(), FileBytes.Num()))
	{
		return FImageUtils::ImportBufferAsTexture2D(FileBytes);
	}

	TArray64<uint8> UncompressedBGRA;
	if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, UncompressedBGRA))
	{
		return FImageUtils::ImportBufferAsTexture2D(FileBytes);
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

	UTexture2D* NewTexture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
	if (!NewTexture || !NewTexture->GetPlatformData() || NewTexture->GetPlatformData()->Mips.Num() == 0)
	{
		return nullptr;
	}

	NewTexture->SRGB = true;
	NewTexture->NeverStream = true;

	void* TextureData = NewTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(TextureData, UncompressedBGRA.GetData(), UncompressedBGRA.Num());
	NewTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
	NewTexture->UpdateResource();

	return NewTexture;
}

TArray<FWcwSnapJumpData> UWcwSnapJumpLibrary::ScanSnapDirectory(const FString& OptionalDirectoryPath)
{
	TArray<FWcwSnapJumpData> Results;
	TSet<FString> SeenFileNamesLower;

	auto ScanSingleDirectory = [&Results, &SeenFileNamesLower](const FString& InDir, const TCHAR* FileWildcard)
	{
		if (InDir.IsEmpty())
		{
			return;
		}

		TArray<FString> FoundFiles;
		IFileManager::Get().FindFiles(FoundFiles, *(InDir / FileWildcard), true, false);
		for (const FString& FileName : FoundFiles)
		{
			const FString LowerKey = FileName.ToLower();
			if (SeenFileNamesLower.Contains(LowerKey))
			{
				continue;
			}

			const FString FullPath = InDir / FileName;
			FWcwSnapJumpData SnapData;
			if (FWcwSnapJumpSerializer::ReadMetaFromFile(FullPath, SnapData))
			{
				SeenFileNamesLower.Add(LowerKey);
				Results.Add(MoveTemp(SnapData));
			}
		}
	};

	const FString TargetDir = OptionalDirectoryPath.IsEmpty()
		? UWcwSnapJumpSettings::GetResolvedScreenshotDirectory()
		: OptionalDirectoryPath;

	ScanSingleDirectory(TargetDir, TEXT("*.png"));

#if PLATFORM_DESKTOP || WITH_EDITOR
	// On Desktop/Editor, scan all Saved/Screenshots/<Platform>/<RelDir> folders (e.g. WindowsEditor, Windows, Console)
	// plus legacy Saved/<RelDir> for backward compatibility with snapshots captured in v0.2.1 and earlier.
	if (OptionalDirectoryPath.IsEmpty())
	{
		const UWcwSnapJumpSettings* Settings = GetDefault<UWcwSnapJumpSettings>();
		const FString RelDir = (Settings && !Settings->SaveDirectoryRelative.IsEmpty())
			? Settings->SaveDirectoryRelative
			: TEXT("WcwSnapJump");

		const FString ScreenshotsRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Screenshots"));
		TArray<FString> PlatformSubdirs;
		IFileManager::Get().FindFiles(PlatformSubdirs, *(ScreenshotsRoot / TEXT("*")), false, true);
		for (const FString& PlatDir : PlatformSubdirs)
		{
			ScanSingleDirectory(ScreenshotsRoot / PlatDir / RelDir, TEXT("*.png"));
		}

		const FString LegacySavedDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / RelDir);
		ScanSingleDirectory(LegacySavedDir, TEXT("*.png"));
	}
#endif

	Results.Sort([](const FWcwSnapJumpData& A, const FWcwSnapJumpData& B)
	{
		return A.Timestamp > B.Timestamp;
	});

	return Results;
}

TArray<FWcwSnapJumpTableRow> UWcwSnapJumpLibrary::GetResolvedRowsFromDataTable(UDataTable* DataTable)
{
	TArray<FWcwSnapJumpTableRow> OutRows;
	if (!DataTable)
	{
		return OutRows;
	}

	TArray<FWcwSnapJumpTableRow*> RawRows;
	DataTable->GetAllRows<FWcwSnapJumpTableRow>(TEXT("UWcwSnapJumpLibrary::GetResolvedRowsFromDataTable"), RawRows);

	for (const FWcwSnapJumpTableRow* RowPtr : RawRows)
	{
		if (!RowPtr)
		{
			continue;
		}

		FWcwSnapJumpTableRow CopyRow = *RowPtr;
		if (CopyRow.CachedSnapData.LevelName.IsEmpty() && !CopyRow.ScreenshotFilePath.IsEmpty())
		{
			FWcwSnapJumpSerializer::ReadMetaFromFile(CopyRow.ScreenshotFilePath, CopyRow.CachedSnapData);
		}
		if (CopyRow.CachedSnapData.SourceFilePath.IsEmpty() && !CopyRow.ScreenshotFilePath.IsEmpty())
		{
			CopyRow.CachedSnapData.SourceFilePath = CopyRow.ScreenshotFilePath;
		}
		OutRows.Add(MoveTemp(CopyRow));
	}

	return OutRows;
}

bool UWcwSnapJumpLibrary::JumpToSnapData(UObject* WorldContextObject, const FWcwSnapJumpData& SnapData, bool bAutoLoadLevelIfDifferent, bool bAutoTeleportPlayer)
{
	if (!GEngine || !WorldContextObject)
	{
		return false;
	}

	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UWcwSnapJumpSubsystem* Subsystem = GI->GetSubsystem<UWcwSnapJumpSubsystem>())
			{
				return Subsystem->JumpToSnapLocation(WorldContextObject, SnapData, bAutoLoadLevelIfDifferent, bAutoTeleportPlayer);
			}
		}
	}
	return false;
}

FString UWcwSnapJumpLibrary::FormatCompactThumbnailSummary(const FWcwSnapJumpData& SnapData)
{
	if (UWcwSnapJumpHandler* Handler = WcwSnapJumpInternal::ResolveFormattingHandler())
	{
		return Handler->FormatThumbnailSummary(SnapData);
	}
	return SnapData.CaptureTag;
}

FString UWcwSnapJumpLibrary::FormatDetailedTelemetryReport(const FWcwSnapJumpData& SnapData)
{
	if (UWcwSnapJumpHandler* Handler = WcwSnapJumpInternal::ResolveFormattingHandler())
	{
		return Handler->FormatDetailedReport(SnapData);
	}
	return SnapData.CaptureTag;
}

void UWcwSnapJumpLibrary::GatherIssueTrackerUiFields(const FWcwSnapJumpData& SnapData, TArray<FWcwIssueTrackerUiField>& OutUiFields)
{
	OutUiFields.Reset();
	if (UWcwSnapJumpHandler* Handler = WcwSnapJumpInternal::ResolveFormattingHandler())
	{
		Handler->GatherIssueTrackerUiFields(SnapData, OutUiFields);
	}
}

FString UWcwSnapJumpLibrary::FormatIssueTrackerSubject(const FWcwSnapJumpData& SnapData, const FString& UserEnteredTitle)
{
	if (UWcwSnapJumpHandler* Handler = WcwSnapJumpInternal::ResolveFormattingHandler())
	{
		return Handler->FormatIssueTrackerSubject(SnapData, UserEnteredTitle);
	}
	return SnapData.CaptureTag.IsEmpty()
		? UserEnteredTitle
		: FString::Printf(TEXT("[%s] %s"), *SnapData.CaptureTag, *UserEnteredTitle);
}

FString UWcwSnapJumpLibrary::FormatIssueTrackerTelemetryBlock(const FWcwSnapJumpData& SnapData)
{
	if (UWcwSnapJumpHandler* Handler = WcwSnapJumpInternal::ResolveFormattingHandler())
	{
		return Handler->FormatIssueTrackerTelemetryBlock(SnapData);
	}
	return FString();
}

void UWcwSnapJumpLibrary::GatherIssueTrackerPresetSelection(const FWcwSnapJumpData& SnapData, FWcwIssueTrackerPresetSelection& OutPresetSelection)
{
	OutPresetSelection = FWcwIssueTrackerPresetSelection();
	if (UWcwSnapJumpHandler* Handler = WcwSnapJumpInternal::ResolveFormattingHandler())
	{
		Handler->GatherIssueTrackerPresetSelection(SnapData, OutPresetSelection);
	}
}

void UWcwSnapJumpLibrary::GatherIssueTrackerIssueFields(const FWcwSnapJumpData& SnapData, TMap<FString, FString>& InOutIssueFields)
{
	if (UWcwSnapJumpHandler* Handler = WcwSnapJumpInternal::ResolveFormattingHandler())
	{
		Handler->GatherIssueTrackerIssueFields(SnapData, InOutIssueFields);
	}
}

void UWcwSnapJumpLibrary::GatherIssueTrackerCustomFields(const FWcwSnapJumpData& SnapData, TMap<FString, FString>& InOutCustomFields)
{
	if (UWcwSnapJumpHandler* Handler = WcwSnapJumpInternal::ResolveFormattingHandler())
	{
		Handler->GatherIssueTrackerCustomFields(SnapData, InOutCustomFields);
	}
}