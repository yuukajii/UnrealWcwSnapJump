// Copyright (c) 2026 WinterCrownWORKS. All Rights Reserved.
// Licensed under the MIT License.

#include "WcwSnapJumpRedmineSettings.h"
#include "UObject/UnrealType.h"

UWcwSnapJumpRedmineSettings::UWcwSnapJumpRedmineSettings()
	: DefaultTrackerName(TEXT("\u30D0\u30B0")) // Japanese "Bug" (\u30D0\u30B0) (falls back to "Bug" or first tracker if not found)
{
}

FString UWcwSnapJumpRedmineSettings::GetNormalizedBaseUrl() const
{
	FString Clean = RedmineBaseUrl.TrimStartAndEnd();
	while (Clean.EndsWith(TEXT("/")))
	{
		Clean.LeftChopInline(1, EAllowShrinking::No);
	}
	return Clean;
}

bool UWcwSnapJumpRedmineSettings::ParseRedmineProjectUrl(const FString& InFullUrl, FString& OutBaseUrl, FString& OutProjectId)
{
	FString Trimmed = InFullUrl.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		return false;
	}

	int32 QueryIdx = INDEX_NONE;
	if (Trimmed.FindChar(TEXT('?'), QueryIdx))
	{
		Trimmed.LeftInline(QueryIdx, EAllowShrinking::No);
	}
	int32 HashIdx = INDEX_NONE;
	if (Trimmed.FindChar(TEXT('#'), HashIdx))
	{
		Trimmed.LeftInline(HashIdx, EAllowShrinking::No);
	}

	const FString Marker = TEXT("/projects/");
	const int32 MarkerPos = Trimmed.Find(Marker, ESearchCase::IgnoreCase, ESearchDir::FromStart);
	if (MarkerPos == INDEX_NONE)
	{
		return false;
	}

	OutBaseUrl = Trimmed.Left(MarkerPos);
	while (OutBaseUrl.EndsWith(TEXT("/")))
	{
		OutBaseUrl.LeftChopInline(1, EAllowShrinking::No);
	}

	FString Remainder = Trimmed.Mid(MarkerPos + Marker.Len());
	int32 NextSlash = INDEX_NONE;
	if (Remainder.FindChar(TEXT('/'), NextSlash))
	{
		Remainder.LeftInline(NextSlash, EAllowShrinking::No);
	}
	if (Remainder.EndsWith(TEXT(".json"), ESearchCase::IgnoreCase))
	{
		Remainder.LeftChopInline(5, EAllowShrinking::No);
	}
	else if (Remainder.EndsWith(TEXT(".xml"), ESearchCase::IgnoreCase))
	{
		Remainder.LeftChopInline(4, EAllowShrinking::No);
	}

	OutProjectId = Remainder.TrimStartAndEnd();
	return !OutBaseUrl.IsEmpty() && !OutProjectId.IsEmpty();
}

#if WITH_EDITOR
void UWcwSnapJumpRedmineSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropName = PropertyChangedEvent.GetPropertyName();
	if (PropName == GET_MEMBER_NAME_CHECKED(UWcwSnapJumpRedmineSettings, PasteProjectUrlHelper))
	{
		FString ParsedBase;
		FString ParsedProj;
		if (ParseRedmineProjectUrl(PasteProjectUrlHelper, ParsedBase, ParsedProj))
		{
			RedmineBaseUrl = ParsedBase;
			ProjectIdentifier = ParsedProj;
			PasteProjectUrlHelper.Reset();
		}
		else if (!PasteProjectUrlHelper.TrimStartAndEnd().IsEmpty())
		{
			RedmineBaseUrl = PasteProjectUrlHelper.TrimStartAndEnd();
			while (RedmineBaseUrl.EndsWith(TEXT("/")))
			{
				RedmineBaseUrl.LeftChopInline(1, EAllowShrinking::No);
			}
			PasteProjectUrlHelper.Reset();
		}
	}
	else if (PropName == GET_MEMBER_NAME_CHECKED(UWcwSnapJumpRedmineSettings, RedmineBaseUrl))
	{
		FString ParsedBase;
		FString ParsedProj;
		if (ParseRedmineProjectUrl(RedmineBaseUrl, ParsedBase, ParsedProj))
		{
			RedmineBaseUrl = ParsedBase;
			if (ProjectIdentifier.IsEmpty())
			{
				ProjectIdentifier = ParsedProj;
			}
		}
		else
		{
			RedmineBaseUrl = GetNormalizedBaseUrl();
		}
	}

	SaveConfig();
	TryUpdateDefaultConfigFile();
}
#endif

void UWcwSnapJumpRedmineUserSettings::SaveUserConfig()
{
	ApiAccessKey = ApiAccessKey.TrimStartAndEnd();
	SaveConfig();
}
