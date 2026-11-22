// Copyright (c) 2026 WinterCrownWORKS. All Rights Reserved.
// Licensed under the MIT License.

#include "WcwRedmineClient.h"
#include "WcwSnapJumpLibrary.h"
#include "WcwSnapJumpSerializer.h"
#include "HttpModule.h"
#include "PlatformHttp.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace WcwRedmineJsonKeys
{
	static const TCHAR* PostedIssueId = TEXT("redminePostedIssueId");
	static const TCHAR* PostedIssueUrl = TEXT("redminePostedIssueUrl");
	static const TCHAR* PostedAt = TEXT("redminePostedAt");

	static bool ExtractLocalizedOrPlainName(const TSharedPtr<FJsonObject>& Obj, FString& OutName)
	{
		if (!Obj.IsValid())
		{
			return false;
		}
		if (Obj->TryGetStringField(TEXT("name"), OutName) && !OutName.IsEmpty())
		{
			return true;
		}
		const TSharedPtr<FJsonObject>* NameObj = nullptr;
		if (Obj->TryGetObjectField(TEXT("name"), NameObj) && NameObj && NameObj->IsValid())
		{
			if ((*NameObj)->TryGetStringField(TEXT("default"), OutName) && !OutName.IsEmpty())
			{
				return true;
			}
		}
		return false;
	}
}

FString FWcwRedmineClient::NormalizeBaseUrl(const FString& InUrl)
{
	FString Clean = InUrl.TrimStartAndEnd();
	while (Clean.EndsWith(TEXT("/")))
	{
		Clean.LeftChopInline(1, EAllowShrinking::No);
	}
	return Clean;
}

FString FWcwRedmineClient::ExtractRedmineErrorDetail(int32 HttpCode, const FString& ResponseBody)
{
	TArray<FString> DetailLines;

	if (!ResponseBody.IsEmpty())
	{
		TSharedPtr<FJsonObject> RootObj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
		if (FJsonSerializer::Deserialize(Reader, RootObj) && RootObj.IsValid())
		{
			const TArray<TSharedPtr<FJsonValue>>* ErrorsArray = nullptr;
			if (RootObj->TryGetArrayField(TEXT("errors"), ErrorsArray) && ErrorsArray)
			{
				for (const TSharedPtr<FJsonValue>& Val : *ErrorsArray)
				{
					if (Val.IsValid() && Val->Type == EJson::String)
					{
						DetailLines.Add(FString::Printf(TEXT("- %s"), *Val->AsString()));
					}
				}
			}
		}
	}

	if (DetailLines.Num() > 0)
	{
		return FString::Printf(
			TEXT("HTTP %d:%s%s"),
			HttpCode,
			LINE_TERMINATOR,
			*FString::Join(DetailLines, LINE_TERMINATOR));
	}

	if (HttpCode == 401)
	{
		return TEXT("HTTP 401 Unauthorized: Invalid API Access Key, or REST API authentication is disabled in Redmine (Administration -> Settings -> API -> Enable REST web service).");
	}
	if (HttpCode == 403)
	{
		return TEXT("HTTP 403 Forbidden: Current user does not have permission for this Redmine project/operation.");
	}
	if (HttpCode == 404)
	{
		return TEXT("HTTP 404 Not Found: Please verify RedmineBaseUrl and ProjectIdentifier in Project Settings.");
	}

	const FString Snippet = ResponseBody.Left(240).Replace(TEXT("\r"), TEXT(" ")).Replace(TEXT("\n"), TEXT(" "));
	return Snippet.IsEmpty()
		? FString::Printf(TEXT("HTTP %d"), HttpCode)
		: FString::Printf(TEXT("HTTP %d: %s"), HttpCode, *Snippet);
}

void FWcwRedmineClient::EnrichCustomFieldsMetadataAsync(
	const FString& BaseUrl,
	const FString& EncodedProj,
	const FString& ApiKey,
	bool bProjectFilteredList,
	TArray<FWcwRedmineOptionItem>&& InitialDefs,
	TFunction<void(TArray<FWcwRedmineOptionItem>&&)> OnComplete)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> AdminCfReq = FHttpModule::Get().CreateRequest();
	AdminCfReq->SetURL(FString::Printf(TEXT("%s/custom_fields.json"), *BaseUrl));
	AdminCfReq->SetVerb(TEXT("GET"));
	AdminCfReq->SetHeader(TEXT("X-Redmine-API-Key"), ApiKey);
	AdminCfReq->SetHeader(TEXT("Accept"), TEXT("application/json"));

	AdminCfReq->OnProcessRequestComplete().BindLambda(
		[BaseUrl, EncodedProj, ApiKey, bProjectFilteredList, Defs = MoveTemp(InitialDefs), OnComplete]
		(FHttpRequestPtr AReq, FHttpResponsePtr AResp, bool bAdminConn) mutable
		{
			if (bAdminConn && AResp.IsValid() && AResp->GetResponseCode() >= 200 && AResp->GetResponseCode() < 300)
			{
				TSharedPtr<FJsonObject> RootObj;
				TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(AResp->GetContentAsString());
				if (FJsonSerializer::Deserialize(Reader, RootObj) && RootObj.IsValid())
				{
					const TArray<TSharedPtr<FJsonValue>>* CfArr = nullptr;
					if (RootObj->TryGetArrayField(TEXT("custom_fields"), CfArr) && CfArr)
					{
						for (const TSharedPtr<FJsonValue>& Val : *CfArr)
						{
							const TSharedPtr<FJsonObject>* ItemObj = nullptr;
							if (!Val.IsValid() || !Val->TryGetObject(ItemObj) || !ItemObj || !ItemObj->IsValid())
							{
								continue;
							}

							FString CustomizedType;
							(*ItemObj)->TryGetStringField(TEXT("customized_type"), CustomizedType);
							if (!CustomizedType.IsEmpty() && !CustomizedType.Equals(TEXT("issue"), ESearchCase::IgnoreCase))
							{
								continue;
							}

							int32 Id = 0;
							FString Name;
							if (!(*ItemObj)->TryGetNumberField(TEXT("id"), Id) || Id <= 0 ||
							    !WcwRedmineJsonKeys::ExtractLocalizedOrPlainName(*ItemObj, Name))
							{
								continue;
							}

							bool bMultiple = false;
							(*ItemObj)->TryGetBoolField(TEXT("multiple"), bMultiple);

							FString FieldFormat;
							(*ItemObj)->TryGetStringField(TEXT("field_format"), FieldFormat);

							FString DefaultValue;
							(*ItemObj)->TryGetStringField(TEXT("default_value"), DefaultValue);

							TArray<FWcwIssueTrackerUiOption> PossibleValues;
							const TArray<TSharedPtr<FJsonValue>>* PossibleArr = nullptr;
							if ((*ItemObj)->TryGetArrayField(TEXT("possible_values"), PossibleArr) && PossibleArr)
							{
								for (const TSharedPtr<FJsonValue>& PvVal : *PossibleArr)
								{
									if (!PvVal.IsValid())
									{
										continue;
									}
									if (PvVal->Type == EJson::String)
									{
										const FString Str = PvVal->AsString();
										PossibleValues.Emplace(Str, Str);
									}
									else if (PvVal->Type == EJson::Object)
									{
										const TSharedPtr<FJsonObject>* PvObj = nullptr;
										if (PvVal->TryGetObject(PvObj) && PvObj && PvObj->IsValid())
										{
											FString OptVal;
											FString OptLabel;
											(*PvObj)->TryGetStringField(TEXT("value"), OptVal);
											(*PvObj)->TryGetStringField(TEXT("label"), OptLabel);
											if (OptVal.IsEmpty() && !OptLabel.IsEmpty())
											{
												OptVal = OptLabel;
											}
											if (OptLabel.IsEmpty())
											{
												OptLabel = OptVal;
											}
											if (!OptVal.IsEmpty() || !OptLabel.IsEmpty())
											{
												PossibleValues.Emplace(OptLabel, OptVal);
											}
										}
									}
								}
							}

							FWcwRedmineOptionItem* Existing = Defs.FindByPredicate(
								[Id](const FWcwRedmineOptionItem& Candidate) { return Candidate.Id == Id; });

							if (Existing)
							{
								Existing->bMultiple = Existing->bMultiple || bMultiple;
								Existing->FieldFormat = FieldFormat;
								Existing->DefaultValue = DefaultValue;
								if (PossibleValues.Num() > 0)
								{
									Existing->PossibleValues = MoveTemp(PossibleValues);
								}
							}
							else if (!bProjectFilteredList)
							{
								FWcwRedmineOptionItem NewItem(Id, Name);
								NewItem.bMultiple = bMultiple;
								NewItem.FieldFormat = FieldFormat;
								NewItem.DefaultValue = DefaultValue;
								NewItem.PossibleValues = MoveTemp(PossibleValues);
								Defs.Add(MoveTemp(NewItem));
							}
						}
					}
				}

				if (OnComplete)
				{
					OnComplete(MoveTemp(Defs));
				}
				return;
			}

			// Non-admin fallback: inspect 1 project issue via GET /issues.json?project_id=<id>&status_id=*&limit=1
			// so `"multiple": true` on multiselect custom fields is still detected without admin privileges!
			if (Defs.Num() == 0 || EncodedProj.IsEmpty())
			{
				if (OnComplete)
				{
					OnComplete(MoveTemp(Defs));
				}
				return;
			}

			TSharedRef<IHttpRequest, ESPMode::ThreadSafe> SampleIssueReq = FHttpModule::Get().CreateRequest();
			SampleIssueReq->SetURL(FString::Printf(TEXT("%s/issues.json?project_id=%s&status_id=*&limit=1"), *BaseUrl, *EncodedProj));
			SampleIssueReq->SetVerb(TEXT("GET"));
			SampleIssueReq->SetHeader(TEXT("X-Redmine-API-Key"), ApiKey);
			SampleIssueReq->SetHeader(TEXT("Accept"), TEXT("application/json"));

			SampleIssueReq->OnProcessRequestComplete().BindLambda(
				[Defs = MoveTemp(Defs), OnComplete]
				(FHttpRequestPtr SReq, FHttpResponsePtr SResp, bool bSampleConn) mutable
				{
					if (bSampleConn && SResp.IsValid() && SResp->GetResponseCode() >= 200 && SResp->GetResponseCode() < 300)
					{
						TSharedPtr<FJsonObject> RootObj;
						TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SResp->GetContentAsString());
						if (FJsonSerializer::Deserialize(Reader, RootObj) && RootObj.IsValid())
						{
							const TArray<TSharedPtr<FJsonValue>>* IssuesArr = nullptr;
							if (RootObj->TryGetArrayField(TEXT("issues"), IssuesArr) && IssuesArr && IssuesArr->Num() > 0)
							{
								const TSharedPtr<FJsonObject>* FirstIssue = nullptr;
								if ((*IssuesArr)[0].IsValid() && (*IssuesArr)[0]->TryGetObject(FirstIssue) && FirstIssue && FirstIssue->IsValid())
								{
									const TArray<TSharedPtr<FJsonValue>>* IssueCfs = nullptr;
									if ((*FirstIssue)->TryGetArrayField(TEXT("custom_fields"), IssueCfs) && IssueCfs)
									{
										for (const TSharedPtr<FJsonValue>& CfVal : *IssueCfs)
										{
											const TSharedPtr<FJsonObject>* CfObj = nullptr;
											if (!CfVal.IsValid() || !CfVal->TryGetObject(CfObj) || !CfObj || !CfObj->IsValid())
											{
												continue;
											}
											int32 CfId = 0;
											if (!(*CfObj)->TryGetNumberField(TEXT("id"), CfId) || CfId <= 0)
											{
												continue;
											}
											bool bMult = false;
											(*CfObj)->TryGetBoolField(TEXT("multiple"), bMult);
											const TSharedPtr<FJsonValue> RawValField = (*CfObj)->TryGetField(TEXT("value"));
											if (RawValField.IsValid() && RawValField->Type == EJson::Array)
											{
												bMult = true;
											}
											if (bMult)
											{
												for (FWcwRedmineOptionItem& Existing : Defs)
												{
													if (Existing.Id == CfId)
													{
														Existing.bMultiple = true;
														break;
													}
												}
											}
										}
									}
								}
							}
						}
					}

					if (OnComplete)
					{
						OnComplete(MoveTemp(Defs));
					}
				});

			SampleIssueReq->ProcessRequest();
		});

	AdminCfReq->ProcessRequest();
}

void FWcwRedmineClient::ResolveCustomFieldsToNumericIds(
	const TMap<FString, FString>& RawCustomFields,
	const TSet<FString>& MultiValueKeys,
	const TArray<FWcwRedmineOptionItem>& KnownDefs,
	TArray<FWcwResolvedCustomFieldEntry>& OutResolvedEntries,
	TArray<FString>& OutUnresolvedKeys)
{
	OutResolvedEntries.Reset();
	OutUnresolvedKeys.Reset();

	for (const TPair<FString, FString>& Pair : RawCustomFields)
	{
		const FString Key = Pair.Key.TrimStartAndEnd();
		if (Key.IsEmpty())
		{
			continue;
		}

		int32 ResolvedId = 0;
		bool bDefMultiple = MultiValueKeys.Contains(Key);

		// 1. Exact case-sensitive name match
		for (const FWcwRedmineOptionItem& Def : KnownDefs)
		{
			if (Def.Id > 0 && Def.Name.Equals(Key, ESearchCase::CaseSensitive))
			{
				ResolvedId = Def.Id;
				bDefMultiple = bDefMultiple || Def.bMultiple;
				break;
			}
		}

		// 2. Case-insensitive name match
		if (ResolvedId <= 0)
		{
			for (const FWcwRedmineOptionItem& Def : KnownDefs)
			{
				if (Def.Id > 0 && Def.Name.Equals(Key, ESearchCase::IgnoreCase))
				{
					ResolvedId = Def.Id;
					bDefMultiple = bDefMultiple || Def.bMultiple;
					break;
				}
			}
		}

		// 3. Explicit numeric ID string (e.g. "1" or "#1")
		if (ResolvedId <= 0)
		{
			FString DigitsCandidate = Key;
			DigitsCandidate.RemoveFromStart(TEXT("#"));
			if (!DigitsCandidate.IsEmpty() && DigitsCandidate.IsNumeric())
			{
				const int32 ParsedId = FCString::Atoi(*DigitsCandidate);
				if (ParsedId > 0)
				{
					ResolvedId = ParsedId;
					for (const FWcwRedmineOptionItem& Def : KnownDefs)
					{
						if (Def.Id == ResolvedId)
						{
							bDefMultiple = bDefMultiple || Def.bMultiple;
							break;
						}
					}
				}
			}
		}

		if (ResolvedId > 0)
		{
			FWcwResolvedCustomFieldEntry* Existing = OutResolvedEntries.FindByPredicate(
				[ResolvedId](const FWcwResolvedCustomFieldEntry& Item) { return Item.Id == ResolvedId; });
			if (Existing)
			{
				Existing->RawValue = Pair.Value;
				Existing->bSendAsArray = Existing->bSendAsArray || bDefMultiple;
			}
			else
			{
				FWcwResolvedCustomFieldEntry Entry;
				Entry.Id = ResolvedId;
				Entry.RawValue = Pair.Value;
				Entry.bSendAsArray = bDefMultiple;
				OutResolvedEntries.Add(MoveTemp(Entry));
			}
		}
		else
		{
			OutUnresolvedKeys.AddUnique(Key);
		}
	}
}

void FWcwRedmineClient::FetchCustomFieldDefsOnDemandAsync(
	const FString& BaseUrl,
	const FString& ProjectId,
	const FString& ApiKey,
	TFunction<void(const TArray<FWcwRedmineOptionItem>&)> OnComplete)
{
	const FString EncodedProj = FPlatformHttp::UrlEncode(ProjectId);
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> ProjCfReq = FHttpModule::Get().CreateRequest();
	ProjCfReq->SetURL(FString::Printf(TEXT("%s/projects/%s.json?include=issue_custom_fields"), *BaseUrl, *EncodedProj));
	ProjCfReq->SetVerb(TEXT("GET"));
	ProjCfReq->SetHeader(TEXT("X-Redmine-API-Key"), ApiKey);
	ProjCfReq->SetHeader(TEXT("Accept"), TEXT("application/json"));

	ProjCfReq->OnProcessRequestComplete().BindLambda(
		[BaseUrl, EncodedProj, ApiKey, OnComplete](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bConnected)
		{
			TArray<FWcwRedmineOptionItem> CollectedDefs;
			TSet<int32> SeenIds;
			bool bProjHasIssueCustomFieldsArray = false;

			if (bConnected && Resp.IsValid() && Resp->GetResponseCode() >= 200 && Resp->GetResponseCode() < 300)
			{
				TSharedPtr<FJsonObject> RootObj;
				TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Resp->GetContentAsString());
				if (FJsonSerializer::Deserialize(Reader, RootObj) && RootObj.IsValid())
				{
					const TSharedPtr<FJsonObject>* ProjObj = nullptr;
					if (RootObj->TryGetObjectField(TEXT("project"), ProjObj) && ProjObj && ProjObj->IsValid())
					{
						const TArray<TSharedPtr<FJsonValue>>* CfArr = nullptr;
						if ((*ProjObj)->TryGetArrayField(TEXT("issue_custom_fields"), CfArr) && CfArr)
						{
							bProjHasIssueCustomFieldsArray = true;
							for (const TSharedPtr<FJsonValue>& Val : *CfArr)
							{
								const TSharedPtr<FJsonObject>* ItemObj = nullptr;
								if (Val.IsValid() && Val->TryGetObject(ItemObj) && ItemObj && ItemObj->IsValid())
								{
									int32 Id = 0;
									FString Name;
									if ((*ItemObj)->TryGetNumberField(TEXT("id"), Id) && Id > 0 &&
									    WcwRedmineJsonKeys::ExtractLocalizedOrPlainName(*ItemObj, Name) && !SeenIds.Contains(Id))
									{
										SeenIds.Add(Id);
										FWcwRedmineOptionItem Item(Id, Name);
										(*ItemObj)->TryGetBoolField(TEXT("multiple"), Item.bMultiple);
										CollectedDefs.Add(MoveTemp(Item));
									}
								}
							}
						}
					}
				}
			}

			EnrichCustomFieldsMetadataAsync(
				BaseUrl,
				EncodedProj,
				ApiKey,
				bProjHasIssueCustomFieldsArray,
				MoveTemp(CollectedDefs),
				[OnComplete](TArray<FWcwRedmineOptionItem>&& EnrichedDefs)
				{
					if (OnComplete)
					{
						OnComplete(EnrichedDefs);
					}
				});
		});

	ProjCfReq->ProcessRequest();
}

void FWcwRedmineClient::VerifyAndFetchProjectMetaAsync(
	const FString& InBaseUrl,
	const FString& InProjectIdentifier,
	const FString& InApiKey,
	FOnVerifyAndFetchComplete OnComplete)
{
	const FString BaseUrl = NormalizeBaseUrl(InBaseUrl);
	const FString ProjectId = InProjectIdentifier.TrimStartAndEnd();
	const FString ApiKey = InApiKey.TrimStartAndEnd();

	if (BaseUrl.IsEmpty() || ProjectId.IsEmpty())
	{
		if (OnComplete)
		{
			OnComplete(false, FWcwRedmineProjectMeta(), TEXT("RedmineBaseUrl or ProjectIdentifier is empty. Configure them in Project Settings -> Wcw Snap Jump (Redmine Project)."));
		}
		return;
	}
	if (ApiKey.IsEmpty())
	{
		if (OnComplete)
		{
			OnComplete(false, FWcwRedmineProjectMeta(), TEXT("Personal API Access Key is not set."));
		}
		return;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> UserReq = FHttpModule::Get().CreateRequest();
	UserReq->SetURL(FString::Printf(TEXT("%s/users/current.json"), *BaseUrl));
	UserReq->SetVerb(TEXT("GET"));
	UserReq->SetHeader(TEXT("X-Redmine-API-Key"), ApiKey);
	UserReq->SetHeader(TEXT("Accept"), TEXT("application/json"));

	UserReq->OnProcessRequestComplete().BindLambda(
		[BaseUrl, ProjectId, ApiKey, OnComplete](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bConnected)
		{
			if (!bConnected || !Resp.IsValid())
			{
				if (OnComplete)
				{
					OnComplete(false, FWcwRedmineProjectMeta(), FString::Printf(TEXT("Could not connect to Redmine server (%s). Check URL/Port or VPN."), *BaseUrl));
				}
				return;
			}

			const int32 UserCode = Resp->GetResponseCode();
			if (UserCode < 200 || UserCode >= 300)
			{
				if (OnComplete)
				{
					OnComplete(false, FWcwRedmineProjectMeta(), ExtractRedmineErrorDetail(UserCode, Resp->GetContentAsString()));
				}
				return;
			}

			int32 CurrentUserId = 0;
			FString DisplayUserName = TEXT("Authenticated");
			{
				TSharedPtr<FJsonObject> RootObj;
				TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Resp->GetContentAsString());
				if (FJsonSerializer::Deserialize(Reader, RootObj) && RootObj.IsValid())
				{
					const TSharedPtr<FJsonObject>* UserObj = nullptr;
					if (RootObj->TryGetObjectField(TEXT("user"), UserObj) && UserObj && UserObj->IsValid())
					{
						(*UserObj)->TryGetNumberField(TEXT("id"), CurrentUserId);

						FString FirstName;
						FString LastName;
						FString Login;
						(*UserObj)->TryGetStringField(TEXT("firstname"), FirstName);
						(*UserObj)->TryGetStringField(TEXT("lastname"), LastName);
						(*UserObj)->TryGetStringField(TEXT("login"), Login);

						const FString FullName = FString::Printf(TEXT("%s %s"), *FirstName, *LastName).TrimStartAndEnd();
						if (!FullName.IsEmpty())
						{
							DisplayUserName = FullName;
						}
						else if (!Login.IsEmpty())
						{
							DisplayUserName = Login;
						}
					}
				}
			}

			const FString EncodedProj = FPlatformHttp::UrlEncode(ProjectId);
			TSharedRef<IHttpRequest, ESPMode::ThreadSafe> ProjReq = FHttpModule::Get().CreateRequest();
			ProjReq->SetURL(FString::Printf(TEXT("%s/projects/%s.json?include=trackers,issue_categories,issue_custom_fields"), *BaseUrl, *EncodedProj));
			ProjReq->SetVerb(TEXT("GET"));
			ProjReq->SetHeader(TEXT("X-Redmine-API-Key"), ApiKey);
			ProjReq->SetHeader(TEXT("Accept"), TEXT("application/json"));

			ProjReq->OnProcessRequestComplete().BindLambda(
				[BaseUrl, EncodedProj, ApiKey, CurrentUserId, DisplayUserName, OnComplete]
				(FHttpRequestPtr PReq, FHttpResponsePtr PResp, bool bProjConnected)
				{
					if (!bProjConnected || !PResp.IsValid())
					{
						if (OnComplete)
						{
							OnComplete(false, FWcwRedmineProjectMeta(), TEXT("Network error while fetching Redmine project metadata."));
						}
						return;
					}

					const int32 ProjCode = PResp->GetResponseCode();
					if (ProjCode < 200 || ProjCode >= 300)
					{
						if (OnComplete)
						{
							OnComplete(false, FWcwRedmineProjectMeta(), ExtractRedmineErrorDetail(ProjCode, PResp->GetContentAsString()));
						}
						return;
					}

					FWcwRedmineProjectMeta OutMeta;
					OutMeta.CurrentUserId = CurrentUserId;
					OutMeta.CurrentUserDisplayName = DisplayUserName;

					TSharedPtr<FJsonObject> RootObj;
					TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PResp->GetContentAsString());
					if (!FJsonSerializer::Deserialize(Reader, RootObj) || !RootObj.IsValid())
					{
						if (OnComplete)
						{
							OnComplete(false, OutMeta, TEXT("Failed to parse JSON from /projects/<id>.json."));
						}
						return;
					}

					bool bProjHasCustomFieldsList = false;
					const TSharedPtr<FJsonObject>* ProjObj = nullptr;
					if (RootObj->TryGetObjectField(TEXT("project"), ProjObj) && ProjObj && ProjObj->IsValid())
					{
						(*ProjObj)->TryGetNumberField(TEXT("id"), OutMeta.NumericProjectId);
						(*ProjObj)->TryGetStringField(TEXT("name"), OutMeta.ProjectName);

						const TArray<TSharedPtr<FJsonValue>>* TrackersArr = nullptr;
						if ((*ProjObj)->TryGetArrayField(TEXT("trackers"), TrackersArr) && TrackersArr)
						{
							for (const TSharedPtr<FJsonValue>& Val : *TrackersArr)
							{
								const TSharedPtr<FJsonObject>* ItemObj = nullptr;
								if (Val.IsValid() && Val->TryGetObject(ItemObj) && ItemObj && ItemObj->IsValid())
								{
									int32 Id = 0;
									FString Name;
									if ((*ItemObj)->TryGetNumberField(TEXT("id"), Id) && WcwRedmineJsonKeys::ExtractLocalizedOrPlainName(*ItemObj, Name))
									{
										OutMeta.Trackers.Emplace(Id, Name);
									}
								}
							}
						}

						const TArray<TSharedPtr<FJsonValue>>* CategoriesArr = nullptr;
						if ((*ProjObj)->TryGetArrayField(TEXT("issue_categories"), CategoriesArr) && CategoriesArr)
						{
							for (const TSharedPtr<FJsonValue>& Val : *CategoriesArr)
							{
								const TSharedPtr<FJsonObject>* ItemObj = nullptr;
								if (Val.IsValid() && Val->TryGetObject(ItemObj) && ItemObj && ItemObj->IsValid())
								{
									int32 Id = 0;
									FString Name;
									if ((*ItemObj)->TryGetNumberField(TEXT("id"), Id) && WcwRedmineJsonKeys::ExtractLocalizedOrPlainName(*ItemObj, Name))
									{
										OutMeta.Categories.Emplace(Id, Name);
									}
								}
							}
						}

						const TArray<TSharedPtr<FJsonValue>>* CustomFieldsArr = nullptr;
						if ((*ProjObj)->TryGetArrayField(TEXT("issue_custom_fields"), CustomFieldsArr) && CustomFieldsArr)
						{
							bProjHasCustomFieldsList = true;
							for (const TSharedPtr<FJsonValue>& Val : *CustomFieldsArr)
							{
								const TSharedPtr<FJsonObject>* ItemObj = nullptr;
								if (Val.IsValid() && Val->TryGetObject(ItemObj) && ItemObj && ItemObj->IsValid())
								{
									int32 Id = 0;
									FString Name;
									if ((*ItemObj)->TryGetNumberField(TEXT("id"), Id) && WcwRedmineJsonKeys::ExtractLocalizedOrPlainName(*ItemObj, Name))
									{
										FWcwRedmineOptionItem CfItem(Id, Name);
										(*ItemObj)->TryGetBoolField(TEXT("multiple"), CfItem.bMultiple);
										OutMeta.IssueCustomFields.Add(MoveTemp(CfItem));
									}
								}
							}
						}
					}

					if (OutMeta.CurrentUserId > 0)
					{
						OutMeta.Assignees.Emplace(
							OutMeta.CurrentUserId,
							FString::Printf(TEXT("%s (Me)"), *OutMeta.CurrentUserDisplayName));
					}

					// Step 3: Fetch assignable project members & groups via GET /projects/<id>/memberships.json?limit=100
					TSharedRef<IHttpRequest, ESPMode::ThreadSafe> MemReq = FHttpModule::Get().CreateRequest();
					MemReq->SetURL(FString::Printf(TEXT("%s/projects/%s/memberships.json?limit=100"), *BaseUrl, *EncodedProj));
					MemReq->SetVerb(TEXT("GET"));
					MemReq->SetHeader(TEXT("X-Redmine-API-Key"), ApiKey);
					MemReq->SetHeader(TEXT("Accept"), TEXT("application/json"));

					MemReq->OnProcessRequestComplete().BindLambda(
						[BaseUrl, EncodedProj, ApiKey, bProjHasCustomFieldsList, OutMeta = MoveTemp(OutMeta), OnComplete]
						(FHttpRequestPtr MReq, FHttpResponsePtr MResp, bool bMemConnected) mutable
						{
							TSet<int32> SeenAssigneeIds;
							for (const FWcwRedmineOptionItem& Existing : OutMeta.Assignees)
							{
								SeenAssigneeIds.Add(Existing.Id);
							}

							if (bMemConnected && MResp.IsValid() && MResp->GetResponseCode() >= 200 && MResp->GetResponseCode() < 300)
							{
								TSharedPtr<FJsonObject> MemRoot;
								TSharedRef<TJsonReader<>> MemReader = TJsonReaderFactory<>::Create(MResp->GetContentAsString());
								if (FJsonSerializer::Deserialize(MemReader, MemRoot) && MemRoot.IsValid())
								{
									const TArray<TSharedPtr<FJsonValue>>* MembershipsArr = nullptr;
									if (MemRoot->TryGetArrayField(TEXT("memberships"), MembershipsArr) && MembershipsArr)
									{
										TArray<FWcwRedmineOptionItem> GroupAssignees;
										for (const TSharedPtr<FJsonValue>& Val : *MembershipsArr)
										{
											const TSharedPtr<FJsonObject>* MemObj = nullptr;
											if (!Val.IsValid() || !Val->TryGetObject(MemObj) || !MemObj || !MemObj->IsValid())
											{
												continue;
											}

											const TSharedPtr<FJsonObject>* MemberUserObj = nullptr;
											if ((*MemObj)->TryGetObjectField(TEXT("user"), MemberUserObj) && MemberUserObj && MemberUserObj->IsValid())
											{
												int32 MemberId = 0;
												FString MemberName;
												if ((*MemberUserObj)->TryGetNumberField(TEXT("id"), MemberId) &&
												    MemberId > 0 &&
												    WcwRedmineJsonKeys::ExtractLocalizedOrPlainName(*MemberUserObj, MemberName))
												{
													if (MemberId == OutMeta.CurrentUserId)
													{
														if (OutMeta.Assignees.Num() > 0 && OutMeta.Assignees[0].Id == MemberId)
														{
															OutMeta.Assignees[0].Name = FString::Printf(TEXT("%s (Me)"), *MemberName);
														}
													}
													else if (!SeenAssigneeIds.Contains(MemberId))
													{
														SeenAssigneeIds.Add(MemberId);
														OutMeta.Assignees.Emplace(MemberId, MemberName);
													}
												}
												continue;
											}

											const TSharedPtr<FJsonObject>* MemberGroupObj = nullptr;
											if ((*MemObj)->TryGetObjectField(TEXT("group"), MemberGroupObj) && MemberGroupObj && MemberGroupObj->IsValid())
											{
												int32 GroupId = 0;
												FString GroupName;
												if ((*MemberGroupObj)->TryGetNumberField(TEXT("id"), GroupId) &&
												    GroupId > 0 &&
												    WcwRedmineJsonKeys::ExtractLocalizedOrPlainName(*MemberGroupObj, GroupName) &&
												    !SeenAssigneeIds.Contains(GroupId))
												{
													SeenAssigneeIds.Add(GroupId);
													GroupAssignees.Emplace(GroupId, FString::Printf(TEXT("[Group] %s"), *GroupName));
												}
											}
										}

										OutMeta.Assignees.Append(GroupAssignees);
									}
								}
							}

							// Step 4: Fetch Redmine issue priorities via GET /enumerations/issue_priorities.json
							TSharedRef<IHttpRequest, ESPMode::ThreadSafe> PrioReq = FHttpModule::Get().CreateRequest();
							PrioReq->SetURL(FString::Printf(TEXT("%s/enumerations/issue_priorities.json"), *BaseUrl));
							PrioReq->SetVerb(TEXT("GET"));
							PrioReq->SetHeader(TEXT("X-Redmine-API-Key"), ApiKey);
							PrioReq->SetHeader(TEXT("Accept"), TEXT("application/json"));

							PrioReq->OnProcessRequestComplete().BindLambda(
								[BaseUrl, EncodedProj, ApiKey, bProjHasCustomFieldsList, OutMeta = MoveTemp(OutMeta), OnComplete]
								(FHttpRequestPtr PrReq, FHttpResponsePtr PrResp, bool bPrioConn) mutable
								{
									if (bPrioConn && PrResp.IsValid() && PrResp->GetResponseCode() >= 200 && PrResp->GetResponseCode() < 300)
									{
										TSharedPtr<FJsonObject> PrioRoot;
										TSharedRef<TJsonReader<>> PrioReader = TJsonReaderFactory<>::Create(PrResp->GetContentAsString());
										if (FJsonSerializer::Deserialize(PrioReader, PrioRoot) && PrioRoot.IsValid())
										{
											const TArray<TSharedPtr<FJsonValue>>* PrioArr = nullptr;
											if (PrioRoot->TryGetArrayField(TEXT("issue_priorities"), PrioArr) && PrioArr)
											{
												for (const TSharedPtr<FJsonValue>& Val : *PrioArr)
												{
													const TSharedPtr<FJsonObject>* ItemObj = nullptr;
													if (!Val.IsValid() || !Val->TryGetObject(ItemObj) || !ItemObj || !ItemObj->IsValid())
													{
														continue;
													}
													bool bActive = true;
													if ((*ItemObj)->TryGetBoolField(TEXT("active"), bActive) && !bActive)
													{
														continue;
													}
													int32 PrioId = 0;
													FString PrioName;
													if ((*ItemObj)->TryGetNumberField(TEXT("id"), PrioId) &&
													    PrioId > 0 &&
													    WcwRedmineJsonKeys::ExtractLocalizedOrPlainName(*ItemObj, PrioName))
													{
														bool bIsDefault = false;
														(*ItemObj)->TryGetBoolField(TEXT("is_default"), bIsDefault);
														OutMeta.Priorities.Emplace(PrioId, PrioName, bIsDefault);
														if (bIsDefault || OutMeta.DefaultPriorityId <= 0)
														{
															if (bIsDefault)
															{
																OutMeta.DefaultPriorityId = PrioId;
															}
															else if (OutMeta.DefaultPriorityId <= 0)
															{
																OutMeta.DefaultPriorityId = PrioId;
															}
														}
													}
												}
											}
										}
									}

									// Step 5: Enrich IssueCustomFields with PossibleValues & bMultiple (admin /custom_fields.json + non-admin fallback)
									TArray<FWcwRedmineOptionItem> InitialCustomFields = MoveTemp(OutMeta.IssueCustomFields);
									EnrichCustomFieldsMetadataAsync(
										BaseUrl,
										EncodedProj,
										ApiKey,
										bProjHasCustomFieldsList,
										MoveTemp(InitialCustomFields),
										[OutMeta = MoveTemp(OutMeta), OnComplete](TArray<FWcwRedmineOptionItem>&& EnrichedDefs) mutable
										{
											OutMeta.IssueCustomFields = MoveTemp(EnrichedDefs);
											if (OnComplete)
											{
												OnComplete(true, OutMeta, FString());
											}
										});
								});

							PrioReq->ProcessRequest();
						});

					MemReq->ProcessRequest();
				});

			ProjReq->ProcessRequest();
		});

	UserReq->ProcessRequest();
}

void FWcwRedmineClient::VerifyParentIssueAsync(
	const FString& InBaseUrl,
	const FString& InApiKey,
	int32 InIssueId,
	FOnVerifyParentComplete OnComplete)
{
	const FString BaseUrl = NormalizeBaseUrl(InBaseUrl);
	const FString ApiKey = InApiKey.TrimStartAndEnd();
	if (BaseUrl.IsEmpty() || ApiKey.IsEmpty() || InIssueId <= 0)
	{
		if (OnComplete)
		{
			OnComplete(false, InIssueId, FString(), TEXT("Invalid BaseUrl, API Key, or Issue ID."));
		}
		return;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(FString::Printf(TEXT("%s/issues/%d.json"), *BaseUrl, InIssueId));
	Req->SetVerb(TEXT("GET"));
	Req->SetHeader(TEXT("X-Redmine-API-Key"), ApiKey);
	Req->SetHeader(TEXT("Accept"), TEXT("application/json"));

	Req->OnProcessRequestComplete().BindLambda(
		[InIssueId, OnComplete](FHttpRequestPtr HttpReq, FHttpResponsePtr HttpResp, bool bConnected)
		{
			if (!bConnected || !HttpResp.IsValid())
			{
				if (OnComplete)
				{
					OnComplete(false, InIssueId, FString(), TEXT("Connection failed"));
				}
				return;
			}

			const int32 Code = HttpResp->GetResponseCode();
			if (Code < 200 || Code >= 300)
			{
				if (OnComplete)
				{
					OnComplete(false, InIssueId, FString(), FString::Printf(TEXT("#%d not found (HTTP %d)"), InIssueId, Code));
				}
				return;
			}

			FString Subject;
			FString TrackerName;
			TSharedPtr<FJsonObject> RootObj;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(HttpResp->GetContentAsString());
			if (FJsonSerializer::Deserialize(Reader, RootObj) && RootObj.IsValid())
			{
				const TSharedPtr<FJsonObject>* IssueObj = nullptr;
				if (RootObj->TryGetObjectField(TEXT("issue"), IssueObj) && IssueObj && IssueObj->IsValid())
				{
					(*IssueObj)->TryGetStringField(TEXT("subject"), Subject);
					const TSharedPtr<FJsonObject>* TrackerObj = nullptr;
					if ((*IssueObj)->TryGetObjectField(TEXT("tracker"), TrackerObj) && TrackerObj && TrackerObj->IsValid())
					{
						WcwRedmineJsonKeys::ExtractLocalizedOrPlainName(*TrackerObj, TrackerName);
					}
				}
			}

			const FString Summary = TrackerName.IsEmpty()
				? FString::Printf(TEXT("#%d: %s"), InIssueId, *Subject)
				: FString::Printf(TEXT("#%d [%s] %s"), InIssueId, *TrackerName, *Subject);

			if (OnComplete)
			{
				OnComplete(true, InIssueId, Summary, FString());
			}
		});

	Req->ProcessRequest();
}

void FWcwRedmineClient::UploadSingleBinaryAsync(
	const FString& BaseUrl,
	const FString& ApiKey,
	const FString& FileName,
	TArray<uint8>&& FileBytes,
	TFunction<void(bool, const FString&, const FString&)> OnComplete)
{
	const FString EncodedFileName = FPlatformHttp::UrlEncode(FileName);
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> UploadReq = FHttpModule::Get().CreateRequest();
	UploadReq->SetURL(FString::Printf(TEXT("%s/uploads.json?filename=%s"), *BaseUrl, *EncodedFileName));
	UploadReq->SetVerb(TEXT("POST"));
	UploadReq->SetHeader(TEXT("X-Redmine-API-Key"), ApiKey);
	UploadReq->SetHeader(TEXT("Content-Type"), TEXT("application/octet-stream"));
	UploadReq->SetHeader(TEXT("Accept"), TEXT("application/json"));
	UploadReq->SetContent(MoveTemp(FileBytes));

	UploadReq->OnProcessRequestComplete().BindLambda(
		[FileName, OnComplete](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bConnected)
		{
			if (!bConnected || !Resp.IsValid())
			{
				OnComplete(false, FString(), FString::Printf(TEXT("Network error while uploading '%s'."), *FileName));
				return;
			}

			const int32 Code = Resp->GetResponseCode();
			if (Code < 200 || Code >= 300)
			{
				OnComplete(false, FString(), FString::Printf(TEXT("Upload failed for '%s' (%s)"), *FileName, *ExtractRedmineErrorDetail(Code, Resp->GetContentAsString())));
				return;
			}

			FString Token;
			TSharedPtr<FJsonObject> RootObj;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Resp->GetContentAsString());
			if (FJsonSerializer::Deserialize(Reader, RootObj) && RootObj.IsValid())
			{
				const TSharedPtr<FJsonObject>* UploadObj = nullptr;
				if (RootObj->TryGetObjectField(TEXT("upload"), UploadObj) && UploadObj && UploadObj->IsValid())
				{
					(*UploadObj)->TryGetStringField(TEXT("token"), Token);
				}
			}

			if (Token.IsEmpty())
			{
				OnComplete(false, FString(), FString::Printf(TEXT("Redmine did not return an upload token for '%s'."), *FileName));
				return;
			}

			OnComplete(true, Token, FString());
		});

	UploadReq->ProcessRequest();
}

void FWcwRedmineClient::PostIssueWithAttachmentsAsync(
	const FWcwRedmineTicketRequest& InRequest,
	FOnPostIssueComplete OnComplete)
{
	const FString BaseUrl = NormalizeBaseUrl(InRequest.BaseUrl);
	const FString ProjectId = InRequest.ProjectIdentifier.TrimStartAndEnd();
	const FString ApiKey = InRequest.ApiKey.TrimStartAndEnd();

	if (BaseUrl.IsEmpty() || ProjectId.IsEmpty())
	{
		if (OnComplete)
		{
			OnComplete(false, 0, FString(), TEXT("RedmineBaseUrl or ProjectIdentifier is missing."));
		}
		return;
	}
	if (ApiKey.IsEmpty())
	{
		if (OnComplete)
		{
			OnComplete(false, 0, FString(), TEXT("Redmine API Access Key is empty."));
		}
		return;
	}
	if (InRequest.Subject.TrimStartAndEnd().IsEmpty())
	{
		if (OnComplete)
		{
			OnComplete(false, 0, FString(), TEXT("Ticket Subject cannot be empty."));
		}
		return;
	}

	TArray<FWcwResolvedCustomFieldEntry> ResolvedCustomFields;
	TArray<FString> UnresolvedKeys;
	ResolveCustomFieldsToNumericIds(
		InRequest.CustomFields,
		InRequest.MultiValueCustomFieldKeys,
		InRequest.KnownCustomFields,
		ResolvedCustomFields,
		UnresolvedKeys);

	if (UnresolvedKeys.Num() == 0)
	{
		UploadAndCreateIssueInternalAsync(InRequest, BaseUrl, ApiKey, ResolvedCustomFields, OnComplete);
		return;
	}

	// On-demand custom field definition refresh (handles newly added Custom Fields without requiring manual Sync)
	FetchCustomFieldDefsOnDemandAsync(
		BaseUrl,
		ProjectId,
		ApiKey,
		[InRequest, BaseUrl, ProjectId, ApiKey, OnComplete](const TArray<FWcwRedmineOptionItem>& FetchedDefs)
		{
			TArray<FWcwRedmineOptionItem> MergedDefs = InRequest.KnownCustomFields;
			for (const FWcwRedmineOptionItem& Def : FetchedDefs)
			{
				FWcwRedmineOptionItem* Existing = MergedDefs.FindByPredicate(
					[&Def](const FWcwRedmineOptionItem& Item) { return Item.Id == Def.Id; });
				if (Existing)
				{
					Existing->bMultiple = Existing->bMultiple || Def.bMultiple;
					if (Existing->PossibleValues.Num() == 0 && Def.PossibleValues.Num() > 0)
					{
						Existing->PossibleValues = Def.PossibleValues;
					}
				}
				else
				{
					MergedDefs.Add(Def);
				}
			}

			TArray<FWcwResolvedCustomFieldEntry> SecondPassResolved;
			TArray<FString> StillUnresolved;
			ResolveCustomFieldsToNumericIds(
				InRequest.CustomFields,
				InRequest.MultiValueCustomFieldKeys,
				MergedDefs,
				SecondPassResolved,
				StillUnresolved);

			if (StillUnresolved.Num() > 0)
			{
				TArray<FString> AvailableList;
				for (const FWcwRedmineOptionItem& Def : MergedDefs)
				{
					AvailableList.Add(FString::Printf(TEXT("%s (ID:%d)"), *Def.Name, Def.Id));
				}
				const FString AvailStr = AvailableList.Num() > 0
					? FString::Join(AvailableList, TEXT(", "))
					: TEXT("(none enabled on this project)");

				if (OnComplete)
				{
					OnComplete(
						false,
						0,
						FString(),
						FString::Printf(
							TEXT("Custom Field not found on Redmine project '%s': %s\nAvailable project custom fields: %s\n(Check Redmine Administration -> Custom fields -> enable for this Project and Tracker.)"),
							*ProjectId,
							*FString::Join(StillUnresolved, TEXT(", ")),
							*AvailStr));
				}
				return;
			}

			UploadAndCreateIssueInternalAsync(InRequest, BaseUrl, ApiKey, SecondPassResolved, OnComplete);
		});
}

void FWcwRedmineClient::UploadAndCreateIssueInternalAsync(
	const FWcwRedmineTicketRequest& InRequest,
	const FString& BaseUrl,
	const FString& ApiKey,
	const TArray<FWcwResolvedCustomFieldEntry>& ResolvedCustomFields,
	FOnPostIssueComplete OnComplete)
{
	FString ResolvedPngPath = InRequest.PngFilePath;
	if (FPaths::IsRelative(ResolvedPngPath))
	{
		ResolvedPngPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), ResolvedPngPath);
	}

	TArray<uint8> PngBytes;
	if (!FFileHelper::LoadFileToArray(PngBytes, *ResolvedPngPath))
	{
		if (OnComplete)
		{
			OnComplete(false, 0, FString(), FString::Printf(TEXT("Failed to read snapshot PNG file: %s"), *ResolvedPngPath));
		}
		return;
	}

	const FString PngFileName = FPaths::GetCleanFilename(ResolvedPngPath);
	const FString SidecarPath = FPaths::ChangeExtension(ResolvedPngPath, TEXT("json"));
	const FString JsonFileName = FPaths::GetCleanFilename(SidecarPath);

	TArray<uint8> JsonBytes;
	if (!FFileHelper::LoadFileToArray(JsonBytes, *SidecarPath))
	{
		FString GeneratedJson;
		if (FWcwSnapJumpSerializer::StructToJsonString(InRequest.SnapData, GeneratedJson, true))
		{
			FTCHARToUTF8 Utf8Conv(*GeneratedJson);
			JsonBytes.Append(reinterpret_cast<const uint8*>(Utf8Conv.Get()), Utf8Conv.Length());
		}
	}

	UploadSingleBinaryAsync(
		BaseUrl,
		ApiKey,
		PngFileName,
		MoveTemp(PngBytes),
		[InRequest, BaseUrl, ApiKey, PngFileName, JsonFileName, JsonBytes = MoveTemp(JsonBytes), ResolvedCustomFields, OnComplete]
		(bool bPngOk, const FString& PngToken, const FString& PngErr) mutable
		{
			if (!bPngOk)
			{
				if (OnComplete)
				{
					OnComplete(false, 0, FString(), PngErr);
				}
				return;
			}

			UploadSingleBinaryAsync(
				BaseUrl,
				ApiKey,
				JsonFileName,
				MoveTemp(JsonBytes),
				[InRequest, BaseUrl, ApiKey, PngFileName, PngToken, JsonFileName, ResolvedCustomFields, OnComplete]
				(bool bJsonOk, const FString& JsonToken, const FString& JsonErr)
				{
					if (!bJsonOk)
					{
						if (OnComplete)
						{
							OnComplete(false, 0, FString(), JsonErr);
						}
						return;
					}

					TSharedPtr<FJsonObject> IssueObj = MakeShared<FJsonObject>();
					IssueObj->SetStringField(TEXT("project_id"), InRequest.ProjectIdentifier.TrimStartAndEnd());
					IssueObj->SetStringField(TEXT("subject"), InRequest.Subject.TrimStartAndEnd());
					IssueObj->SetStringField(TEXT("description"), InRequest.Description);

					if (InRequest.TrackerId > 0)
					{
						IssueObj->SetNumberField(TEXT("tracker_id"), InRequest.TrackerId);
					}
					if (InRequest.CategoryId > 0)
					{
						IssueObj->SetNumberField(TEXT("category_id"), InRequest.CategoryId);
					}
					if (InRequest.AssignedToId > 0)
					{
						IssueObj->SetNumberField(TEXT("assigned_to_id"), InRequest.AssignedToId);
					}
					if (InRequest.ParentIssueId > 0)
					{
						IssueObj->SetNumberField(TEXT("parent_issue_id"), InRequest.ParentIssueId);
					}

					// Apply Title-side / UI / Project Settings standard Redmine `issue` fields (e.g. priority_id, fixed_version_id, is_private)
					for (const TPair<FString, FString>& ExtraPair : InRequest.ExtraIssueFields)
					{
						const FString Key = ExtraPair.Key.TrimStartAndEnd();
						const FString Val = ExtraPair.Value.TrimStartAndEnd();
						if (Key.IsEmpty() || Val.IsEmpty() ||
						    Key.Equals(TEXT("project_id"), ESearchCase::IgnoreCase) ||
						    Key.Equals(TEXT("uploads"), ESearchCase::IgnoreCase) ||
						    Key.Equals(TEXT("custom_fields"), ESearchCase::IgnoreCase))
						{
							continue;
						}

						if (Key.Equals(TEXT("priority_id"), ESearchCase::IgnoreCase) && !Val.IsNumeric())
						{
							int32 MatchedPrioId = 0;
							for (const FWcwRedmineOptionItem& P : InRequest.KnownPriorities)
							{
								if (P.Id > 0 && P.Name.Equals(Val, ESearchCase::IgnoreCase))
								{
									MatchedPrioId = P.Id;
									break;
								}
							}
							if (MatchedPrioId <= 0)
							{
								for (const FWcwRedmineOptionItem& P : InRequest.KnownPriorities)
								{
									if (P.Id > 0 && P.Name.Contains(Val, ESearchCase::IgnoreCase))
									{
										MatchedPrioId = P.Id;
										break;
									}
								}
							}
							if (MatchedPrioId > 0)
							{
								IssueObj->SetNumberField(Key, MatchedPrioId);
							}
							continue;
						}

						if (Val.Equals(TEXT("true"), ESearchCase::IgnoreCase))
						{
							IssueObj->SetBoolField(Key, true);
						}
						else if (Val.Equals(TEXT("false"), ESearchCase::IgnoreCase))
						{
							IssueObj->SetBoolField(Key, false);
						}
						else if ((Key.EndsWith(TEXT("_id"), ESearchCase::IgnoreCase) ||
						          Key.Equals(TEXT("done_ratio"), ESearchCase::IgnoreCase)) &&
						         Val.IsNumeric())
						{
							IssueObj->SetNumberField(Key, FCString::Atoi(*Val));
						}
						else if (Key.Equals(TEXT("estimated_hours"), ESearchCase::IgnoreCase))
						{
							IssueObj->SetNumberField(Key, FCString::Atod(*Val));
						}
						else
						{
							IssueObj->SetStringField(Key, ExtraPair.Value);
						}
					}

					if (ResolvedCustomFields.Num() > 0)
					{
						TArray<TSharedPtr<FJsonValue>> CustomFieldsArr;
						for (const FWcwResolvedCustomFieldEntry& Entry : ResolvedCustomFields)
						{
							if (Entry.Id <= 0)
							{
								continue;
							}

							TSharedPtr<FJsonObject> CfObj = MakeShared<FJsonObject>();
							// Redmine REST API strictly requires numeric "id" (it ignores "name" on POST /issues.json)
							CfObj->SetNumberField(TEXT("id"), Entry.Id);

							if (Entry.bSendAsArray)
							{
								// Multiselect custom fields (`multiple=true`) require `"value": ["..."]` JSON array
								TArray<TSharedPtr<FJsonValue>> MultiValArr;
								TArray<FString> Segments;
								Entry.RawValue.ParseIntoArray(Segments, TEXT("|"), true);
								for (FString& Seg : Segments)
								{
									Seg.TrimStartAndEndInline();
									if (!Seg.IsEmpty())
									{
										MultiValArr.Add(MakeShared<FJsonValueString>(Seg));
									}
								}
								CfObj->SetArrayField(TEXT("value"), MultiValArr);
							}
							else
							{
								CfObj->SetStringField(TEXT("value"), Entry.RawValue);
							}

							CustomFieldsArr.Add(MakeShared<FJsonValueObject>(CfObj));
						}
						if (CustomFieldsArr.Num() > 0)
						{
							IssueObj->SetArrayField(TEXT("custom_fields"), CustomFieldsArr);
						}
					}

					TArray<TSharedPtr<FJsonValue>> UploadsArr;
					{
						TSharedPtr<FJsonObject> UpPng = MakeShared<FJsonObject>();
						UpPng->SetStringField(TEXT("token"), PngToken);
						UpPng->SetStringField(TEXT("filename"), PngFileName);
						UpPng->SetStringField(TEXT("content_type"), TEXT("image/png"));
						UploadsArr.Add(MakeShared<FJsonValueObject>(UpPng));

						TSharedPtr<FJsonObject> UpJson = MakeShared<FJsonObject>();
						UpJson->SetStringField(TEXT("token"), JsonToken);
						UpJson->SetStringField(TEXT("filename"), JsonFileName);
						UpJson->SetStringField(TEXT("content_type"), TEXT("application/json"));
						UploadsArr.Add(MakeShared<FJsonValueObject>(UpJson));
					}
					IssueObj->SetArrayField(TEXT("uploads"), UploadsArr);

					TSharedPtr<FJsonObject> RootPayload = MakeShared<FJsonObject>();
					RootPayload->SetObjectField(TEXT("issue"), IssueObj);

					FString PayloadJsonStr;
					TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PayloadJsonStr);
					FJsonSerializer::Serialize(RootPayload.ToSharedRef(), Writer);

					TSharedRef<IHttpRequest, ESPMode::ThreadSafe> CreateReq = FHttpModule::Get().CreateRequest();
					CreateReq->SetURL(FString::Printf(TEXT("%s/issues.json"), *BaseUrl));
					CreateReq->SetVerb(TEXT("POST"));
					CreateReq->SetHeader(TEXT("X-Redmine-API-Key"), ApiKey);
					CreateReq->SetHeader(TEXT("Content-Type"), TEXT("application/json; charset=utf-8"));
					CreateReq->SetHeader(TEXT("Accept"), TEXT("application/json"));
					CreateReq->SetContentAsString(PayloadJsonStr);

					CreateReq->OnProcessRequestComplete().BindLambda(
						[BaseUrl, OnComplete](FHttpRequestPtr CReq, FHttpResponsePtr CResp, bool bCreatedConn)
						{
							if (!bCreatedConn || !CResp.IsValid())
							{
								if (OnComplete)
								{
									OnComplete(false, 0, FString(), TEXT("Network error while posting /issues.json."));
								}
								return;
							}

							const int32 Code = CResp->GetResponseCode();
							if (Code < 200 || Code >= 300)
							{
								if (OnComplete)
								{
									OnComplete(false, 0, FString(), ExtractRedmineErrorDetail(Code, CResp->GetContentAsString()));
								}
								return;
							}

							int32 CreatedId = 0;
							TSharedPtr<FJsonObject> RespRoot;
							TSharedRef<TJsonReader<>> RespReader = TJsonReaderFactory<>::Create(CResp->GetContentAsString());
							if (FJsonSerializer::Deserialize(RespReader, RespRoot) && RespRoot.IsValid())
							{
								const TSharedPtr<FJsonObject>* CreatedIssueObj = nullptr;
								if (RespRoot->TryGetObjectField(TEXT("issue"), CreatedIssueObj) && CreatedIssueObj && CreatedIssueObj->IsValid())
								{
									(*CreatedIssueObj)->TryGetNumberField(TEXT("id"), CreatedId);
								}
							}

							const FString CreatedUrl = CreatedId > 0
								? FString::Printf(TEXT("%s/issues/%d"), *BaseUrl, CreatedId)
								: BaseUrl;

							if (OnComplete)
							{
								OnComplete(true, CreatedId, CreatedUrl, FString());
							}
						});

					CreateReq->ProcessRequest();
				});
		});
}

bool FWcwRedmineClient::ReadPostedIssueFromSidecar(
	const FString& InPngFilePath,
	int32& OutIssueId,
	FString& OutIssueUrl,
	FString& OutPostedAt)
{
	OutIssueId = 0;
	OutIssueUrl.Reset();
	OutPostedAt.Reset();

	if (InPngFilePath.IsEmpty())
	{
		return false;
	}

	FString ResolvedPngPath = InPngFilePath;
	if (FPaths::IsRelative(ResolvedPngPath))
	{
		ResolvedPngPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), ResolvedPngPath);
	}

	const FString SidecarPath = FPaths::ChangeExtension(ResolvedPngPath, TEXT("json"));
	FString JsonStr;
	if (!FPaths::FileExists(SidecarPath) || !FFileHelper::LoadFileToString(JsonStr, *SidecarPath))
	{
		return false;
	}

	TSharedPtr<FJsonObject> RootObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
	if (!FJsonSerializer::Deserialize(Reader, RootObj) || !RootObj.IsValid())
	{
		return false;
	}

	if (RootObj->TryGetNumberField(WcwRedmineJsonKeys::PostedIssueId, OutIssueId) && OutIssueId > 0)
	{
		RootObj->TryGetStringField(WcwRedmineJsonKeys::PostedIssueUrl, OutIssueUrl);
		RootObj->TryGetStringField(WcwRedmineJsonKeys::PostedAt, OutPostedAt);
		return true;
	}

	return false;
}

bool FWcwRedmineClient::WritePostedIssueToSidecar(
	const FString& InPngFilePath,
	const FWcwSnapJumpData& FallbackSnapData,
	int32 InIssueId,
	const FString& InIssueUrl)
{
	if (InPngFilePath.IsEmpty() || InIssueId <= 0)
	{
		return false;
	}

	FString ResolvedPngPath = InPngFilePath;
	if (FPaths::IsRelative(ResolvedPngPath))
	{
		ResolvedPngPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), ResolvedPngPath);
	}

	const FString SidecarPath = FPaths::ChangeExtension(ResolvedPngPath, TEXT("json"));

	TSharedPtr<FJsonObject> RootObj;
	FString ExistingJsonStr;
	if (FPaths::FileExists(SidecarPath) && FFileHelper::LoadFileToString(ExistingJsonStr, *SidecarPath))
	{
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ExistingJsonStr);
		FJsonSerializer::Deserialize(Reader, RootObj);
	}

	if (!RootObj.IsValid())
	{
		FString FallbackJson;
		if (FWcwSnapJumpSerializer::StructToJsonString(FallbackSnapData, FallbackJson, true))
		{
			TSharedRef<TJsonReader<>> FallbackReader = TJsonReaderFactory<>::Create(FallbackJson);
			FJsonSerializer::Deserialize(FallbackReader, RootObj);
		}
	}
	if (!RootObj.IsValid())
	{
		RootObj = MakeShared<FJsonObject>();
	}

	RootObj->SetNumberField(WcwRedmineJsonKeys::PostedIssueId, InIssueId);
	RootObj->SetStringField(WcwRedmineJsonKeys::PostedIssueUrl, InIssueUrl);
	RootObj->SetStringField(WcwRedmineJsonKeys::PostedAt, FDateTime::Now().ToIso8601());

	FString OutJsonStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJsonStr);
	if (!FJsonSerializer::Serialize(RootObj.ToSharedRef(), Writer))
	{
		return false;
	}

	return FFileHelper::SaveStringToFile(OutJsonStr, *SidecarPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

FString FWcwRedmineClient::BuildRedmineTelemetryBlock(const FWcwSnapJumpData& SnapData)
{
	return UWcwSnapJumpLibrary::FormatIssueTrackerTelemetryBlock(SnapData);
}
