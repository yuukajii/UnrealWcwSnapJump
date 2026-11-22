// Copyright (c) 2026 WinterCrownWORKS. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "WcwSnapJumpRedmineSettings.generated.h"

/**
 * Named parent-ticket preset selectable from the Wcw SnapJump Viewer Redmine tab.
 */
USTRUCT()
struct FWcwRedmineParentPreset
{
	GENERATED_BODY()

	/** Display name shown in the Parent Issue dropdown (e.g. "AutoPostTest_Root" or "Background Bugs") */
	UPROPERTY(Config, EditAnywhere, Category = "Redmine")
	FString Label;

	/** Numeric Redmine issue ID (e.g. 2 for #2) */
	UPROPERTY(Config, EditAnywhere, Category = "Redmine", meta = (ClampMin = "1"))
	int32 IssueId = 0;
};

/**
 * Team-shared Redmine connection & ticket defaults (saved to Config/DefaultEditor.ini, safe to commit to Git/Perforce).
 * Isolated inside WcwSnapJumpEditor so zero Editor/HTTP settings are cooked into packaged builds.
 */
UCLASS(Config = Editor, DefaultConfig, meta = (DisplayName = "Wcw Snap Jump (Redmine Project)"))
class WCWSNAPJUMPEDITOR_API UWcwSnapJumpRedmineSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UWcwSnapJumpRedmineSettings();

	//~ Begin UDeveloperSettings Interface
	virtual FName GetContainerName() const override { return FName(TEXT("Project")); }
	virtual FName GetCategoryName() const override { return FName(TEXT("Extension")); }
	//~ End UDeveloperSettings Interface

	/** Show the Redmine Ticket Post tab inside Wcw SnapJump Viewer */
	UPROPERTY(Config, EditAnywhere, Category = "Connection")
	bool bEnableRedmineIntegration = true;

	/**
	 * Convenience helper: Paste a full Redmine project URL here (e.g. "http://192.168.1.50:3000/projects/sandbox"
	 * or "https://redmine.example.com:8443/redmine/projects/my_game") to automatically fill RedmineBaseUrl and ProjectIdentifier.
	 */
	UPROPERTY(EditAnywhere, Transient, Category = "Connection", meta = (DisplayName = "Paste Full Project URL (Auto-Split)"))
	FString PasteProjectUrlHelper;

	/** Scheme + Host + Port (+ optional reverse-proxy subpath), e.g. "http://192.168.1.50:3000" */
	UPROPERTY(Config, EditAnywhere, Category = "Connection")
	FString RedmineBaseUrl;

	/** Redmine project identifier slug (from /projects/<identifier>) or numeric project ID */
	UPROPERTY(Config, EditAnywhere, Category = "Connection")
	FString ProjectIdentifier;

	/** Default tracker name to pre-select from dynamically fetched project trackers (defaults to "\u30D0\u30B0" / "Bug") */
	UPROPERTY(Config, EditAnywhere, Category = "Ticket Defaults")
	FString DefaultTrackerName;

	/** Optional preset list of parent tickets (root/umbrella issues) selectable when posting child tickets */
	UPROPERTY(Config, EditAnywhere, Category = "Ticket Defaults")
	TArray<FWcwRedmineParentPreset> ParentTicketPresets;

	/** Pre-fill the ticket subject input via UWcwSnapJumpHandler::FormatIssueTrackerSubject ("[CaptureTag] " by default) */
	UPROPERTY(Config, EditAnywhere, Category = "Ticket Defaults")
	bool bPrefixSubjectWithCaptureTag = true;

	/** Append UWcwSnapJumpHandler::FormatIssueTrackerTelemetryBlock to the ticket description */
	UPROPERTY(Config, EditAnywhere, Category = "Ticket Defaults")
	bool bAppendTelemetrySummaryToDescription = true;

	/**
	 * Optional map of Redmine Custom Field Name (e.g. "CustomFieldTest") or numeric ID string (e.g. "1") -> Default String Value.
	 * Field names are automatically resolved to numeric Redmine Custom Field IDs via GET /projects/<id>.json?include=issue_custom_fields.
	 * Can also be populated dynamically per snapshot by overriding UWcwSnapJumpHandler::GatherIssueTrackerCustomFields.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Custom Fields")
	TMap<FString, FString> DefaultCustomFields;

	/**
	 * Optional map of standard Redmine `issue` JSON fields -> Default Value (e.g. "priority_id" -> "4", "fixed_version_id" -> "2", "is_private" -> "true").
	 * Can also be populated dynamically per snapshot by overriding UWcwSnapJumpHandler::GatherIssueTrackerIssueFields.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Custom Fields")
	TMap<FString, FString> DefaultExtraIssueFields;

	/** Returns RedmineBaseUrl trimmed of whitespace and trailing slashes */
	FString GetNormalizedBaseUrl() const;

	/** Parses a full "/projects/<slug>" URL into OutBaseUrl and OutProjectId; returns true if "/projects/" was found */
	static bool ParseRedmineProjectUrl(const FString& InFullUrl, FString& OutBaseUrl, FString& OutProjectId);

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

/**
 * Per-developer local Redmine credentials & last-used UI selections.
 * Stored strictly in Saved/Config/<Platform>Editor/EditorPerProjectUserSettings.ini (NO DefaultConfig -> never written to Config/).
 */
UCLASS(Config = EditorPerProjectUserSettings, meta = (DisplayName = "Wcw Snap Jump (Redmine User Auth)"))
class WCWSNAPJUMPEDITOR_API UWcwSnapJumpRedmineUserSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	//~ Begin UDeveloperSettings Interface
	virtual FName GetContainerName() const override { return FName(TEXT("Editor")); }
	virtual FName GetCategoryName() const override { return FName(TEXT("Extension")); }
	//~ End UDeveloperSettings Interface

	/**
	 * Personal Redmine API Access Key from Redmine "/my/account" (right-hand pane -> API access key).
	 * Sent via X-Redmine-API-Key header (works seamlessly even when web 2FA is enabled).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Authentication", meta = (PasswordField = true))
	FString ApiAccessKey;

	/** Last selected Redmine Tracker ID remembered locally per user */
	UPROPERTY(Config)
	int32 LastSelectedTrackerId = 0;

	/** Last selected Redmine Issue Category ID remembered locally per user (0 = None) */
	UPROPERTY(Config)
	int32 LastSelectedCategoryId = 0;

	/** Last selected Redmine Assignee User/Group ID remembered locally per user (0 = Unassigned) */
	UPROPERTY(Config)
	int32 LastSelectedAssigneeId = 0;

	/** Last used Parent Issue ID remembered locally per user (0 = No Parent) */
	UPROPERTY(Config)
	int32 LastUsedParentIssueId = 0;

	/** Saves personal settings strictly to Saved/Config/<Platform>Editor/EditorPerProjectUserSettings.ini */
	void SaveUserConfig();
};
