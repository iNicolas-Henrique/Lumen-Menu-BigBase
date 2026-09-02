#include "Menu.hpp"

#include "core/commands/Commands.hpp"
#include "core/frontend/manager/UIManager.hpp"
#include "core/renderer/Renderer.hpp"
#include "game/backend/FiberPool.hpp"
#include "game/backend/ScriptMgr.hpp"
#include "game/frontend/fonts/Fonts.hpp"
#include "game/pointers/Pointers.hpp"
#include "game/rdr/Natives.hpp"
#include "game/rdr/invoker/Invoker.hpp"
#include "submenus/Debug.hpp"
#include "submenus/Network.hpp"
#include "submenus/Players.hpp"
#include "submenus/Recovery.hpp"
#include "submenus/Self.hpp"
#include "submenus/Settings.hpp"
#include "submenus/Teleport.hpp"
#include "submenus/World.hpp"

#include <Windows.h>
#include <algorithm>

namespace YimMenu
{
	static YimMenu::Submenus::Settings g_SettingsInstance;

	void Menu::Init()
	{
		g_SettingsInstance.LoadSettings();

		UIManager::AddSubmenu(std::make_shared<Submenus::Self>());
		UIManager::AddSubmenu(std::make_shared<Submenus::Teleport>());
		UIManager::AddSubmenu(std::make_shared<Submenus::Network>());
		UIManager::AddSubmenu(std::make_shared<Submenus::Players>());
		UIManager::AddSubmenu(std::make_shared<Submenus::World>());
		UIManager::AddSubmenu(std::make_shared<Submenus::Recovery>());
		UIManager::AddSubmenu(std::make_shared<Submenus::Settings>());
		UIManager::AddSubmenu(std::make_shared<Submenus::Debug>());

		Renderer::AddRendererCallBack(
		    [] {
			    if (!GUI::IsOpen())
				    return;
			    static bool runtimeInfoLoaded = false;
			    if (!runtimeInfoLoaded && NativeInvoker::AreHandlersCached())
			    {
				    const char* gameBuild = MISC::GET_GAME_VERSION_NAME();
				    UIManager::SetRuntimeInfo(gameBuild && *gameBuild ? gameBuild : "desconhecida", LUMEN_VERSION);
				    runtimeInfoLoaded = true;
			    }

			    ImGui::PushFont(Menu::Font::g_DefaultFont);
			    UIManager::Render();
			    ImGui::PopFont();
		    },
		    -1);
	}

	void Menu::SetupStyle()
	{
		ImGuiStyle& style = ImGui::GetStyle();

		style.WindowPadding = ImVec2(12.0f, 10.0f);
		style.FramePadding = ImVec2(10.0f, 7.0f);
		style.CellPadding = ImVec2(8.0f, 5.0f);
		style.ItemSpacing = ImVec2(8.0f, 7.0f);
		style.ItemInnerSpacing = ImVec2(7.0f, 5.0f);
		style.ScrollbarSize = 12.0f;

		const ImVec4 darkGreen(34.0f / 255.0f, 48.0f / 255.0f, 11.0f / 255.0f, 1.0f);
		const ImVec4 lightGreen(52.0f / 255.0f, 77.0f / 255.0f, 14.0f / 255.0f, 1.0f);
		const ImVec4 background(9.0f / 255.0f, 12.0f / 255.0f, 7.0f / 255.0f, 0.98f);
		style.Colors[ImGuiCol_WindowBg] = background;
		style.Colors[ImGuiCol_ChildBg] = ImVec4(14.0f / 255.0f, 18.0f / 255.0f, 10.0f / 255.0f, 0.96f);
		style.Colors[ImGuiCol_PopupBg] = background;
		style.Colors[ImGuiCol_Border] = lightGreen;
		style.Colors[ImGuiCol_FrameBg] = darkGreen;
		style.Colors[ImGuiCol_FrameBgHovered] = lightGreen;
		style.Colors[ImGuiCol_FrameBgActive] = lightGreen;
		style.Colors[ImGuiCol_Button] = darkGreen;
		style.Colors[ImGuiCol_ButtonHovered] = lightGreen;
		style.Colors[ImGuiCol_ButtonActive] = lightGreen;
		style.Colors[ImGuiCol_CheckMark] = ImVec4(0.82f, 0.92f, 0.60f, 1.0f);
		style.Colors[ImGuiCol_Header] = darkGreen;
		style.Colors[ImGuiCol_HeaderHovered] = lightGreen;
		style.Colors[ImGuiCol_HeaderActive] = lightGreen;
		style.Colors[ImGuiCol_SliderGrab] = lightGreen;
		style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.32f, 0.46f, 0.10f, 1.0f);

		style.GrabRounding = style.FrameRounding = style.ChildRounding = style.WindowRounding = 6.0f;
		style.PopupRounding = style.ScrollbarRounding = 6.0f;
		style.WindowBorderSize = 1.0f;
		style.ChildBorderSize = 1.0f;
	}

	void Menu::SetupFonts()
	{
		auto& IO = ImGui::GetIO();
		auto file_path = std::filesystem::path(std::getenv("appdata")) / "Lumen" / "imgui.ini";
		static auto path = file_path.string();
		IO.IniFilename = path.c_str();
		IO.LogFilename = NULL;
		ImFontConfig FontCfg{};
		FontCfg.FontDataOwnedByAtlas = false;

		Menu::Font::g_DefaultFont = IO.Fonts->AddFontFromMemoryTTF(const_cast<std::uint8_t*>(Fonts::MainFont), sizeof(Fonts::MainFont), 19.0f, &FontCfg);
		Menu::Font::g_OptionsFont = IO.Fonts->AddFontFromMemoryTTF(const_cast<std::uint8_t*>(Fonts::MainFont), sizeof(Fonts::MainFont), 19.0f, &FontCfg);
		Menu::Font::g_ChildTitleFont = IO.Fonts->AddFontFromMemoryTTF(const_cast<std::uint8_t*>(Fonts::MainFont), sizeof(Fonts::MainFont), 19.0f, &FontCfg);
		Menu::Font::g_ChatFont = IO.Fonts->AddFontFromMemoryTTF(const_cast<std::uint8_t*>(Fonts::MainFont), sizeof(Fonts::MainFont), 22.0f, &FontCfg);
		Menu::Font::g_OverlayFont = IO.Fonts->AddFontFromMemoryTTF(const_cast<std::uint8_t*>(Fonts::MainFont), sizeof(Fonts::MainFont), 16.0f, &FontCfg);

		UIManager::SetOptionsFont(Menu::Font::g_OptionsFont);
	}
}
