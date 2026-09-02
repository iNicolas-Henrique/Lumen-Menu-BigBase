#include "UIManager.hpp"

#include "core/frontend/theme/LumenTheme.hpp"
#include "core/frontend/widgets/NavigationTab.hpp"
#include "game/pointers/Pointers.hpp"

namespace YimMenu
{
	namespace
	{
		constexpr float kCompactThreshold = 790.0f;
	}

	void UIManager::AddSubmenuImpl(const std::shared_ptr<Submenu>&& submenu)
	{
		if (!m_ActiveSubmenu)
			m_ActiveSubmenu = submenu;

		m_Submenus.push_back(std::move(submenu));
	}

	void UIManager::SetActiveSubmenuImpl(const std::shared_ptr<Submenu> Submenu)
	{
		m_ActiveSubmenu = Submenu;
	}

	void UIManager::DrawImpl()
	{
		const ImVec2 available = ImGui::GetContentRegionAvail();
		const bool compact = available.x < kCompactThreshold;
		const float navigationWidth = compact ? LumenTheme::Layout::CompactNavigationWidth : LumenTheme::Layout::NavigationWidth;
		const float contentHeight =
		    std::max(available.y - LumenTheme::Layout::FooterHeight - ImGui::GetStyle().ItemSpacing.y, 1.0f);
		const ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);

		ImGui::PushStyleColor(ImGuiCol_ChildBg, LumenTheme::WithAlpha(ImGui::GetStyleColorVec4(ImGuiCol_FrameBg), 0.48f));
		if (ImGui::BeginChild("##lumen_navigation", ImVec2(navigationWidth, contentHeight), true))
		{
			ImGui::TextColored(LumenTheme::WithAlpha(accent, 0.9f), "NAVEGACAO");
			ImGui::TextDisabled("Areas do Lumen");
			ImGui::Spacing();
			for (auto& submenu : m_Submenus)
			{
				const bool active = submenu == m_ActiveSubmenu;
				if (Widgets::NavigationTab(submenu->m_Name, active))
					SetActiveSubmenu(submenu);
			}

			ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY(), contentHeight - 58.0f));
			ImGui::Separator();
			ImGui::TextDisabled("ATALHO DO MENU");
			ImGui::TextUnformatted("F5 / INSERT");
		}
		ImGui::EndChild();
		ImGui::PopStyleColor();

		ImGui::SameLine();
		if (ImGui::BeginChild("##lumen_content", ImVec2(0.0f, contentHeight), false))
		{
			if (m_ActiveSubmenu)
			{
				ImGui::TextColored(accent, "%s", m_ActiveSubmenu->m_Name.c_str());
				ImGui::SameLine();
				if (const auto category = m_ActiveSubmenu->GetActiveCategory())
					ImGui::TextDisabled("  /  %s", category->m_Name.c_str());
			}

			ImGui::Spacing();
			ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, LumenTheme::Layout::PanelRounding);
			if (ImGui::BeginChild("##lumen_categories", ImVec2(0.0f, LumenTheme::Layout::CategoryBarHeight), true, ImGuiWindowFlags_HorizontalScrollbar))
			{
				if (m_ActiveSubmenu)
					m_ActiveSubmenu->DrawCategorySelectors();
			}
			ImGui::EndChild();

			if (ImGui::BeginChild("##lumen_options", ImVec2(0.0f, 0.0f), true))
			{
				if (m_OptionsFont)
					ImGui::PushFont(m_OptionsFont);

				if (m_ActiveSubmenu)
					m_ActiveSubmenu->Draw();

				if (m_OptionsFont)
					ImGui::PopFont();
			}
			ImGui::EndChild();
			ImGui::PopStyleVar();
		}
		ImGui::EndChild();

		ImGui::Separator();
		ImGui::TextDisabled("LUMEN  |  RDR2  |  Mouse e teclado");
		const char* renderer = Pointers.IsVulkan ? "VULKAN" : "DIRECTX 12";
		const float rendererWidth = ImGui::CalcTextSize(renderer).x;
		ImGui::SameLine(std::max(ImGui::GetWindowContentRegionMax().x - rendererWidth, ImGui::GetCursorPosX()));
		ImGui::TextColored(accent, "%s", renderer);
	}

	std::shared_ptr<Submenu> UIManager::GetActiveSubmenuImpl()
	{
		if (m_ActiveSubmenu)
		{
			return m_ActiveSubmenu;
		}

		return nullptr;
	}

	std::shared_ptr<Category> UIManager::GetActiveCategoryImpl()
	{
		if (m_ActiveSubmenu)
		{
			return m_ActiveSubmenu->GetActiveCategory();
		}

		return nullptr;
	}
}
