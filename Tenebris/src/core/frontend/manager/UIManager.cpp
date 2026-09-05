#include "UIManager.hpp"

#include "AdvancedEditor.hpp"
#include "ResponsiveLayout.hpp"
#include "core/commands/Commands.hpp"
#include "game/backend/FiberPool.hpp"
#include "game/backend/ScriptMgr.hpp"
#include "game/frontend/Menu.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace YimMenu
{
	namespace
	{
		constexpr ImU32 kDarkGreen = IM_COL32(34, 48, 11, 255);
		constexpr ImU32 kLightGreen = IM_COL32(52, 77, 14, 255);

		ImU32 ApplyAlpha(ImU32 color, float alpha)
		{
			ImVec4 converted = ImGui::ColorConvertU32ToFloat4(color);
			converted.w *= std::clamp(alpha, 0.0f, 1.0f);
			return ImGui::ColorConvertFloat4ToU32(converted);
		}

		void DrawText(ImDrawList* drawList, const ImVec2& position, ImU32 color, std::string_view text, float scale)
		{
			const ImFont* font = ImGui::GetFont();
			const float size = ImGui::GetFontSize() * scale;
			// Dupla passagem subpixel: preserva a fonte classica do Tenebris, mas a
			// deixa um pouco mais encorpada sem trocar por uma fonte de sistema.
			drawList->AddText(font, size, position, color, text.data(), text.data() + text.size());
			if (scale >= 0.80f)
				drawList->AddText(font, size, ImVec2(position.x + 0.42f, position.y), color, text.data(), text.data() + text.size());
		}

		void DrawWrappedText(ImDrawList* drawList, const ImVec2& position, ImU32 color, std::string_view text, float width, float scale)
		{
			const ImFont* font = ImGui::GetFont();
			const float size = ImGui::GetFontSize() * scale;
			drawList->AddText(font, size, position, color, text.data(), text.data() + text.size(), width);
			drawList->AddText(font, size, ImVec2(position.x + 0.32f, position.y), color, text.data(), text.data() + text.size(), width);
		}

		std::string GetPortugueseDescription(std::string_view description, std::string_view label)
		{
			static constexpr std::array portugueseMarkers = {
			    "Abre", "Ajusta", "Aplica", "Ativa", "Configura", "Cria", "Escolha", "Entrega", "Executa", "Mostra", "Permite", "Retorna", "Seleciona", "Reune"};
			for (const auto marker : portugueseMarkers)
				if (description.contains(marker))
					return std::string(description);
			return std::format("Permite usar ou configurar a opcao '{}'.", label);
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
			m_Level = Level::Root;
			m_Selected = m_Submenus.size();
			return;
		}
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
			const float kDescriptionHeight = layout.DescriptionHeight;
			const std::size_t kOptionsPerPage = layout.OptionsPerPage;
			const auto C = [classicAlpha](ImU32 color) { return ApplyAlpha(color, classicAlpha); };
			const ImU32 white = C(IM_COL32(245, 241, 232, 255));
			float y = kMenuY;

			const float time = static_cast<float>(ImGui::GetTime());
			const float slowPulse = 0.5f + 0.5f * std::sin(time * 1.55f);
			const float sweep = 0.5f + 0.5f * std::sin(time * 0.82f + 1.2f);
			const ImU32 headerLeft = C(ImGui::ColorConvertFloat4ToU32(ImVec4((25.0f + 8.0f * slowPulse) / 255.0f, (39.0f + 12.0f * slowPulse) / 255.0f, 7.0f / 255.0f, 1.0f)));
			const ImU32 headerBright = C(ImGui::ColorConvertFloat4ToU32(ImVec4((61.0f + 20.0f * sweep) / 255.0f, (91.0f + 25.0f * sweep) / 255.0f, (15.0f + 7.0f * sweep) / 255.0f, 1.0f)));
			const ImU32 headerMid = C(ImGui::ColorConvertFloat4ToU32(ImVec4((45.0f + 16.0f * slowPulse) / 255.0f, (67.0f + 22.0f * slowPulse) / 255.0f, 10.0f / 255.0f, 1.0f)));
			drawList->AddRectFilledMultiColor(ImVec2(kMenuX, y), ImVec2(kMenuX + kMenuWidth, y + kHeaderHeight), headerLeft, headerBright, headerMid, headerLeft);
			// Linha luminosa fina reforca o gradient sem deixar o cabecalho carregado.
			const float highlightX = kMenuX + std::fmod(time * 72.0f, kMenuWidth + 90.0f) - 45.0f;
			drawList->AddLine(ImVec2(highlightX, y + 1.0f), ImVec2(std::min(highlightX + 58.0f, kMenuX + kMenuWidth), y + 1.0f), C(IM_COL32(168, 207, 72, 115)), 1.5f);

			const char* letters = "TENEBRIS";
			constexpr int letterCount = 8;
			const ImFont* brandFont = m_OptionsFont ? m_OptionsFont : ImGui::GetFont();
			const float brandSize = brandFont->FontSize * layout.Scale;
			const float letterGap = brandSize * 0.34f;
			float totalWidth = letterGap * static_cast<float>(letterCount - 1);
			std::array<float, letterCount> widths{};
			for (int i = 0; i < letterCount; ++i)
			{
				char glyph[2] = {letters[i], '\0'};
				widths[i] = brandFont->CalcTextSizeA(brandSize, FLT_MAX, 0.0f, glyph).x;
				totalWidth += widths[i];
			}
			float cursorX = kMenuX + (kMenuWidth - totalWidth) * 0.5f;
			const float activeLetter = std::fmod(time * 2.15f, static_cast<float>(letterCount));
			for (int i = 0; i < letterCount; ++i)
			{
				float distance = std::fabs(activeLetter - static_cast<float>(i));
				distance = std::min(distance, static_cast<float>(letterCount) - distance);
				const float envelope = std::exp(-distance * distance * 3.6f);
				const float microX = std::sin(time * 24.0f + i * 2.21f) * 0.72f * envelope;
				const float microY = std::sin(time * 31.0f + i * 1.73f) * 1.05f * envelope;
				const float colorWave = 0.5f + 0.5f * std::sin(time * 1.9f + i * 0.71f);
				const ImVec4 letterColor(
				    (205.0f + 36.0f * colorWave) / 255.0f,
				    (216.0f + 31.0f * colorWave) / 255.0f,
				    (172.0f + 55.0f * colorWave) / 255.0f,
				    1.0f);
				char glyph[2] = {letters[i], '\0'};
				const ImVec2 glyphPos(cursorX + microX, y + kHeaderHeight * 0.34f + microY);
				drawList->AddText(brandFont, brandSize, ImVec2(glyphPos.x + 1.0f, glyphPos.y + 1.0f), C(IM_COL32(3, 6, 2, 150)), glyph);
				drawList->AddText(brandFont, brandSize, glyphPos, C(ImGui::ColorConvertFloat4ToU32(letterColor)), glyph);
				// Pequena segunda passagem somente na letra ativa da onda cria peso sem borrar.
				if (envelope > 0.35f)
					drawList->AddText(brandFont, brandSize, ImVec2(glyphPos.x + 0.38f, glyphPos.y), C(ImGui::ColorConvertFloat4ToU32(letterColor)), glyph);
				cursorX += widths[i] + letterGap;
			}
			y += kHeaderHeight;

			std::string title = "MENU PRINCIPAL";
			if (m_Level == Level::ConfirmShutdown)
				title = "CONFIRMACAO";
			else if (m_Level != Level::Root && m_ActiveSubmenu)
				title = m_Level == Level::Categories ? m_ActiveSubmenu->m_Name : m_ActiveSubmenu->GetActiveCategory()->m_Name;
			drawList->AddRectFilled(ImVec2(kMenuX, y), ImVec2(kMenuX + kMenuWidth, y + kSubmenuHeight), C(IM_COL32(12, 12, 12, 235)));
			DrawText(drawList, ImVec2(kMenuX + 10.0f * layout.Scale, y + 6.0f * layout.Scale), white, title, layout.Scale);
			const std::size_t count = currentCount;
			const std::string counter = count ? std::format("{} / {}", m_Selected + 1, count) : "0 / 0";
			DrawText(drawList,
			    ImVec2(kMenuX + kMenuWidth - ImGui::CalcTextSize(counter.c_str()).x * layout.Scale - 10.0f * layout.Scale,
			        y + 6.0f * layout.Scale),
			    C(IM_COL32(180, 180, 180, 255)),
			    counter,
			    layout.Scale);
			y += kSubmenuHeight;

			std::vector<std::pair<std::string, std::string>> entries;
			entries.reserve(currentCount + (m_Level == Level::Root ? 1 : 0));
			std::string description;
			if (m_Level == Level::ConfirmShutdown)
			{
				entries.emplace_back("Sim", "");
				entries.emplace_back("Nao", "");
				description = "Deseja realmente encerrar? Enter confirma; Backspace cancela.";
			}
			else if (m_Level == Level::Root)
			{
				for (const auto& submenu : m_Submenus)
					entries.emplace_back(submenu->m_Name, ">");
				entries.emplace_back("Encerrar Tenebris", "");
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
					description = GetPortugueseDescription(currentItems[m_Selected]->GetMenuDescription(), currentItems[m_Selected]->GetMenuLabel());
			}

			const std::size_t first = m_Selected >= kOptionsPerPage ? m_Selected - kOptionsPerPage + 1 : 0;
			const std::size_t last = std::min(first + kOptionsPerPage, entries.size());
			for (std::size_t index = first; index < last; ++index)
			{
				const bool selected = index == m_Selected;
				const ImU32 rowColor = selected ? C(kLightGreen) : C(index % 2 ? IM_COL32(18, 21, 14, 235) : IM_COL32(10, 12, 8, 235));
				drawList->AddRectFilled(ImVec2(kMenuX, y), ImVec2(kMenuX + kMenuWidth, y + kOptionHeight), rowColor);
				if (selected)
					drawList->AddRectFilled(ImVec2(kMenuX, y), ImVec2(kMenuX + 4.0f, y + kOptionHeight), white);
				const float rightWidth = ImGui::CalcTextSize(entries[index].second.c_str()).x * layout.Scale;
				const float labelClipRight = std::max(kMenuX + 8.0f, kMenuX + kMenuWidth - rightWidth - 20.0f);
				drawList->PushClipRect(ImVec2(kMenuX + 8.0f, y), ImVec2(labelClipRight, y + kOptionHeight), true);
				DrawText(drawList,
				    ImVec2(kMenuX + 10.0f * layout.Scale, y + 5.0f * layout.Scale),
				    selected ? C(IM_COL32(10, 10, 10, 255)) : white,
				    entries[index].first,
				    layout.Scale);
				drawList->PopClipRect();
				DrawText(drawList,
				    ImVec2(kMenuX + kMenuWidth - rightWidth - 10.0f, y + 7.0f),
				    selected ? C(IM_COL32(10, 10, 10, 255)) : white,
				    entries[index].second,
				    layout.Scale);
				y += kOptionHeight;
			}

			drawList->AddRectFilled(ImVec2(kMenuX, y), ImVec2(kMenuX + kMenuWidth, y + kFooterHeight), C(IM_COL32(12, 12, 12, 245)));
			drawList->AddLine(ImVec2(kMenuX, y), ImVec2(kMenuX + kMenuWidth, y), C(kLightGreen), 1.0f);
			const std::string runtime = std::format("Jogo {}  |  Tenebris v{}", m_GameBuild, m_ModVersion);
			DrawText(drawList, ImVec2(kMenuX + 10.0f, y + 4.0f * layout.Scale), C(IM_COL32(190, 200, 175, 255)), runtime, layout.Scale * 0.82f);
			DrawText(drawList,
			    ImVec2(kMenuX + 10.0f, y + 21.0f * layout.Scale),
			    C(IM_COL32(155, 165, 145, 255)),
			    "Setas: navegar  |  Enter: selecionar  |  Voltar: Backspace",
			    layout.Scale * 0.72f);
			y += kFooterHeight;
			if (!description.empty())
			{
				const float descriptionFontSize = ImGui::GetFontSize() * layout.Scale * 0.86f;
				const float textHeight = ImGui::GetFont()->CalcTextSizeA(descriptionFontSize, FLT_MAX, kMenuWidth - 20.0f, description.data(), description.data() + description.size()).y;
				const float panelHeight = std::max(kDescriptionHeight, textHeight + 16.0f * layout.Scale);
				drawList->AddRectFilled(ImVec2(kMenuX, y), ImVec2(kMenuX + kMenuWidth, y + panelHeight), C(IM_COL32(5, 5, 5, 225)));
				drawList->AddLine(ImVec2(kMenuX, y), ImVec2(kMenuX + kMenuWidth, y), C(IM_COL32(52, 77, 14, 190)), 1.0f);
				DrawWrappedText(drawList,
				    ImVec2(kMenuX + 10.0f, y + 8.0f),
				    C(IM_COL32(220, 214, 201, 255)),
				    description,
				    kMenuWidth - 20.0f,
				    layout.Scale * 0.86f);
				y += panelHeight;
			}
			drawList->AddRect(ImVec2(kMenuX - 2.0f, kMenuY - 2.0f), ImVec2(kMenuX + kMenuWidth + 2.0f, y + 2.0f), C(IM_COL32(12, 16, 7, 230)), 6.0f, 0, 4.0f);
			drawList->AddRect(ImVec2(kMenuX, kMenuY), ImVec2(kMenuX + kMenuWidth, y), C(kLightGreen), 4.0f, 0, 1.0f);
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
