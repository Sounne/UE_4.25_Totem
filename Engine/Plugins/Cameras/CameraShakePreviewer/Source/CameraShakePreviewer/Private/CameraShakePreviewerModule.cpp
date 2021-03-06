// Copyright Epic Games, Inc. All Rights Reserved.

#include "CameraShakePreviewerModule.h"
#include "Framework/Commands/Commands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "ILevelEditor.h"
#include "ILevelViewport.h"
#include "LevelEditor.h"
#include "LevelEditorViewport.h"
#include "Modules/ModuleManager.h"
#include "SCameraShakePreviewer.h"
#include "ToolMenus.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SWidget.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "IAssetViewport.h"
#include "EditorViewportClient.h"

#define LOCTEXT_NAMESPACE "CameraShakePreviewer"

IMPLEMENT_MODULE(FCameraShakePreviewerModule, CameraShakePreviewer);

static const FName LevelEditorModuleName("LevelEditor");
static const FName LevelEditorCameraShakePreviewerTab("CameraShakePreviewer");

/**
 * Editor commands for the camera shake preview tool.
 */
class FCameraShakePreviewerCommands : public TCommands<FCameraShakePreviewerCommands>
{
public:
	FCameraShakePreviewerCommands()
		: TCommands<FCameraShakePreviewerCommands>(
				TEXT("CameraShakePreviewer"),
				LOCTEXT("CameraShakePreviewerContextDescription", "Camera Shake Previewer"),
				TEXT("EditorViewport"),
				FEditorStyle::GetStyleSetName())
	{
	}

	static FCameraShakePreviewerCommands& Get()
	{
		return *(Instance.Pin());
	}

	virtual void RegisterCommands() override
	{
		UI_COMMAND(ToggleCameraShakesPreview, "Allow Camera Shakes", "If enabled, allows the camera shakes previewer panel to apply shakes to this viewport", EUserInterfaceActionType::ToggleButton, FInputChord());
	}

	TSharedPtr<FUICommandInfo> ToggleCameraShakesPreview;
};


void FCameraShakePreviewerModule::StartupModule()
{
	FCameraShakePreviewerCommands::Register();

	if (ensure(FModuleManager::Get().IsModuleLoaded(LevelEditorModuleName)))
	{
		RegisterEditorTab();
		RegisterViewportOptionMenuExtender();
	}
}

void FCameraShakePreviewerModule::ShutdownModule()
{
	UnregisterViewportOptionMenuExtender();
	UnregisterEditorTab();

	FCameraShakePreviewerCommands::Unregister();
}

void FCameraShakePreviewerModule::RegisterEditorTab()
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(LevelEditorModuleName);

	LevelEditorTabManagerChangedHandle = LevelEditorModule.OnTabManagerChanged().AddLambda([]()
	{
		// Add a new entry in the level editor's "Window" menu, which lets the user open the camera shake preview tool.
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(LevelEditorModuleName);
		TSharedPtr<FTabManager> LevelEditorTabManager = LevelEditorModule.GetLevelEditorTabManager();

		const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();
		const FSlateIcon Icon(FEditorStyle::GetStyleSetName(), "LevelViewport.ToggleActorPilotCameraView");

		LevelEditorTabManager->RegisterTabSpawner("CameraShakePreviewer", FOnSpawnTab::CreateStatic(&FCameraShakePreviewerModule::CreateCameraShakePreviewerTab))
			.SetDisplayName(LOCTEXT("CameraShakePreviewer", "Camera Shake Previewer"))
			.SetTooltipText(LOCTEXT("CameraShakePreviewerTooltipText", "Open the camera shake preview panel."))
			.SetIcon(Icon)
			.SetGroup(MenuStructure.GetLevelEditorCategory());
	});
}

void FCameraShakePreviewerModule::UnregisterEditorTab()
{
	if (LevelEditorTabManagerChangedHandle.IsValid())
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(LevelEditorModuleName);
		LevelEditorModule.OnTabManagerChanged().Remove(LevelEditorTabManagerChangedHandle);
	}
}

void FCameraShakePreviewerModule::RegisterViewportOptionMenuExtender()
{
	// Register a callback for adding a "Show Camera Shakes" option in the viewport options menu.
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(LevelEditorModuleName);

	FLevelEditorModule::FLevelEditorMenuExtender Extender = FLevelEditorModule::FLevelEditorMenuExtender::CreateRaw(this, &FCameraShakePreviewerModule::OnExtendLevelViewportOptionMenu);
	LevelEditorModule.GetAllLevelViewportOptionsMenuExtenders().Add(Extender);
	ViewportOptionsMenuExtenderHandle = LevelEditorModule.GetAllLevelViewportOptionsMenuExtenders().Last().GetHandle();

	GEditor->OnLevelViewportClientListChanged().AddRaw(this, &FCameraShakePreviewerModule::OnLevelViewportClientListChanged);

	OnLevelViewportClientListChanged();
}

void FCameraShakePreviewerModule::UnregisterViewportOptionMenuExtender()
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(LevelEditorModuleName);
	LevelEditorModule.GetAllLevelViewportOptionsMenuExtenders().RemoveAll([=](const FLevelEditorModule::FLevelEditorMenuExtender& Extender) { return Extender.GetHandle() == ViewportOptionsMenuExtenderHandle; });

	if (GEditor)
	{
		GEditor->OnLevelViewportClientListChanged().RemoveAll(this);
	}
}

TSharedRef<FExtender> FCameraShakePreviewerModule::OnExtendLevelViewportOptionMenu(const TSharedRef<FUICommandList> CommandList)
{
	// Find the viewport for which we're opening this options menu.
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(LevelEditorModuleName);
	TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetLevelEditorInstance().Pin();
	TSharedPtr<IAssetViewport> ViewportInterface = LevelEditor->GetActiveViewportInterface();
	FLevelEditorViewportClient* ViewportClient = (FLevelEditorViewportClient*)&ViewportInterface->GetAssetViewportClient();
	if (ViewportClient->ViewportType != ELevelViewportType::LVT_Perspective)
	{
		// If the current active viewport isn't perspective, let's not add any option for previewing camera shakes.
		return MakeShared<FExtender>();
	}

	// Bind the "toggle camera shakes preview" action to some callbacks that will set the flag on the appropriate viewport.
	FUIAction ToggleCameraShakesPreviewAction;
	ToggleCameraShakesPreviewAction.ExecuteAction.BindLambda([this, ViewportClient]()
	{
		ToggleCameraShakesPreview(ViewportClient);
	});
	ToggleCameraShakesPreviewAction.GetActionCheckState.BindLambda([this, ViewportClient]() -> ECheckBoxState
	{
		return HasCameraShakesPreview(ViewportClient) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	});

	// Make the extender that adds the menu entry.
	TSharedRef<FExtender> Extender = MakeShared<FExtender>();
	Extender->AddMenuExtension(
		"LevelViewportViewportOptions2",
		EExtensionHook::After,
		nullptr,
		FMenuExtensionDelegate::CreateLambda([ToggleCameraShakesPreviewAction](FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("AllowCameraShakes", "Allow Camera Shakes"),
				LOCTEXT("AllowCameraShakesTooltip", "If enabled, allows the camera shakes previewer panel to apply shakes to this viewport"),
				FSlateIcon(),
				ToggleCameraShakesPreviewAction,
				NAME_None,
				EUserInterfaceActionType::ToggleButton);
		})
	);
	return Extender;
}

void FCameraShakePreviewerModule::OnLevelViewportClientListChanged()
{
	const TArray<FLevelEditorViewportClient*> LevelViewportClients = GEditor->GetLevelViewportClients();
	// Remove viewports that don't exist anymore.
	for (auto It = ViewportInfos.CreateIterator(); It; ++It)
	{
		if (!LevelViewportClients.Contains(It.Key()))
		{
			ViewportInfos.Remove(It.Key());
		}
	}
	// Add recently added viewports that we don't know about yet.
	for (FLevelEditorViewportClient* LevelViewportClient : LevelViewportClients)
	{
		if (!ViewportInfos.Contains(LevelViewportClient))
		{
			ViewportInfos.Add(LevelViewportClient, FViewportInfo{ false });
		}
	}
}

void FCameraShakePreviewerModule::ToggleCameraShakesPreview(FLevelEditorViewportClient* ViewportClient)
{
	if (FViewportInfo* ViewportInfo = ViewportInfos.Find(ViewportClient))
	{
		ViewportInfo->bPreviewCameraShakes = !ViewportInfo->bPreviewCameraShakes;
		OnTogglePreviewCameraShakes.Broadcast(FTogglePreviewCameraShakesParams{ ViewportClient, ViewportInfo->bPreviewCameraShakes });
	}
}

bool FCameraShakePreviewerModule::HasCameraShakesPreview(FLevelEditorViewportClient* ViewportClient) const
{
	const FViewportInfo* ViewportInfo = ViewportInfos.Find(ViewportClient);
	return ViewportInfo != nullptr && ViewportInfo->bPreviewCameraShakes;
}

TSharedRef<SDockTab> FCameraShakePreviewerModule::CreateCameraShakePreviewerTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::PanelTab)
		[
			SNew(SCameraShakePreviewer)
		];
}

#undef LOCTEXT_NAMESPACE
