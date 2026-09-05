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
#include "game/frontend/ChatDisplay.hpp"

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

		Renderer::AddRendererCallBack(
		    [&] {
			    Notifications::Draw();
		    },
		    -2);
		Renderer::AddRendererCallBack(
		    [&] {
			    ESP::Draw();
		    },
		    -3);

		Renderer::AddRendererCallBack(
		    [&] {
			    ContextMenu::DrawContextMenu();
		    },
		    -4);
		Renderer::AddRendererCallBack(
		    [&] {
			    ChatDisplay::Draw();
		    },
		    -5);
		Renderer::AddRendererCallBack(
		    [&] {
			    Overlay::Draw();
		    },
		    -6);
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

		// Um unico WM_KEYDOWN novo por toque. O bit 30 fica ligado nas repeticoes
		// automaticas do Windows, evitando multiplas acoes pelo mesmo toque.
		if (m_IsOpen && msg == WM_KEYDOWN && (lparam & (1LL << 30)) == 0)
		{
			if (AdvancedEditor::IsOpen())
			{
				// Alguns teclados compactos reportam as setas horizontais como Home/End.
				// O editor aceita ambos, alem de Q/E como alternativa.
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
			if (m_IsOpen)
			{
				GetCursorPos(&CursorCoords);
			}
			else if (CursorCoords.x + CursorCoords.y)
			{
				SetCursorPos(CursorCoords.x, CursorCoords.y);
			}
			Toggle();
			ToggleMouse();
		}
	}
}
