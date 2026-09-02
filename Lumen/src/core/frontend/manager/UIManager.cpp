#include "UIManager.hpp"
#include "game/frontend/Menu.hpp" // Include the Menu header for font access
#include "game/pointers/Pointers.hpp"

namespace YimMenu
{
	namespace
	{
		constexpr float kSidebarWidth = 168.0f;
		constexpr float kFooterHeight = 28.0f;

		ImVec4 WithAlpha(ImVec4 color, float alpha)
		{
			color.w = alpha;
			return color;
		}
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
		const float contentHeight = std::max(available.y - kFooterHeight - ImGui::GetStyle().ItemSpacing.y, 1.0f);
		const ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);

		ImGui::PushStyleColor(ImGuiCol_ChildBg, WithAlpha(ImGui::GetStyleColorVec4(ImGuiCol_FrameBg), 0.55f));
		if (ImGui::BeginChild("##lumen_navigation", ImVec2(kSidebarWidth, contentHeight), true))
		{
			ImGui::TextDisabled("NAVEGACAO");
			ImGui::Spacing();
			for (auto& submenu : m_Submenus)
			{
				const bool active = submenu == m_ActiveSubmenu;
				if (active)
				{
					ImGui::PushStyleColor(ImGuiCol_Header, WithAlpha(accent, 0.85f));
					ImGui::PushStyleColor(ImGuiCol_HeaderHovered, accent);
				}

				const std::string label = std::string(active ? "  >  " : "     ") + submenu->m_Name;
				if (ImGui::Selectable(label.c_str(), active, 0, ImVec2(0.0f, 34.0f)))
					SetActiveSubmenu(submenu);

				if (active)
					ImGui::PopStyleColor(2);
			}

			ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY(), contentHeight - 52.0f));
			ImGui::Separator();
			ImGui::TextDisabled("F5 / INSERT");
			ImGui::TextDisabled("Abrir ou fechar o menu");
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
					ImGui::TextDisabled("/  %s", category->m_Name.c_str());
			}

			ImGui::Spacing();
			ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
			if (ImGui::BeginChild("##lumen_categories", ImVec2(0.0f, 48.0f), true, ImGuiWindowFlags_NoScrollbar))
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
		ImGui::TextDisabled("Lumen  |  Navegue com mouse ou teclado");
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
