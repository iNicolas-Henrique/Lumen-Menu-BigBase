#include "LumenTheme.hpp"

namespace YimMenu::LumenTheme
{
	void ApplyMetrics()
	{
		auto& style = ImGui::GetStyle();
		style.WindowPadding = ImVec2(13.0f, 11.0f);
		style.FramePadding = ImVec2(10.0f, 7.0f);
		style.CellPadding = ImVec2(8.0f, 5.0f);
		style.ItemSpacing = ImVec2(8.0f, 7.0f);
		style.ItemInnerSpacing = ImVec2(7.0f, 5.0f);
		style.ScrollbarSize = 12.0f;
		style.GrabMinSize = 10.0f;
		style.WindowRounding = Layout::PanelRounding;
		style.ChildRounding = Layout::PanelRounding;
		style.FrameRounding = 5.0f;
		style.PopupRounding = Layout::PanelRounding;
		style.ScrollbarRounding = 8.0f;
		style.GrabRounding = 5.0f;
		style.TabRounding = 5.0f;
		style.WindowBorderSize = 1.0f;
		style.ChildBorderSize = 1.0f;
		style.FrameBorderSize = 0.0f;
	}

	void DrawWindowAtmosphere(ImDrawList* drawList, const ImVec2& position, const ImVec2& size)
	{
		const ImVec2 end(position.x + size.x, position.y + size.y);
		drawList->AddRectFilledMultiColor(position, end, IM_COL32(8, 12, 17, 238), IM_COL32(24, 18, 11, 232), IM_COL32(9, 18, 20, 238), IM_COL32(7, 10, 15, 242));
		drawList->AddCircleFilled(ImVec2(end.x - 38.0f, position.y + 28.0f), 155.0f, IM_COL32(219, 165, 70, 18), 48);
		drawList->AddCircleFilled(ImVec2(position.x + 20.0f, end.y), 185.0f, IM_COL32(55, 165, 158, 12), 48);
	}

	void DrawBrandMark(ImDrawList* drawList, const ImVec2& position, const ImVec4& accent)
	{
		const ImU32 color = ImGui::ColorConvertFloat4ToU32(accent);
		drawList->AddCircle(position, 13.0f, color, 32, 2.0f);
		drawList->AddLine(ImVec2(position.x - 5.0f, position.y - 7.0f), ImVec2(position.x - 5.0f, position.y + 7.0f), color, 2.0f);
		drawList->AddLine(ImVec2(position.x - 5.0f, position.y + 7.0f), ImVec2(position.x + 6.0f, position.y + 7.0f), color, 2.0f);
	}

	ImVec4 WithAlpha(ImVec4 color, float alpha)
	{
		color.w = alpha;
		return color;
	}
}
