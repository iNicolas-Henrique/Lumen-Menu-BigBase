#include "NavigationTab.hpp"

#include "core/frontend/theme/LumenTheme.hpp"

namespace YimMenu::Widgets
{
	bool NavigationTab(std::string_view label, bool active)
	{
		const auto& style = ImGui::GetStyle();
		const ImVec4 accent = style.Colors[ImGuiCol_ButtonActive];
		const ImVec4 idle = LumenTheme::WithAlpha(style.Colors[ImGuiCol_FrameBg], 0.28f);

		ImGui::PushID(label.data(), label.data() + label.size());
		ImGui::PushStyleColor(ImGuiCol_Header, active ? LumenTheme::WithAlpha(accent, 0.72f) : idle);
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, LumenTheme::WithAlpha(accent, active ? 0.86f : 0.42f));
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, LumenTheme::WithAlpha(accent, 0.92f));
		const bool pressed = ImGui::Selectable(label.data(), active, 0, ImVec2(0.0f, 36.0f));
		ImGui::PopStyleColor(3);

		if (active)
		{
			const ImVec2 minimum = ImGui::GetItemRectMin();
			const ImVec2 maximum = ImGui::GetItemRectMax();
			ImGui::GetWindowDrawList()->AddRectFilled(minimum, ImVec2(minimum.x + 3.0f, maximum.y), ImGui::ColorConvertFloat4ToU32(accent), 2.0f);
		}

		ImGui::PopID();
		return pressed;
	}
}
