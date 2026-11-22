// Copyright (c) 2026 WinterCrownWORKS. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "WcwSnapJumpTypes.h"

/**
 * Rich metadata option item for Redmine Trackers, Issue Categories, Assignees, Priorities, Custom Fields, and Parent Presets.
 */
struct FWcwRedmineOptionItem
{
	int32 Id = 0;
	FString Name;

	/** True when Redmine marks this enumeration item as `"is_default": true` (e.g. default Priority) */
	bool bIsDefault = false;

	/** True when Redmine marks a Custom Field as `"multiple": true` (requires JSON array `"value": [...]`) */
	bool bMultiple = false;

	/** Redmine custom field format (e.g. "string", "text", "list", "bool", "int") when available */
	FString FieldFormat;

	/** Redmine custom field default value when available */
	FString DefaultValue;

	/** Selectable list options fetched from Redmine `/custom_fields.json` when available */
	TArray<FWcwIssueTrackerUiOption> PossibleValues;

	FWcwRedmineOptionItem() = default;
	FWcwRedmineOptionItem(int32 InId, const FString& InName, bool bInIsDefault = false)
		: Id(InId)
		, Name(InName)
		, bIsDefault(bInIsDefault)
	{
	}
};

/**
 * Resolved numeric Redmine Custom Field payload entry ready for JSON serialization in POST /issues.json.
 */
struct FWcwResolvedCustomFieldEntry
{
	int32 Id = 0;
	FString RawValue;
	bool bSendAsArray = false;
};

/**
 * Project metadata fetched dynamically from:
 *   - GET /users/current.json
 *   - GET /projects/<id>.json?include=trackers,issue_categories,issue_custom_fields
 *   - GET /projects/<id>/memberships.json?limit=100
 *   - GET /enumerations/issue_priorities.json
 *   - GET /custom_fields.json (with non-admin /issues.json fallback for `multiple` detection)
 */
struct FWcwRedmineProjectMeta
{
	int32 NumericProjectId = 0;
	FString ProjectName;
	int32 CurrentUserId = 0;
	FString CurrentUserDisplayName;
	int32 DefaultPriorityId = 0;
	TArray<FWcwRedmineOptionItem> Trackers;
	TArray<FWcwRedmineOptionItem> Categories;
	TArray<FWcwRedmineOptionItem> Assignees;
	TArray<FWcwRedmineOptionItem> Priorities;
	TArray<FWcwRedmineOptionItem> IssueCustomFields;
};

/**
 * Complete ticket creation payload passed to FWcwRedmineClient::PostIssueWithAttachmentsAsync.
 */
struct FWcwRedmineTicketRequest
{
	FString BaseUrl;
	FString ProjectIdentifier;
	FString ApiKey;

	int32 TrackerId = 0;
	int32 CategoryId = 0;
	int32 AssignedToId = 0;
	int32 ParentIssueId = 0;

	FString Subject;
	FString Description;

	/**
	 * Optional standard Redmine `issue` JSON fields (e.g. "priority_id" -> "3", "fixed_version_id" -> "2", "is_private" -> "true")
	 * merged from Settings::DefaultExtraIssueFields + Handler::GatherIssueTrackerIssueFields + Interactive Title UI Controls.
	 */
	TMap<FString, FString> ExtraIssueFields;

	/**
	 * Redmine Custom Field Name (e.g. "CustomFieldTest", "CustomFieldTest_List") or numeric ID string -> Value string
	 * merged from Settings::DefaultCustomFields + Handler::GatherIssueTrackerCustomFields + Interactive Title UI Controls.
	 */
	TMap<FString, FString> CustomFields;

	/** Explicit Custom Field keys flagged by Title UI controls to serialize `"value"` as a JSON array */
	TSet<FString> MultiValueCustomFieldKeys;

	/** Cached project custom field definitions (Id, Name, bMultiple, PossibleValues) */
	TArray<FWcwRedmineOptionItem> KnownCustomFields;

	/** Cached Redmine priority definitions used to resolve priority names to numeric priority_id if needed */
	TArray<FWcwRedmineOptionItem> KnownPriorities;

	/** Path to the snapshot .png file on disk */
	FString PngFilePath;

	/** Snapshot data used to synthesize <BaseName>.json bytes if a sidecar file is not yet on disk */
	FWcwSnapJumpData SnapData;
};

/**
 * Editor-only Redmine REST API client & local .json sidecar posted-ticket tracker.
 * Uses Unreal's built-in HTTP / Json modules strictly inside WcwSnapJumpEditor (zero runtime footprint).
 */
class WCWSNAPJUMPEDITOR_API FWcwRedmineClient
{
public:
	using FOnVerifyAndFetchComplete = TFunction<void(bool /*bSuccess*/, const FWcwRedmineProjectMeta& /*OutMeta*/, const FString& /*ErrorMessage*/)>;
	using FOnVerifyParentComplete = TFunction<void(bool /*bSuccess*/, int32 /*IssueId*/, const FString& /*IssueSummary*/, const FString& /*ErrorMessage*/)>;
	using FOnPostIssueComplete = TFunction<void(bool /*bSuccess*/, int32 /*CreatedIssueId*/, const FString& /*CreatedIssueUrl*/, const FString& /*ErrorMessage*/)>;

	/**
	 * Authenticates via GET /users/current.json (X-Redmine-API-Key) and fetches project-specific
	 * Trackers, Issue Categories, Priorities, Issue Custom Fields (including list options & multiple flag), and Assignees.
	 */
	static void VerifyAndFetchProjectMetaAsync(
		const FString& InBaseUrl,
		const FString& InProjectIdentifier,
		const FString& InApiKey,
		FOnVerifyAndFetchComplete OnComplete);

	/**
	 * Checks an individual parent issue ID via GET /issues/<id>.json so the Editor UI can preview its subject.
	 */
	static void VerifyParentIssueAsync(
		const FString& InBaseUrl,
		const FString& InApiKey,
		int32 InIssueId,
		FOnVerifyParentComplete OnComplete);

	/**
	 * Resolves custom_fields names to numeric Redmine IDs (fetching /projects/<id>.json?include=issue_custom_fields on demand if needed),
	 * uploads <BaseName>.png and <BaseName>.json via POST /uploads.json, and creates the issue via POST /issues.json.
	 */
	static void PostIssueWithAttachmentsAsync(
		const FWcwRedmineTicketRequest& InRequest,
		FOnPostIssueComplete OnComplete);

	/**
	 * Reads posted Redmine ticket metadata ("redminePostedIssueId", "redminePostedIssueUrl", "redminePostedAt")
	 * from the local <BaseName>.json sidecar file next to InPngFilePath.
	 */
	static bool ReadPostedIssueFromSidecar(
		const FString& InPngFilePath,
		int32& OutIssueId,
		FString& OutIssueUrl,
		FString& OutPostedAt);

	/**
	 * Writes posted Redmine ticket metadata non-destructively into <BaseName>.json next to InPngFilePath
	 * (creating the sidecar from FallbackSnapData if it did not exist yet).
	 */
	static bool WritePostedIssueToSidecar(
		const FString& InPngFilePath,
		const FWcwSnapJumpData& FallbackSnapData,
		int32 InIssueId,
		const FString& InIssueUrl);

	/** Delegates to UWcwSnapJumpLibrary::FormatIssueTrackerTelemetryBlock so Titles can override the <pre> block */
	static FString BuildRedmineTelemetryBlock(const FWcwSnapJumpData& SnapData);

private:
	static FString NormalizeBaseUrl(const FString& InUrl);
	static FString ExtractRedmineErrorDetail(int32 HttpCode, const FString& ResponseBody);

	static void EnrichCustomFieldsMetadataAsync(
		const FString& BaseUrl,
		const FString& EncodedProj,
		const FString& ApiKey,
		bool bProjectFilteredList,
		TArray<FWcwRedmineOptionItem>&& InitialDefs,
		TFunction<void(TArray<FWcwRedmineOptionItem>&& /*EnrichedDefs*/)> OnComplete);

	static void ResolveCustomFieldsToNumericIds(
		const TMap<FString, FString>& RawCustomFields,
		const TSet<FString>& MultiValueKeys,
		const TArray<FWcwRedmineOptionItem>& KnownDefs,
		TArray<FWcwResolvedCustomFieldEntry>& OutResolvedEntries,
		TArray<FString>& OutUnresolvedKeys);

	static void FetchCustomFieldDefsOnDemandAsync(
		const FString& BaseUrl,
		const FString& ProjectId,
		const FString& ApiKey,
		TFunction<void(const TArray<FWcwRedmineOptionItem>& /*FetchedDefs*/)> OnComplete);

	static void UploadAndCreateIssueInternalAsync(
		const FWcwRedmineTicketRequest& InRequest,
		const FString& BaseUrl,
		const FString& ApiKey,
		const TArray<FWcwResolvedCustomFieldEntry>& ResolvedCustomFields,
		FOnPostIssueComplete OnComplete);

	static void UploadSingleBinaryAsync(
		const FString& BaseUrl,
		const FString& ApiKey,
		const FString& FileName,
		TArray<uint8>&& FileBytes,
		TFunction<void(bool /*bSuccess*/, const FString& /*UploadToken*/, const FString& /*ErrorMessage*/)> OnComplete);
};
