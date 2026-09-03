#pragma once

#include <algorithm>
#include <cstddef>
#include <imgui.h>

namespace YimMenu
{
	struct ResponsiveMenuLayout
	{
		float Scale;
		float X;
		float Y;
		float Width;
		float HeaderHeight;
		float SubmenuHeight;
		float OptionHeight;
		float FooterHeight;
		float DescriptionHeight;
		std::size_t OptionsPerPage;
	};

	inline ResponsiveMenuLayout GetResponsiveMenuLayout()
	{
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		const ImVec2 display = viewport ? viewport->WorkSize : ImGui::GetIO().DisplaySize;
		const ImVec2 origin = viewport ? viewport->WorkPos : ImVec2(0.0f, 0.0f);
		const float scale = std::clamp(std::min(display.x / 800.0f, display.y / 600.0f), 0.72f, 1.15f);
		const float margin = std::max(10.0f, 18.0f * scale);
		const float width = std::clamp(360.0f * scale, 280.0f, std::max(280.0f, display.x - margin * 2.0f));
		const float header = 64.0f * scale;
		const float submenu = 30.0f * scale;
		const float option = 28.0f * scale;
		const float footer = 40.0f * scale;
		const float description = 56.0f * scale;
		const float fixedHeight = header + submenu + footer + description + margin * 2.0f + 8.0f;
		const std::size_t pageSize = static_cast<std::size_t>(std::clamp((display.y - fixedHeight) / option, 2.0f, 11.0f));

		return {scale, origin.x + margin, origin.y + margin, width, header, submenu, option, footer, description, pageSize};
	}
}
