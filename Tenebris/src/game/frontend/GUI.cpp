#include "GUI.hpp"

#include "ContextMenu.hpp"
#include "ESP.hpp"
#include "Menu.hpp"
#include "Overlay.hpp"
#include "core/commands/BoolCommand.hpp"
#include "core/commands/Commands.hpp"
#include "core/frontend/Notifications.hpp"
#include "core/frontend/manager/AdvancedEditor.hpp"
#include "core/frontend/manager/UIManager.hpp"
#include "core/renderer/Renderer.hpp"
#include "game/backend/FiberPool.hpp"
#include "game/backend/ScriptMgr.hpp"
#include "game/frontend/ChatDisplay.hpp"
#include "game/rdr/Natives.hpp"

namespace YimMenu
{
	GUI::GUI() :
	    m_IsOpen(false)
	{
		Menu::SetupFonts();
		Menu::SetupStyle();
		Menu::Init();

		Renderer::AddWindowProcedureCallback([this](HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
			GUI::WndProc(hwnd, msg, wparam, lparam);
		});

		Renderer::AddRendererCallBack([&] { Notifications::Draw(); }, -2);
		Renderer::AddRendererCallBack([&] { ESP::Draw(); }, -3);
		Renderer::AddRendererCallBack([&] { ContextMenu::DrawContextMenu(); }, -4);
		Renderer::AddRendererCallBack([&] { ChatDisplay::Draw(); }, -5);
		Renderer::AddRendererCallBack([&] { Overlay::Draw(); }, -6);
	}

	GUI::~GUI()
	{
	}

	void GUI::ToggleMouse()
	{
		auto& io = ImGui::GetIO();
		io.MouseDrawCursor = GUI::IsOpen();
		GUI::IsOpen() ? io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse : io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
	}

	void GUI::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		const auto use_insert_key = Commands::GetCommand<BoolCommand>("togglemenukey"_J)->GetState();
		const auto key_to_check = use_insert_key ? VK_INSERT : VK_F5;

		// Ignore the OS key-repeat bit here. Besides keeping navigation predictable,
		// this prevents frontend sounds from being retriggered every repeat frame.
		if (m_IsOpen && msg == WM_KEYDOWN && (lparam & (1LL << 30)) == 0)
		{
			if (AdvancedEditor::IsOpen())
			{
				int editorKey = static_cast<int>(wparam);
				if (editorKey == VK_LEFT || editorKey == VK_HOME)
					editorKey = 'Q';
				else if (editorKey == VK_RIGHT || editorKey == VK_END)
					editorKey = 'E';
				AdvancedEditor::HandleKey(editorKey);
			}
			else
			{
				UIManager::HandleKey(wparam);
			}
		}

		if (msg == WM_KEYUP && wparam == key_to_check)
		{
			static POINT CursorCoords{};
			const bool wasOpen = m_IsOpen;
			if (m_IsOpen)
			{
				GetCursorPos(&CursorCoords);
				if (AdvancedEditor::IsOpen())
					AdvancedEditor::CloseImmediate();
			}
			else if (CursorCoords.x + CursorCoords.y)
			{
				SetCursorPos(CursorCoords.x, CursorCoords.y);
			}

			Toggle();
			ToggleMouse();

			if (ScriptMgr::CanTick())
			{
				FiberPool::Push([wasOpen] {
					// These are the RDR2 native-menu open/close cues. Normal option
					// selection/back still use SELECT/BACK in UIManager, so all four
					// interactions have a distinct audible identity.
					AUDIO::PLAY_SOUND_FRONTEND(wasOpen ? "MENU_CLOSE" : "MENU_ENTER", "HUD_PLAYER_MENU", 1, 0);
				});
			}
		}
	}
}
