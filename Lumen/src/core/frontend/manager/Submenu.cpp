#include "Submenu.hpp"

#include "core/frontend/theme/LumenTheme.hpp"

namespace YimMenu
{
	void Submenu::SetActiveCategory(const std::shared_ptr<Category> category)
	{
		m_ActiveCategory = category;
	}

	void Submenu::AddCategory(std::shared_ptr<Category>&& category)
	{
		if (!m_ActiveCategory)
			m_ActiveCategory = category;

		m_Categories.push_back(std::move(category));
	}

	void Submenu::DrawCategorySelectors()
	{
		const ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
		for (auto& category : m_Categories)
		{
			if (category)
			{
				const bool active = category == GetActiveCategory();
				ImGui::PushStyleColor(ImGuiCol_Button, active ? LumenTheme::WithAlpha(accent, 0.78f) : LumenTheme::WithAlpha(ImGui::GetStyleColorVec4(ImGuiCol_FrameBg), 0.34f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, LumenTheme::WithAlpha(accent, 0.58f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, accent);

				if (ImGui::Button(category->m_Name.data(), ImVec2(category->GetLength(), 35)))
					SetActiveCategory(category);
				ImGui::PopStyleColor(3);

				if (m_Categories.back() != category)
					ImGui::SameLine();
			}
		}
	}

	void Submenu::Draw()
	{
		if (m_ActiveCategory)
		{
			m_ActiveCategory->Draw();
		}
	}
}
