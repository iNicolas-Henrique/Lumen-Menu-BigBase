#include "Menu.hpp"

#include "core/commands/Commands.hpp"
#include "core/frontend/manager/UIManager.hpp"
#include "core/renderer/Renderer.hpp"
#include "game/backend/FiberPool.hpp"
#include "game/backend/ScriptMgr.hpp"
#include "game/frontend/fonts/Fonts.hpp"
#include "game/pointers/Pointers.hpp"
#include "submenus/Debug.hpp"
#include "submenus/Network.hpp"
#include "submenus/Players.hpp"
#include "submenus/Self.hpp"
#include "submenus/Settings.hpp"
#include "submenus/Teleport.hpp"
#include "submenus/World.hpp"
#include "submenus/Recovery.hpp"

#include <algorithm>
#include <Windows.h>

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
				ImGui::PushStyleColor(ImGuiCol_WindowBg, ImU32(ImColor(0, 0, 0, 100)));

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

				if (firstFrame) {
					ImGui::SetNextWindowPos(ImVec2((float)posX, (float)posY), ImGuiCond_Once);
					ImGui::SetNextWindowSize(ImVec2((float)width, (float)height), ImGuiCond_Once);
				}

				const bool visible = ImGui::Begin("##Lumen", nullptr,
					ImGuiWindowFlags_NoTitleBar |
					ImGuiWindowFlags_NoCollapse |
					ImGuiWindowFlags_AlwaysUseWindowPadding |
					ImGuiWindowFlags_NoSavedSettings);
				if (visible)
				{
					// Renderer-neutral translucent backdrop. It deliberately uses ImGui's
					// draw list so the same Lumen artwork works on Vulkan and DirectX 12.
					ImDrawList* background = ImGui::GetWindowDrawList();
					const ImVec2 windowPos = ImGui::GetWindowPos();
					const ImVec2 windowSize = ImGui::GetWindowSize();
					const ImVec2 windowEnd(windowPos.x + windowSize.x, windowPos.y + windowSize.y);
					background->AddRectFilledMultiColor(windowPos,
					    windowEnd,
					    IM_COL32(8, 12, 18, 225),
					    IM_COL32(24, 18, 10, 218),
					    IM_COL32(9, 18, 20, 225),
					    IM_COL32(7, 10, 15, 232));
					background->AddCircleFilled(ImVec2(windowEnd.x - 45.0f, windowPos.y + 35.0f), 145.0f, IM_COL32(226, 174, 76, 18), 48);
					background->AddCircleFilled(ImVec2(windowPos.x + 25.0f, windowEnd.y - 15.0f), 180.0f, IM_COL32(58, 183, 178, 13), 48);
					background->AddText(ImVec2(windowEnd.x - 168.0f, windowEnd.y - 42.0f), IM_COL32(255, 255, 255, 16), "L U M E N");

					const ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
					ImGui::TextColored(accent, "LUMEN");
					ImGui::SameLine();
					ImGui::TextDisabled("Lumen");

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

					if (firstFrame ||
						curPosX != lastPosX || curPosY != lastPosY ||
						curWidth != lastWidth || curHeight != lastHeight ||
						maximized != lastMaximized)
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
		ImGuiStyle& style = ImGui::GetStyle();

		style.WindowPadding    = ImVec2(12.0f, 10.0f);
		style.FramePadding     = ImVec2(10.0f, 7.0f);
		style.CellPadding      = ImVec2(8.0f, 5.0f);
		style.ItemSpacing      = ImVec2(8.0f, 7.0f);
		style.ItemInnerSpacing = ImVec2(7.0f, 5.0f);
		style.ScrollbarSize    = 12.0f;

		YimMenu::Submenus::ApplyMenuColors();

		style.GrabRounding = style.FrameRounding = style.ChildRounding = style.WindowRounding = 6.0f;
		style.PopupRounding = style.ScrollbarRounding = 6.0f;
		style.WindowBorderSize = 1.0f;
		style.ChildBorderSize = 1.0f;
	}

	void Menu::SetupFonts()
	{
		auto& IO       = ImGui::GetIO();
		auto file_path = std::filesystem::path(std::getenv("appdata")) / "Lumen" / "imgui.ini";
		static auto path = file_path.string();
		IO.IniFilename   = path.c_str();
		IO.LogFilename   = NULL;
		ImFontConfig FontCfg{};
		FontCfg.FontDataOwnedByAtlas = false;

		Menu::Font::g_DefaultFont = IO.Fonts->AddFontFromMemoryTTF(
			const_cast<std::uint8_t*>(Fonts::MainFont), sizeof(Fonts::MainFont), 19.0f, &FontCfg);
		Menu::Font::g_OptionsFont = IO.Fonts->AddFontFromMemoryTTF(
			const_cast<std::uint8_t*>(Fonts::MainFont), sizeof(Fonts::MainFont), 19.0f, &FontCfg);
		Menu::Font::g_ChildTitleFont = IO.Fonts->AddFontFromMemoryTTF(
			const_cast<std::uint8_t*>(Fonts::MainFont), sizeof(Fonts::MainFont), 19.0f, &FontCfg);
		Menu::Font::g_ChatFont = IO.Fonts->AddFontFromMemoryTTF(
			const_cast<std::uint8_t*>(Fonts::MainFont), sizeof(Fonts::MainFont), 22.0f, &FontCfg);
		Menu::Font::g_OverlayFont = IO.Fonts->AddFontFromMemoryTTF(
			const_cast<std::uint8_t*>(Fonts::MainFont), sizeof(Fonts::MainFont), 16.0f, &FontCfg);

		UIManager::SetOptionsFont(Menu::Font::g_OptionsFont);
	}
}
