#pragma once

#include <imgui.h>

namespace YimMenu::LumenTheme
{
	struct Layout
	{
		static constexpr float HeaderHeight = 58.0f;
		static constexpr float FooterHeight = 31.0f;
		static constexpr float NavigationWidth = 174.0f;
		static constexpr float CompactNavigationWidth = 146.0f;
		static constexpr float CategoryBarHeight = 49.0f;
		static constexpr float PanelRounding = 7.0f;
	};

	void ApplyMetrics();
	void DrawWindowAtmosphere(ImDrawList* drawList, const ImVec2& position, const ImVec2& size);
	void DrawBrandMark(ImDrawList* drawList, const ImVec2& position, const ImVec4& accent);
	ImVec4 WithAlpha(ImVec4 color, float alpha);
}
