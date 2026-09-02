#include "UIManager.hpp"

#include "AdvancedEditor.hpp"
#include "ResponsiveLayout.hpp"
#include "core/commands/Commands.hpp"
#include "game/backend/FiberPool.hpp"
#include "game/backend/ScriptMgr.hpp"
#include "game/frontend/Menu.hpp"

#include <cmath>

namespace YimMenu
{
	namespace
	{
		constexpr ImU32 kDarkGreen = IM_COL32(34, 48, 11, 255);
		constexpr ImU32 kLightGreen = IM_COL32(52, 77, 14, 255);

		void DrawText(ImDrawList* drawList, const ImVec2& position, ImU32 color, std::string_view text, float scale)
		{
			drawList->AddText(
			    ImGui::GetFont(), ImGui::GetFontSize() * scale, position, color, text.data(), text.data() + text.size());
		}

		void DrawWrappedText(ImDrawList* drawList, const ImVec2& position, ImU32 color, std::string_view text, float width, float scale)
		{
			drawList->AddText(
			    ImGui::GetFont(), ImGui::GetFontSize() * scale, position, color, text.data(), text.data() + text.size(), width);
		constexpr float kMenuX = 42.0f;
		constexpr float kMenuY = 82.0f;
		constexpr float kMenuWidth = 360.0f;
		constexpr float kHeaderHeight = 72.0f;
		constexpr float kSubmenuHeight = 32.0f;
		constexpr float kOptionHeight = 31.0f;
		constexpr float kFooterHeight = 43.0f;
		constexpr float kDescriptionHeight = 62.0f;
		constexpr std::size_t kOptionsPerPage = 11;

		ImU32 Color(const ImVec4& color, float alpha = 1.0f)
		{
			ImVec4 adjusted = color;
			adjusted.w *= alpha;
			return ImGui::ColorConvertFloat4ToU32(adjusted);
		}

		void DrawText(ImDrawList* drawList, const ImVec2& position, ImU32 color, std::string_view text)
		{
			drawList->AddText(position, color, text.data(), text.data() + text.size());
		}

		void DrawWrappedText(ImDrawList* drawList, const ImVec2& position, ImU32 color, std::string_view text, float width)
		{
			drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), position, color, text.data(), text.data() + text.size(), width);
		}
	}

	void UIManager::AddSubmenuImpl(const std::shared_ptr<Submenu>&& submenu)
	{
		if (!m_ActiveSubmenu)
			m_ActiveSubmenu = submenu;
		m_Submenus.push_back(std::move(submenu));
	}

	void UIManager::SetActiveSubmenuImpl(const std::shared_ptr<Submenu> submenu)
	{
		m_ActiveSubmenu = submenu;
	}

	std::vector<UIItem*> UIManager::GetCurrentItems() const
	{
		std::vector<UIItem*> items;
		if (!m_ActiveSubmenu || !m_ActiveSubmenu->GetActiveCategory())
			return items;
		for (const auto& item : m_ActiveSubmenu->GetActiveCategory()->GetItems())
			if (item)
				item->CollectMenuItems(items);
		return items;
	}

	std::size_t UIManager::GetEntryCount() const
	{
		switch (m_Level)
		{
		case Level::Root: return m_Submenus.size() + 1;
		case Level::Categories: return m_ActiveSubmenu ? m_ActiveSubmenu->m_Categories.size() : 0;
		case Level::Options: return GetCurrentItems().size();
		}
		return 0;
	}

	void UIManager::HandleKeyImpl(WPARAM key)
	{
		const std::size_t count = GetEntryCount();
		if (count > 0 && m_Selected >= count)
			m_Selected = count - 1;
		if (key == VK_BACK || (key == VK_LEFT && m_Level != Level::Options))
		{
			if (m_Level == Level::Options)
				m_Level = Level::Categories;
			else if (m_Level == Level::Categories)
				m_Level = Level::Root;
			m_Selected = 0;
			return;
		}
		if (count == 0)
			return;
		if (key == VK_UP)
			m_Selected = m_Selected == 0 ? count - 1 : m_Selected - 1;
		else if (key == VK_DOWN)
			m_Selected = (m_Selected + 1) % count;
		else if (m_Level == Level::Options && (key == VK_LEFT || key == VK_RIGHT))
		{
			auto items = GetCurrentItems();
			items[m_Selected]->HandleAction(key == VK_LEFT ? Classic::OptionAction::Left : Classic::OptionAction::Right);
		}
		else if (key == VK_RETURN)
		{
		{
			auto items = GetCurrentItems();
			items[m_Selected]->HandleAction(key == VK_LEFT ? Classic::OptionAction::Left : Classic::OptionAction::Right);
		}
		else if (key == VK_RETURN)
		{
			if (m_Level == Level::Root)
			{
				if (m_Selected == m_Submenus.size())
				{
					if (ScriptMgr::CanTick())
						FiberPool::Push([] {
							Commands::Shutdown();
							g_Running = false;
						});
					else
					{
						Commands::Shutdown();
						g_Running = false;
					}
					return;
				}
				m_ActiveSubmenu = m_Submenus[m_Selected];
				m_Level = Level::Categories;
				m_Selected = 0;
			}
			else if (m_Level == Level::Categories)
			{
				m_ActiveSubmenu->SetActiveCategory(m_ActiveSubmenu->m_Categories[m_Selected]);
				m_Level = Level::Options;
				m_Selected = 0;
			}
			else
			{
				auto items = GetCurrentItems();
				if (items[m_Selected]->RequiresImGuiEditor())
					AdvancedEditor::Open(items[m_Selected]);
				else
					items[m_Selected]->HandleAction(Classic::OptionAction::Enter);
			}
		}
	}

	void UIManager::RenderImpl()
	{
		if (ImGui::GetIO().DisplaySize.x < 320.0f || ImGui::GetIO().DisplaySize.y < 240.0f)
			return;
		const auto currentItems = m_Level == Level::Options ? GetCurrentItems() : std::vector<UIItem*>{};
		const std::size_t currentCount = m_Level == Level::Options ? currentItems.size() : GetEntryCount();
		if (currentCount > 0 && m_Selected >= currentCount)
			m_Selected = currentCount - 1;

		ImDrawList* drawList = ImGui::GetForegroundDrawList();
		const auto layout = GetResponsiveMenuLayout();
		const float kMenuX = layout.X;
		const float kMenuY = layout.Y;
		const float kMenuWidth = layout.Width;
		const float kHeaderHeight = layout.HeaderHeight;
		const float kSubmenuHeight = layout.SubmenuHeight;
		const float kOptionHeight = layout.OptionHeight;
		const float kFooterHeight = layout.FooterHeight;
		const float kDescriptionHeight = layout.DescriptionHeight;
		const std::size_t kOptionsPerPage = layout.OptionsPerPage;
		const ImU32 white = IM_COL32(245, 241, 232, 255);
		float y = kMenuY;

		const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(ImGui::GetTime()) * 0.65f);
		const ImU32 animatedGreen = ImGui::ColorConvertFloat4ToU32(ImVec4((34.0f + 18.0f * pulse) / 255.0f, (48.0f + 29.0f * pulse) / 255.0f, (11.0f + 3.0f * pulse) / 255.0f, 1.0f));
		drawList->AddRectFilledMultiColor(ImVec2(kMenuX, y), ImVec2(kMenuX + kMenuWidth, y + kHeaderHeight), kDarkGreen, animatedGreen, kLightGreen, kDarkGreen);
		const char* brand = "L U M E N";
		const float brandWidth = ImGui::CalcTextSize(brand).x;
		DrawText(drawList,
		    ImVec2(kMenuX + (kMenuWidth - brandWidth * layout.Scale) * 0.5f, y + kHeaderHeight * 0.34f),
		    white,
		    brand,
		    layout.Scale);
		y += kHeaderHeight;

		std::string title = "MENU PRINCIPAL";
		if (m_Level != Level::Root && m_ActiveSubmenu)
			title = m_Level == Level::Categories ? m_ActiveSubmenu->m_Name : m_ActiveSubmenu->GetActiveCategory()->m_Name;
		drawList->AddRectFilled(ImVec2(kMenuX, y), ImVec2(kMenuX + kMenuWidth, y + kSubmenuHeight), IM_COL32(12, 12, 12, 235));
		DrawText(drawList, ImVec2(kMenuX + 10.0f * layout.Scale, y + 6.0f * layout.Scale), white, title, layout.Scale);
		const std::size_t count = currentCount;
		const std::string counter = count ? std::format("{} / {}", m_Selected + 1, count) : "0 / 0";
		DrawText(drawList,
		    ImVec2(kMenuX + kMenuWidth - ImGui::CalcTextSize(counter.c_str()).x * layout.Scale - 10.0f * layout.Scale,
		        y + 6.0f * layout.Scale),
		    IM_COL32(180, 180, 180, 255),
		    counter,
		    layout.Scale);
		y += kSubmenuHeight;

		std::vector<std::pair<std::string, std::string>> entries;
		entries.reserve(currentCount + (m_Level == Level::Root ? 1 : 0));
		std::string description;
		if (m_Level == Level::Root)
		{
			for (const auto& submenu : m_Submenus)
				entries.emplace_back(submenu->m_Name, ">");
			entries.emplace_back("Encerrar Lumen", "");
		}
		else if (m_Level == Level::Categories)
		{
			for (const auto& category : m_ActiveSubmenu->m_Categories)
				entries.emplace_back(category->m_Name, ">");
		}
		else
		{
			for (UIItem* item : currentItems)
				entries.emplace_back(item->GetMenuLabel(), item->GetMenuValue());
			if (m_Selected < currentItems.size())
				description = currentItems[m_Selected]->GetMenuDescription();
		}

		const std::size_t first = m_Selected >= kOptionsPerPage ? m_Selected - kOptionsPerPage + 1 : 0;
		const std::size_t last = std::min(first + kOptionsPerPage, entries.size());
		for (std::size_t index = first; index < last; ++index)
		{
			const bool selected = index == m_Selected;
			const ImU32 rowColor = selected ? kLightGreen : (index % 2 ? IM_COL32(18, 21, 14, 235) : IM_COL32(10, 12, 8, 235));
			drawList->AddRectFilled(ImVec2(kMenuX, y), ImVec2(kMenuX + kMenuWidth, y + kOptionHeight), rowColor);
			if (selected)
				drawList->AddRectFilled(ImVec2(kMenuX, y), ImVec2(kMenuX + 4.0f, y + kOptionHeight), white);
			const float rightWidth = ImGui::CalcTextSize(entries[index].second.c_str()).x * layout.Scale;
			const float labelClipRight = std::max(kMenuX + 8.0f, kMenuX + kMenuWidth - rightWidth - 20.0f);
			drawList->PushClipRect(ImVec2(kMenuX + 8.0f, y), ImVec2(labelClipRight, y + kOptionHeight), true);
			DrawText(drawList,
			    ImVec2(kMenuX + 10.0f * layout.Scale, y + 5.0f * layout.Scale),
			    selected ? IM_COL32(10, 10, 10, 255) : white,
			    entries[index].first,
			    layout.Scale);
			drawList->PopClipRect();
			DrawText(drawList,
			    ImVec2(kMenuX + kMenuWidth - rightWidth - 10.0f, y + 7.0f),
			    selected ? IM_COL32(10, 10, 10, 255) : white,
			    entries[index].second,
			    layout.Scale);
			y += kOptionHeight;
		}

		drawList->AddRectFilled(ImVec2(kMenuX, y), ImVec2(kMenuX + kMenuWidth, y + kFooterHeight), IM_COL32(12, 12, 12, 235));
		const std::string runtime = std::format("Jogo {}  |  Lumen v{}", m_GameBuild, m_ModVersion);
		DrawText(
		    drawList, ImVec2(kMenuX + 10.0f, y + 4.0f * layout.Scale), IM_COL32(190, 200, 175, 255), runtime, layout.Scale * 0.82f);
		DrawText(drawList,
		    ImVec2(kMenuX + 10.0f, y + 21.0f * layout.Scale),
		    IM_COL32(155, 165, 145, 255),
		    "Setas: navegar  |  Enter: selecionar  |  Voltar: Backspace",
		    layout.Scale * 0.72f);
		y += kFooterHeight + 5.0f;
		if (!description.empty())
		{
			drawList->AddRectFilled(ImVec2(kMenuX, y), ImVec2(kMenuX + kMenuWidth, y + kDescriptionHeight), IM_COL32(5, 5, 5, 210), 3.0f);
			DrawWrappedText(drawList,
			    ImVec2(kMenuX + 10.0f, y + 8.0f),
			    IM_COL32(220, 214, 201, 255),
			    description,
			    kMenuWidth - 20.0f,
			    layout.Scale * 0.86f);
			y += kDescriptionHeight;
		}
		drawList->AddRect(ImVec2(kMenuX - 2.0f, kMenuY - 2.0f), ImVec2(kMenuX + kMenuWidth + 2.0f, y + 2.0f), IM_COL32(12, 16, 7, 230), 6.0f, 0, 4.0f);
		drawList->AddRect(ImVec2(kMenuX, kMenuY), ImVec2(kMenuX + kMenuWidth, y), kLightGreen, 4.0f, 0, 1.0f);

		ImDrawList* drawList = ImGui::GetForegroundDrawList();
		const ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
		const ImU32 white = IM_COL32(245, 241, 232, 255);
		float y = kMenuY;

		drawList->AddRectFilled(ImVec2(kMenuX, y), ImVec2(kMenuX + kMenuWidth, y + kHeaderHeight), IM_COL32(28, 19, 13, 245), 4.0f);
		drawList->AddRectFilledMultiColor(ImVec2(kMenuX, y + kHeaderHeight - 5.0f), ImVec2(kMenuX + kMenuWidth, y + kHeaderHeight), Color(accent), Color(accent, 0.45f), Color(accent, 0.45f), Color(accent));
		DrawText(drawList, ImVec2(kMenuX + 16.0f, y + 14.0f), white, "L U M E N");
		DrawText(drawList, ImVec2(kMenuX + 16.0f, y + 40.0f), IM_COL32(190, 181, 164, 255), "RED DEAD REDEMPTION 2");
		y += kHeaderHeight;

		std::string title = "MENU PRINCIPAL";
		if (m_Level != Level::Root && m_ActiveSubmenu)
			title = m_Level == Level::Categories ? m_ActiveSubmenu->m_Name : m_ActiveSubmenu->GetActiveCategory()->m_Name;
		drawList->AddRectFilled(ImVec2(kMenuX, y), ImVec2(kMenuX + kMenuWidth, y + kSubmenuHeight), IM_COL32(12, 12, 12, 235));
		DrawText(drawList, ImVec2(kMenuX + 10.0f, y + 7.0f), white, title);
		const std::size_t count = currentCount;
		const std::string counter = count ? std::format("{} / {}", m_Selected + 1, count) : "0 / 0";
		DrawText(drawList, ImVec2(kMenuX + kMenuWidth - ImGui::CalcTextSize(counter.c_str()).x - 10.0f, y + 7.0f), IM_COL32(180, 180, 180, 255), counter);
		y += kSubmenuHeight;

		std::vector<std::pair<std::string, std::string>> entries;
		entries.reserve(currentCount + (m_Level == Level::Root ? 1 : 0));
		std::string description;
		if (m_Level == Level::Root)
		{
			for (const auto& submenu : m_Submenus)
				entries.emplace_back(submenu->m_Name, ">");
			entries.emplace_back("Encerrar Lumen", "");
		}
		else if (m_Level == Level::Categories)
		{
			for (const auto& category : m_ActiveSubmenu->m_Categories)
				entries.emplace_back(category->m_Name, ">");
		}
		else
		{
			for (UIItem* item : currentItems)
				entries.emplace_back(item->GetMenuLabel(), item->GetMenuValue());
			if (m_Selected < currentItems.size())
				description = currentItems[m_Selected]->GetMenuDescription();
		}

		const std::size_t first = m_Selected >= kOptionsPerPage ? m_Selected - kOptionsPerPage + 1 : 0;
		const std::size_t last = std::min(first + kOptionsPerPage, entries.size());
		for (std::size_t index = first; index < last; ++index)
		{
			const bool selected = index == m_Selected;
			const ImU32 rowColor = selected ? Color(accent, 0.9f) : (index % 2 ? IM_COL32(11, 10, 9, 220) : IM_COL32(5, 5, 5, 215));
			drawList->AddRectFilled(ImVec2(kMenuX, y), ImVec2(kMenuX + kMenuWidth, y + kOptionHeight), rowColor);
			if (selected)
				drawList->AddRectFilled(ImVec2(kMenuX, y), ImVec2(kMenuX + 4.0f, y + kOptionHeight), white);
			const float rightWidth = ImGui::CalcTextSize(entries[index].second.c_str()).x;
			const float labelClipRight = std::max(kMenuX + 8.0f, kMenuX + kMenuWidth - rightWidth - 20.0f);
			drawList->PushClipRect(ImVec2(kMenuX + 8.0f, y), ImVec2(labelClipRight, y + kOptionHeight), true);
			DrawText(
			    drawList, ImVec2(kMenuX + 10.0f, y + 7.0f), selected ? IM_COL32(10, 10, 10, 255) : white, entries[index].first);
			drawList->PopClipRect();
			DrawText(drawList,
			    ImVec2(kMenuX + kMenuWidth - rightWidth - 10.0f, y + 7.0f),
			    selected ? IM_COL32(10, 10, 10, 255) : white,
			    entries[index].second);
			y += kOptionHeight;
		}

		drawList->AddRectFilled(ImVec2(kMenuX, y), ImVec2(kMenuX + kMenuWidth, y + kFooterHeight), IM_COL32(12, 12, 12, 235));
		DrawText(drawList, ImVec2(kMenuX + 10.0f, y + 5.0f), IM_COL32(190, 190, 190, 255), "Setas: navegar e alterar");
		DrawText(drawList, ImVec2(kMenuX + 10.0f, y + 23.0f), IM_COL32(165, 165, 165, 255), "Enter: selecionar   Backspace: voltar");
		y += kFooterHeight + 5.0f;
		if (!description.empty())
		{
			drawList->AddRectFilled(ImVec2(kMenuX, y), ImVec2(kMenuX + kMenuWidth, y + kDescriptionHeight), IM_COL32(5, 5, 5, 210), 3.0f);
			DrawWrappedText(drawList, ImVec2(kMenuX + 10.0f, y + 8.0f), IM_COL32(220, 214, 201, 255), description, kMenuWidth - 20.0f);
			y += kDescriptionHeight;
		}
		drawList->AddRect(ImVec2(kMenuX, kMenuY), ImVec2(kMenuX + kMenuWidth, y), Color(accent, 0.55f), 4.0f, 0, 1.0f);

		AdvancedEditor::Draw();
	}

	std::shared_ptr<Submenu> UIManager::GetActiveSubmenuImpl()
	{
		return m_ActiveSubmenu;
	}
	std::shared_ptr<Category> UIManager::GetActiveCategoryImpl()
	{
		return m_ActiveSubmenu ? m_ActiveSubmenu->GetActiveCategory() : nullptr;
	}
}
