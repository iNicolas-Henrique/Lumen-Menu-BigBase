#pragma once
#include "Overlay.hpp"
#include "Menu.hpp"
#include "core/commands/BoolCommand.hpp"
#include "game/pointers/Pointers.hpp"
#include "game/rdr/Natives.hpp"
#include "game/rdr/invoker/Invoker.hpp"

namespace YimMenu::Features
{
	BoolCommand _OverlayEnabled("overlay", "Overlay Enabled", "Show an info overlay at the top left corner of the screen");
	BoolCommand _OverlayShowFPS("overlayfps", "Overlay Show FPS", "Show frame rate in the info overlay");
}

namespace YimMenu
{
	void Overlay::Draw()
	{
		if (!Features::_OverlayEnabled.GetState() || !Features::_OverlayShowFPS.GetState() || !NativeInvoker::AreHandlersCached())
			return;

		ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushFont(Menu::Font::g_OverlayFont);

		ImGui::Begin("##overlay", nullptr,
		    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
		        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
		        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs);

		ImGui::Text("FPS: %d", static_cast<int>(1.0f / MISC::GET_SYSTEM_TIME_STEP()));

		ImGui::End();
		ImGui::PopFont();
		ImGui::PopStyleColor();
	}
}
