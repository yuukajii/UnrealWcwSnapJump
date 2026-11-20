// Copyright (c) 2026 WinterCrownWORKS. All Rights Reserved.
// Licensed under the MIT License.

#include "WcwSnapJumpEditorModule.h"
#include "SWcwSnapJumpEditorPanel.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "ToolMenus.h"

static const FName WcwSnapJumpTabName("WcwSnapJumpViewer");

#define LOCTEXT_NAMESPACE "FWcwSnapJumpEditorModule"

void FWcwSnapJumpEditorModule::StartupModule()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		WcwSnapJumpTabName,
		FOnSpawnTab::CreateRaw(this, &FWcwSnapJumpEditorModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("WcwSnapJumpTabTitle", "Wcw SnapJump Viewer"))
		.SetTooltipText(LOCTEXT("WcwSnapJumpTooltipText", "Inspect embedded performance stats, GPU Top 10 passes, custom event/cutscene state, and jump from Wcw snapshots."))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsMiscCategory());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FWcwSnapJumpEditorModule::RegisterMenus));
}

void FWcwSnapJumpEditorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(WcwSnapJumpTabName);
}

TSharedRef<SDockTab> FWcwSnapJumpEditorModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SWcwSnapJumpEditorPanel)
		];
}

void FWcwSnapJumpEditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);
	if (UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window"))
	{
		FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
		Section.AddMenuEntry(
			"OpenWcwSnapJumpViewer",
			LOCTEXT("OpenWcwSnapJumpViewerLabel", "Wcw SnapJump Viewer"),
			LOCTEXT("OpenWcwSnapJumpViewerTooltip", "Open the Wcw SnapJump visual snapshot & telemetry viewer tab."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([]()
			{
				FGlobalTabmanager::Get()->TryInvokeTab(WcwSnapJumpTabName);
			}))
		);
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FWcwSnapJumpEditorModule, WcwSnapJumpEditor)
