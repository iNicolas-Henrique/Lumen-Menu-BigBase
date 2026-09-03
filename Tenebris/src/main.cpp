#include "common.hpp"
#include "core/commands/HotkeySystem.hpp"
#include "core/filemgr/FileMgr.hpp"
#include "core/frontend/Notifications.hpp"
#include "core/hooking/Hooking.hpp"
#include "core/memory/ModuleMgr.hpp"
#include "game/backend/PlayerDatabase.hpp"
#include "core/renderer/Renderer.hpp"
#include "core/settings/Settings.hpp"
#include "game/backend/FiberPool.hpp"
#include "game/backend/MapEditor/MapEditor.hpp"
#include "game/backend/ScriptMgr.hpp"
#include "game/backend/NativeHooks.hpp"
#include "game/backend/SavedLocations.hpp"
#include "game/features/Features.hpp"
#include "game/frontend/GUI.hpp"
#include "game/pointers/Pointers.hpp"


namespace YimMenu
{
	static DWORD Main(void*)
	{
		const auto documents = std::filesystem::path(std::getenv("appdata")) / "Tenebris";
		FileMgr::Init(documents);

		LogHelper::Init("Tenebris", FileMgr::GetProjectFile("./cout.log"));
		LOG(INFO) << R"(
 _______ ______ _   _ ______ ____  _____  _____ _____
|__   __|  ____| \ | |  ____|  _ \|  __ \|_   _/ ____|
   | |  | |__  |  \| | |__  | |_) | |__) | | || (___
   | |  |  __| | . ` |  __| |  _ <|  _  /  | | \___ \
   | |  | |____| |\  | |____| |_) | | \ \ _| |_ ____) |
   |_|  |______|_| \_|______|____/|_|  \_\_____|_____/
                T E N E B R I S
)";

		g_HotkeySystem.RegisterCommands();
		SavedLocations::FetchSavedLocations();
		Settings::Initialize(FileMgr::GetProjectFile("./settings.json"));

		auto PlayerDatabaseInstance = std::make_unique<PlayerDatabase>();

		if (!ModuleMgr.LoadModules())
			goto unload;
		if (!Pointers.Init())
			goto unload;

		Hooking::Init();

		if (!Renderer::Init())
			goto unload;

		if (!Pointers.LateInit())
			LOG(WARNING) << "Nao foi possivel localizar alguns ponteiros";
		Hooking::LateInit();

		ScriptMgr::Init();
		LOG(INFO) << "Gerenciador de scripts inicializado";

		FiberPool::Init(5);
		LOG(INFO) << "Pool de fibers inicializado";

		GUI::Init();

		ScriptMgr::AddScript(std::make_unique<Script>(&FeatureLoop));
		ScriptMgr::AddScript(std::make_unique<Script>(&BlockControlsForUI));
		ScriptMgr::AddScript(std::make_unique<Script>(&ContextMenuTick));
		ScriptMgr::AddScript(std::make_unique<Script>(&MapEditor::Update));

		Notifications::Show("Tenebris", "Carregado com sucesso", NotificationType::Success);

#ifndef NDEBUG
		LOG(WARNING) << "Build de depuracao. Use RelWithDebInfo ou Release para maior estabilidade.";
#endif

		while (g_Running)
		{
			Settings::Tick(); // TODO: move this somewhere else
		}

		LOG(INFO) << "Descarregando";

		NativeHooks::Destroy();
		LOG(INFO) << "Hooks de natives finalizados";

		ScriptMgr::Destroy();
		LOG(INFO) << "Gerenciador de scripts finalizado";

		FiberPool::Destroy();
		LOG(INFO) << "Pool de fibers finalizado";

		PlayerDatabaseInstance.reset();

	unload:
		Hooking::Destroy();
		LOG(INFO) << "Hooks finalizados";
		Renderer::Destroy();
		LOG(INFO) << "Renderizador finalizado";

		LOG(INFO) << "Ate logo!";
		LogHelper::Destroy();

		FreeLibraryAndExitThread(g_DllInstance, EXIT_SUCCESS);
		return EXIT_SUCCESS;
	}
}

BOOL WINAPI DllMain(HINSTANCE dllInstance, DWORD reason, void*)
{
	using namespace YimMenu;

	DisableThreadLibraryCalls(dllInstance);

	if (reason == DLL_PROCESS_ATTACH)
	{
		g_DllInstance = dllInstance;

		g_MainThread = CreateThread(nullptr, 0, Main, nullptr, 0, &g_MainThreadId);
	}
	return true;
}
