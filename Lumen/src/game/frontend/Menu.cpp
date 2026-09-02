#include "Menu.hpp"

#include "core/commands/Commands.hpp"
#include "core/frontend/manager/UIManager.hpp"
#include "core/frontend/theme/LumenTheme.hpp"
#include "core/renderer/Renderer.hpp"
#include "game/backend/FiberPool.hpp"
#include "game/backend/ScriptMgr.hpp"
#include "game/frontend/fonts/Fonts.hpp"
#include "game/pointers/Pointers.hpp"
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
		    [&] {
			    if (!GUI::IsOpen())
				    return;

			    ImGui::PushFont(Menu::Font::g_DefaultFont);
			    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.4f));

			    static bool firstFrame = true;
			    static int lastPosX = g_SettingsInstance.GetWindowPosX();
			    static int lastPosY = g_SettingsInstance.GetWindowPosY();
			    static int lastWidth = g_SettingsInstance.GetWindowWidth();
			    static int lastHeight = g_SettingsInstance.GetWindowHeight();
			    static bool lastMaximized = g_SettingsInstance.IsWindowMaximized();

			    int screenW = GetSystemMetrics(SM_CXSCREEN);
			    int screenH = GetSystemMetrics(SM_CYSCREEN);
			    const int minW = std::min(720, screenW), minH = std::min(480, screenH), maxW = screenW, maxH = screenH;
			    int posX = std::clamp(g_SettingsInstance.GetWindowPosX(), 0, screenW - minW);
			    int posY = std::clamp(g_SettingsInstance.GetWindowPosY(), 0, screenH - minH);
			    int width = std::clamp(g_SettingsInstance.GetWindowWidth(), minW, maxW);
			    int height = std::clamp(g_SettingsInstance.GetWindowHeight(), minH, maxH);

			    if (firstFrame)
			    {
				    ImGui::SetNextWindowPos(ImVec2((float)posX, (float)posY), ImGuiCond_Once);
				    ImGui::SetNextWindowSize(ImVec2((float)width, (float)height), ImGuiCond_Once);
			    }

			    ImGui::SetNextWindowSizeConstraints(ImVec2(static_cast<float>(minW), static_cast<float>(minH)), ImVec2(static_cast<float>(maxW), static_cast<float>(maxH)));
			    const bool visible = ImGui::Begin("##Lumen", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysUseWindowPadding | ImGuiWindowFlags_NoSavedSettings);
			    if (visible)
			    {
				    ImDrawList* background = ImGui::GetWindowDrawList();
				    const ImVec2 windowPos = ImGui::GetWindowPos();
				    const ImVec2 windowSize = ImGui::GetWindowSize();
				    LumenTheme::DrawWindowAtmosphere(background, windowPos, windowSize);

				    const ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
				    const ImVec2 headerCursor = ImGui::GetCursorScreenPos();
				    LumenTheme::DrawBrandMark(background, ImVec2(headerCursor.x + 14.0f, headerCursor.y + 14.0f), accent);
				    ImGui::Dummy(ImVec2(34.0f, 28.0f));
				    ImGui::SameLine();
				    ImGui::BeginGroup();
				    ImGui::TextColored(accent, "LUMEN");
				    ImGui::TextDisabled("Menu de utilidades para Red Dead Redemption 2");
				    ImGui::EndGroup();

				    const float terminateWidth = 92.0f;
				    ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - terminateWidth));
				    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.12f, 0.14f, 0.85f));
				    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.68f, 0.18f, 0.21f, 1.0f));
				    if (ImGui::Button("Encerrar", ImVec2(terminateWidth, 0)))
				    {
					    if (ScriptMgr::CanTick())
					    {
						    FiberPool::Push([] {
							    Commands::Shutdown();
							    g_Running = false;
						    });
					    }
					    else
					    {
						    g_Running = false;
					    }
				    }
				    ImGui::PopStyleColor(2);
				    ImGui::Separator();

				    UIManager::Draw();

				    ImVec2 winPos = ImGui::GetWindowPos();
				    ImVec2 winSize = ImGui::GetWindowSize();
				    bool maximized = false;

				    int curPosX = (int)winPos.x;
				    int curPosY = (int)winPos.y;
				    int curWidth = (int)winSize.x;
				    int curHeight = (int)winSize.y;

				    if (firstFrame || curPosX != lastPosX || curPosY != lastPosY || curWidth != lastWidth || curHeight != lastHeight || maximized != lastMaximized)
				    {
					    g_SettingsInstance.SaveIfWindowChanged(curWidth, curHeight, curPosX, curPosY, maximized);
					    lastPosX = curPosX;
					    lastPosY = curPosY;
					    lastWidth = curWidth;
					    lastHeight = curHeight;
					    lastMaximized = maximized;
				    }
				    firstFrame = false;
			    }
			    ImGui::End();

			    ImGui::PopStyleColor();
			    ImGui::PopFont();
		    },
		    -1);
	}

	void Menu::SetupStyle()
	{
		YimMenu::Submenus::ApplyMenuColors();
		LumenTheme::ApplyMetrics();
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
