#include "UIManager.hpp"

#include "AdvancedEditor.hpp"
#include "ResponsiveLayout.hpp"
#include "core/commands/Commands.hpp"
#include "core/frontend/Localization.hpp"
#include "core/frontend/PerformanceOptions.hpp"
#include "game/backend/FiberPool.hpp"
#include "game/backend/ScriptMgr.hpp"
#include "game/frontend/Menu.hpp"
#include "game/rdr/Natives.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace YimMenu
{
	namespace
	{
		constexpr ImU32 kLightGreen = IM_COL32(52, 77, 14, 255);

		ImU32 ApplyAlpha(ImU32 color, float alpha)
		{
			const auto sourceAlpha = static_cast<unsigned>((color >> IM_COL32_A_SHIFT) & 0xFFu);
			const auto scaledAlpha = static_cast<unsigned>(std::clamp(alpha, 0.0f, 1.0f) * static_cast<float>(sourceAlpha));
			return (color & ~(0xFFu << IM_COL32_A_SHIFT)) | (scaledAlpha << IM_COL32_A_SHIFT);
		}

		void DrawText(ImDrawList* drawList, const ImVec2& position, ImU32 color, std::string_view text, float scale)
		{
			drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize() * scale, position, color, text.data(), text.data() + text.size());
		}

		void QueueMenuSound(const char* sound)
		{
			if (!sound || !ScriptMgr::CanTick())
				return;
			FiberPool::Push([sound] { AUDIO::PLAY_SOUND_FRONTEND(sound, "HUD_PLAYER_MENU", 1, 0); });
		}

		bool HiddenClassicItem(std::string_view label)
		{
			return label.empty() || label == "Player Must Die" ||
			       label == "Entregar armas e munição" || label == "Entregar armas e municao" ||
			       label == "Nível máximo de procurado" || label == "Nivel maximo de procurado" ||
			       label == "Sem nível de procurado" || label == "Sem nivel de procurado" ||
			       label == "Remover procurado" ||
			       label == "Olho da Morte clássico" || label == "Classic Dead Eye" ||
			       label == "Deadeye Auto-Tagging" || label == "Alvos automáticos do Olho da Morte" ||
			       label == "Unlock Deadeye Abilities + Weak Spots";
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

		items.erase(std::remove_if(items.begin(), items.end(), [](UIItem* item) {
			return !item || HiddenClassicItem(item->GetMenuLabel());
		}), items.end());
		return items;
	}

	std::size_t UIManager::GetEntryCount() const
	{
		switch (m_Level)
		{
		case Level::Root: return m_Submenus.size() + 1;
		case Level::Categories: return m_ActiveSubmenu ? m_ActiveSubmenu->m_Categories.size() : 0;
		case Level::Options: return GetCurrentItems().size();
		case Level::ConfirmShutdown: return 2;
		}
		return 0;
	}

	void UIManager::HandleKeyImpl(WPARAM key)
	{
		if (AdvancedEditor::IsOpen())
		{
			AdvancedEditor::HandleKey(static_cast<int>(key));
			return;
		}

		const std::size_t count = GetEntryCount();
		if (count > 0 && m_Selected >= count)
			m_Selected = count - 1;
		if (m_Level == Level::ConfirmShutdown && (key == VK_BACK || key == VK_LEFT))
		{
			QueueMenuSound("BACK");
			m_Level = Level::Root;
			m_Selected = m_Submenus.size();
			return;
		}

		if (key == VK_BACK || (key == VK_LEFT && m_Level != Level::Options))
		{
			QueueMenuSound("BACK");
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
			QueueMenuSound("SELECT");
			if (m_Level == Level::ConfirmShutdown)
			{
				if (m_Selected == 0)
				{
					if (ScriptMgr::CanTick())
						FiberPool::Push([] { Commands::Shutdown(); g_Running = false; });
					else
					{
						Commands::Shutdown();
						g_Running = false;
					}
				}
				else
				{
					m_Level = Level::Root;
					m_Selected = m_Submenus.size();
				}
				return;
			}

			if (m_Level == Level::Root)
			{
				if (m_Selected == m_Submenus.size())
				{
					m_Level = Level::ConfirmShutdown;
					m_Selected = 1;
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
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		if (!viewport || viewport->WorkSize.x < 320.0f || viewport->WorkSize.y < 240.0f)
			return;

		AdvancedEditor::Tick();
		const float classicAlpha = AdvancedEditor::GetClassicMenuAlpha();

		if (classicAlpha > 0.002f)
		{
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
			const std::size_t kOptionsPerPage = layout.OptionsPerPage;
			const auto C = [classicAlpha](ImU32 color) { return ApplyAlpha(color, classicAlpha); };
			const ImU32 white = C(IM_COL32(245, 241, 232, 255));
			const bool animate = PerformanceOptions::MenuAnimations.GetState();
			const bool shadows = PerformanceOptions::MenuShadows.GetState();
			float y = kMenuY;

			const float time = animate ? static_cast<float>(ImGui::GetTime()) : 0.0f;
			const float pulse = animate ? (0.5f + 0.5f * std::sin(time * 0.95f)) : 0.5f;
			const float sweep = animate ? (0.5f + 0.5f * std::sin(time * 1.28f + 1.15f)) : 0.5f;
			const ImU32 headerTL = C(ImGui::ColorConvertFloat4ToU32(ImVec4((15.0f + 5.0f * pulse) / 255.0f, (28.0f + 12.0f * pulse) / 255.0f, 6.0f / 255.0f, 1.0f)));
			const ImU32 headerTR = C(ImGui::ColorConvertFloat4ToU32(ImVec4((52.0f + 20.0f * sweep) / 255.0f, (88.0f + 28.0f * sweep) / 255.0f, (14.0f + 7.0f * sweep) / 255.0f, 1.0f)));
			const ImU32 headerBR = C(ImGui::ColorConvertFloat4ToU32(ImVec4((25.0f + 13.0f * pulse) / 255.0f, (57.0f + 21.0f * pulse) / 255.0f, (10.0f + 6.0f * pulse) / 255.0f, 1.0f)));
			const ImU32 headerBL = C(ImGui::ColorConvertFloat4ToU32(ImVec4((8.0f + 4.0f * sweep) / 255.0f, (18.0f + 9.0f * sweep) / 255.0f, 5.0f / 255.0f, 1.0f)));
			drawList->AddRectFilledMultiColor(ImVec2(kMenuX, y), ImVec2(kMenuX + kMenuWidth, y + kHeaderHeight), headerTL, headerTR, headerBR, headerBL);

			std::string headerText = "TENEBRIS";
			if (m_Level == Level::ConfirmShutdown)
				headerText = Localization::Text("ENCERRAR TENEBRIS");
			else if (m_Level == Level::Categories && m_ActiveSubmenu)
				headerText = Localization::Text(m_ActiveSubmenu->m_Name);
			else if (m_Level == Level::Options && m_ActiveSubmenu && m_ActiveSubmenu->GetActiveCategory())
				headerText = Localization::Text(m_ActiveSubmenu->GetActiveCategory()->m_Name);

			const ImFont* brandFont = m_OptionsFont ? m_OptionsFont : ImGui::GetFont();
			float brandSize = brandFont->FontSize * layout.Scale;
			const float availableWidth = kMenuWidth - 24.0f * layout.Scale;
			ImVec2 brandTextSize = brandFont->CalcTextSizeA(brandSize, FLT_MAX, 0.0f, headerText.c_str());
			if (brandTextSize.x > availableWidth && brandTextSize.x > 1.0f)
			{
				brandSize *= availableWidth / brandTextSize.x;
				brandTextSize = brandFont->CalcTextSizeA(brandSize, FLT_MAX, 0.0f, headerText.c_str());
			}

			const float titleWave = animate ? std::sin(time * 1.9f) : 0.0f;
			const float titleY = y + (kHeaderHeight - brandTextSize.y) * 0.5f + (animate ? titleWave * 0.65f : 0.0f);
			const ImVec2 titlePos(kMenuX + (kMenuWidth - brandTextSize.x) * 0.5f, titleY);
			const int titleGreen = static_cast<int>(229.0f + (animate ? titleWave * 14.0f : 0.0f));
			const ImU32 titleColor = C(IM_COL32(225, std::clamp(titleGreen, 190, 245), 170, 255));
			if (shadows)
				drawList->AddText(brandFont, brandSize, ImVec2(titlePos.x + 1.0f, titlePos.y + 1.0f), C(IM_COL32(3, 6, 2, 150)), headerText.c_str());
			drawList->AddText(brandFont, brandSize, titlePos, titleColor, headerText.c_str());
			y += kHeaderHeight;

			drawList->AddRectFilled(ImVec2(kMenuX, y), ImVec2(kMenuX + kMenuWidth, y + kSubmenuHeight), C(IM_COL32(12, 12, 12, 235)));
			if (m_Level == Level::Root)
				DrawText(drawList, ImVec2(kMenuX + 10.0f * layout.Scale, y + 6.0f * layout.Scale), white, Localization::Text("MENU PRINCIPAL"), layout.Scale);
			const std::string counter = currentCount ? std::format("{} / {}", m_Selected + 1, currentCount) : "0 / 0";
			DrawText(drawList,
			    ImVec2(kMenuX + kMenuWidth - ImGui::CalcTextSize(counter.c_str()).x * layout.Scale - 10.0f * layout.Scale, y + 6.0f * layout.Scale),
			    C(IM_COL32(180, 180, 180, 255)), counter, layout.Scale);
			y += kSubmenuHeight;

			const std::size_t first = currentCount > 0 && m_Selected >= kOptionsPerPage ? m_Selected - kOptionsPerPage + 1 : 0;
			const std::size_t last = std::min(first + kOptionsPerPage, currentCount);
			std::string label;
			std::string value;

			for (std::size_t index = first; index < last; ++index)
			{
				label.clear();
				value.clear();
				if (m_Level == Level::ConfirmShutdown)
					label = Localization::Text(index == 0 ? "Sim" : "Não");
				else if (m_Level == Level::Root)
				{
					if (index < m_Submenus.size())
					{
						label = Localization::Text(m_Submenus[index]->m_Name);
						value = ">";
					}
					else
						label = Localization::Text("Encerrar Tenebris");
				}
				else if (m_Level == Level::Categories)
				{
					if (m_ActiveSubmenu && index < m_ActiveSubmenu->m_Categories.size())
					{
						label = Localization::Text(m_ActiveSubmenu->m_Categories[index]->m_Name);
						value = ">";
					}
				}
				else if (index < currentItems.size())
				{
					label = Localization::Text(currentItems[index]->GetMenuLabel());
					value = Localization::Text(currentItems[index]->GetMenuValue());
				}

				const bool selected = index == m_Selected;
				const ImU32 rowColor = selected ? C(kLightGreen) : C(index % 2 ? IM_COL32(18, 21, 14, 235) : IM_COL32(10, 12, 8, 235));
				drawList->AddRectFilled(ImVec2(kMenuX, y), ImVec2(kMenuX + kMenuWidth, y + kOptionHeight), rowColor);
				if (selected)
					drawList->AddRectFilled(ImVec2(kMenuX, y), ImVec2(kMenuX + 4.0f, y + kOptionHeight), white);

				const float rightWidth = value.empty() ? 0.0f : ImGui::CalcTextSize(value.c_str()).x * layout.Scale;
				const float labelClipRight = std::max(kMenuX + 8.0f, kMenuX + kMenuWidth - rightWidth - 20.0f);
				drawList->PushClipRect(ImVec2(kMenuX + 8.0f, y), ImVec2(labelClipRight, y + kOptionHeight), true);
				DrawText(drawList, ImVec2(kMenuX + 10.0f * layout.Scale, y + 5.0f * layout.Scale), selected ? C(IM_COL32(10, 10, 10, 255)) : white, label, layout.Scale);
				drawList->PopClipRect();
				if (!value.empty())
					DrawText(drawList, ImVec2(kMenuX + kMenuWidth - rightWidth - 10.0f, y + 7.0f * layout.Scale), selected ? C(IM_COL32(10, 10, 10, 255)) : white, value, layout.Scale);
				y += kOptionHeight;
			}

			drawList->AddRectFilled(ImVec2(kMenuX, y), ImVec2(kMenuX + kMenuWidth, y + kFooterHeight), C(IM_COL32(12, 12, 12, 245)));
			drawList->AddLine(ImVec2(kMenuX, y), ImVec2(kMenuX + kMenuWidth, y), C(kLightGreen), 1.0f);
			const std::string runtime = Localization::IsPortuguese() ? std::format("Jogo {}  |  Tenebris v{}", m_GameBuild, m_ModVersion) : std::format("Game {}  |  Tenebris v{}", m_GameBuild, m_ModVersion);
			DrawText(drawList, ImVec2(kMenuX + 10.0f, y + 4.0f * layout.Scale), C(IM_COL32(190, 200, 175, 255)), runtime, layout.Scale * 0.82f);
			DrawText(drawList, ImVec2(kMenuX + 10.0f, y + 21.0f * layout.Scale), C(IM_COL32(155, 165, 145, 255)), Localization::Text("Setas: navegar | Enter: selecionar | Backspace: Voltar"), layout.Scale * 0.72f);
			y += kFooterHeight;

			if (shadows)
				drawList->AddRect(ImVec2(kMenuX - 2.0f, kMenuY - 2.0f), ImVec2(kMenuX + kMenuWidth + 2.0f, y + 2.0f), C(IM_COL32(12, 16, 7, 230)), 6.0f, 0, 4.0f);

			// Side/bottom accent only: intentionally no green line above the Tenebris title.
			drawList->AddLine(ImVec2(kMenuX, kMenuY), ImVec2(kMenuX, y), C(kLightGreen), 1.0f);
			drawList->AddLine(ImVec2(kMenuX + kMenuWidth, kMenuY), ImVec2(kMenuX + kMenuWidth, y), C(kLightGreen), 1.0f);
			drawList->AddLine(ImVec2(kMenuX, y), ImVec2(kMenuX + kMenuWidth, y), C(kLightGreen), 1.0f);
		}

		if (AdvancedEditor::IsOpen())
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
