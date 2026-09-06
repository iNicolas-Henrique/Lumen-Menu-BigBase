#include "UIManager.hpp"

#include "AdvancedEditor.hpp"
#include "ResponsiveLayout.hpp"
#include "core/commands/Commands.hpp"
#include "core/frontend/Localization.hpp"
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

		void QueueMenuSound(const char* sound)
		{
			if (!sound || !ScriptMgr::CanTick())
				return;
			FiberPool::Push([sound] {
				AUDIO::PLAY_SOUND_FRONTEND(sound, "HUD_PLAYER_MENU", 1, 0);
			});
		}

		ImVec2 HeaderPerimeterPoint(float distance, float x, float y, float width, float height)
		{
			// The path starts at the upper-right corner and follows exactly the order
			// requested for the Tenebris trace: down -> left -> up -> right.
			constexpr float inset = 1.5f;
			const float left = x + inset;
			const float right = x + width - inset;
			const float top = y + inset;
			const float bottom = y + height - inset;
			const float horizontal = std::max(0.0f, right - left);
			const float vertical = std::max(0.0f, bottom - top);
			const float perimeter = 2.0f * (horizontal + vertical);
			if (perimeter <= 0.0f)
				return {left, top};

			distance = std::fmod(distance, perimeter);
			if (distance < 0.0f)
				distance += perimeter;
			if (distance <= vertical)
				return {right, top + distance};
			distance -= vertical;
			if (distance <= horizontal)
				return {right - distance, bottom};
			distance -= horizontal;
			if (distance <= vertical)
				return {left, bottom - distance};
			distance -= vertical;
			return {left + std::min(distance, horizontal), top};
		}

		void DrawHeaderTrace(ImDrawList* drawList, float time, float x, float y, float width, float height, ImU32 color)
		{
			const float horizontal = std::max(0.0f, width - 3.0f);
			const float vertical = std::max(0.0f, height - 3.0f);
			const float perimeter = 2.0f * (horizontal + vertical);
			if (perimeter <= 1.0f)
				return;

			const float head = std::fmod(time * 78.0f, perimeter);
			const float traceLength = std::min(72.0f, perimeter * 0.16f);
			constexpr int pieces = 24;
			drawList->PushClipRect(ImVec2(x, y), ImVec2(x + width, y + height), true);
			for (int i = 0; i < pieces; ++i)
			{
				const float a = static_cast<float>(i) / pieces;
				const float b = static_cast<float>(i + 1) / pieces;
				const ImVec2 p0 = HeaderPerimeterPoint(head - traceLength + traceLength * a, x, y, width, height);
				const ImVec2 p1 = HeaderPerimeterPoint(head - traceLength + traceLength * b, x, y, width, height);
				ImVec4 c = ImGui::ColorConvertU32ToFloat4(color);
				c.w *= 0.18f + 0.82f * b;
				drawList->AddLine(p0, p1, ImGui::ColorConvertFloat4ToU32(c), 1.6f);
			}
			drawList->PopClipRect();
		}

		std::string UsefulDescription(std::string_view description, std::string_view label)
		{
			if (!description.empty() && description != "Empty" && description != "Abre os controles desta secao.")
				return Localization::Text(description);
			return {};
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
			DrawHeaderTrace(drawList, time, kMenuX, y, kMenuWidth, kHeaderHeight, C(IM_COL32(180, 220, 84, 180)));

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
			ImVec2 naturalSize = brandFont->CalcTextSizeA(brandSize, FLT_MAX, 0.0f, headerText.c_str());
			if (naturalSize.x > availableWidth && naturalSize.x > 1.0f)
				brandSize *= availableWidth / naturalSize.x;
			const float letterGap = brandSize * 0.22f;

			std::vector<float> widths(headerText.size(), 0.0f);
			float totalWidth = 0.0f;
			for (std::size_t i = 0; i < headerText.size(); ++i)
			{
				const unsigned char byte = static_cast<unsigned char>(headerText[i]);
				if ((byte & 0xC0) == 0x80)
					continue;
				const char* begin = headerText.data() + i;
				const char* end = begin + 1;
				while (end < headerText.data() + headerText.size() && (static_cast<unsigned char>(*end) & 0xC0) == 0x80)
					++end;
				widths[i] = brandFont->CalcTextSizeA(brandSize, FLT_MAX, 0.0f, begin, end).x;
				totalWidth += widths[i];
				if (end < headerText.data() + headerText.size())
					totalWidth += letterGap;
			}

			std::vector<std::pair<std::size_t, std::size_t>> glyphRanges;
			for (std::size_t i = 0; i < headerText.size();)
			{
				std::size_t end = i + 1;
				while (end < headerText.size() && (static_cast<unsigned char>(headerText[end]) & 0xC0) == 0x80)
					++end;
				glyphRanges.emplace_back(i, end);
				i = end;
			}

			float cursorX = kMenuX + (kMenuWidth - totalWidth) * 0.5f;
			constexpr float perLetterSeconds = 0.46f;
			constexpr float waitSeconds = 4.0f;
			const float sequenceDuration = static_cast<float>(glyphRanges.size()) * perLetterSeconds;
			const float fullCycle = sequenceDuration + waitSeconds;
			const float cycleTime = fullCycle > 0.0f ? std::fmod(time, fullCycle) : 0.0f;
			const int activeGlyph = cycleTime < sequenceDuration ? static_cast<int>(cycleTime / perLetterSeconds) : -1;

			for (std::size_t glyphIndex = 0; glyphIndex < glyphRanges.size(); ++glyphIndex)
			{
				const auto [beginIndex, endIndex] = glyphRanges[glyphIndex];
				const float glyphWidth = widths[beginIndex];
				float envelope = 0.0f;
				if (activeGlyph == static_cast<int>(glyphIndex))
				{
					const float local = std::clamp((cycleTime - static_cast<float>(glyphIndex) * perLetterSeconds) / perLetterSeconds, 0.0f, 1.0f);
					envelope = std::sin(local * 3.14159265f);
				}
				const float microX = std::sin(time * 24.0f + static_cast<float>(glyphIndex) * 2.21f) * 0.72f * envelope;
				const float microY = std::sin(time * 31.0f + static_cast<float>(glyphIndex) * 1.73f) * 1.05f * envelope;
				const float colorWave = 0.5f + 0.5f * std::sin(time * 1.9f + static_cast<float>(glyphIndex) * 0.71f);
				const ImVec4 letterColor((205.0f + 36.0f * colorWave) / 255.0f, (216.0f + 31.0f * colorWave) / 255.0f, (172.0f + 55.0f * colorWave) / 255.0f, 1.0f);
				const char* glyphBegin = headerText.data() + beginIndex;
				const char* glyphEnd = headerText.data() + endIndex;
				const ImVec2 glyphPos(cursorX + microX, y + kHeaderHeight * 0.34f + microY);
				drawList->AddText(brandFont, brandSize, ImVec2(glyphPos.x + 1.0f, glyphPos.y + 1.0f), C(IM_COL32(3, 6, 2, 150)), glyphBegin, glyphEnd);
				drawList->AddText(brandFont, brandSize, glyphPos, C(ImGui::ColorConvertFloat4ToU32(letterColor)), glyphBegin, glyphEnd);
				if (envelope > 0.35f)
					drawList->AddText(brandFont, brandSize, ImVec2(glyphPos.x + 0.38f, glyphPos.y), C(ImGui::ColorConvertFloat4ToU32(letterColor)), glyphBegin, glyphEnd);
				cursorX += glyphWidth + letterGap;
			}
			y += kHeaderHeight;

			// Keep the slim separator/counter bar, but do not repeat submenu/category
			// names here: those names now live exclusively in the animated header.
			drawList->AddRectFilled(ImVec2(kMenuX, y), ImVec2(kMenuX + kMenuWidth, y + kSubmenuHeight), C(IM_COL32(12, 12, 12, 235)));
			if (m_Level == Level::Root)
				DrawText(drawList, ImVec2(kMenuX + 10.0f * layout.Scale, y + 6.0f * layout.Scale), white, Localization::Text("MENU PRINCIPAL"), layout.Scale);
			const std::size_t count = currentCount;
			const std::string counter = count ? std::format("{} / {}", m_Selected + 1, count) : "0 / 0";
			DrawText(drawList,
			    ImVec2(kMenuX + kMenuWidth - ImGui::CalcTextSize(counter.c_str()).x * layout.Scale - 10.0f * layout.Scale, y + 6.0f * layout.Scale),
			    C(IM_COL32(180, 180, 180, 255)), counter, layout.Scale);
			y += kSubmenuHeight;

			std::vector<std::pair<std::string, std::string>> entries;
			entries.reserve(currentCount + (m_Level == Level::Root ? 1 : 0));
			std::string description;
			if (m_Level == Level::ConfirmShutdown)
			{
				entries.emplace_back(Localization::Text("Sim"), "");
				entries.emplace_back(Localization::Text("Não"), "");
				description = Localization::Text("Deseja realmente encerrar o Tenebris?");
			}
			else if (m_Level == Level::Root)
			{
				for (const auto& submenu : m_Submenus)
					entries.emplace_back(Localization::Text(submenu->m_Name), ">");
				entries.emplace_back(Localization::Text("Encerrar Tenebris"), "");
			}
			else if (m_Level == Level::Categories)
			{
				for (const auto& category : m_ActiveSubmenu->m_Categories)
					entries.emplace_back(Localization::Text(category->m_Name), ">");
			}
			else
			{
				for (UIItem* item : currentItems)
					entries.emplace_back(Localization::Text(item->GetMenuLabel()), Localization::Text(item->GetMenuValue()));
				if (m_Selected < currentItems.size())
					description = UsefulDescription(currentItems[m_Selected]->GetMenuDescription(), currentItems[m_Selected]->GetMenuLabel());
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
				DrawText(drawList, ImVec2(kMenuX + 10.0f * layout.Scale, y + 5.0f * layout.Scale), selected ? C(IM_COL32(10, 10, 10, 255)) : white, entries[index].first, layout.Scale);
				drawList->PopClipRect();
				DrawText(drawList, ImVec2(kMenuX + kMenuWidth - rightWidth - 10.0f, y + 7.0f), selected ? C(IM_COL32(10, 10, 10, 255)) : white, entries[index].second, layout.Scale);
				y += kOptionHeight;
			}

			drawList->AddRectFilled(ImVec2(kMenuX, y), ImVec2(kMenuX + kMenuWidth, y + kFooterHeight), C(IM_COL32(12, 12, 12, 245)));
			drawList->AddLine(ImVec2(kMenuX, y), ImVec2(kMenuX + kMenuWidth, y), C(kLightGreen), 1.0f);
			const std::string runtime = Localization::IsPortuguese() ? std::format("Jogo {}  |  Tenebris v{}", m_GameBuild, m_ModVersion) : std::format("Game {}  |  Tenebris v{}", m_GameBuild, m_ModVersion);
			DrawText(drawList, ImVec2(kMenuX + 10.0f, y + 4.0f * layout.Scale), C(IM_COL32(190, 200, 175, 255)), runtime, layout.Scale * 0.82f);
			DrawText(drawList, ImVec2(kMenuX + 10.0f, y + 21.0f * layout.Scale), C(IM_COL32(155, 165, 145, 255)), Localization::Text("Setas: navegar | Enter: selecionar | Backspace: Voltar"), layout.Scale * 0.72f);
			y += kFooterHeight;

			if (!description.empty())
			{
				const float descriptionFontSize = ImGui::GetFontSize() * layout.Scale * 0.86f;
				const float textHeight = ImGui::GetFont()->CalcTextSizeA(descriptionFontSize, FLT_MAX, kMenuWidth - 20.0f, description.data(), description.data() + description.size()).y;
				const float panelHeight = std::max(kDescriptionHeight, textHeight + 16.0f * layout.Scale);
				drawList->AddRectFilled(ImVec2(kMenuX, y), ImVec2(kMenuX + kMenuWidth, y + panelHeight), C(IM_COL32(5, 5, 5, 225)));
				drawList->AddLine(ImVec2(kMenuX, y), ImVec2(kMenuX + kMenuWidth, y), C(IM_COL32(52, 77, 14, 190)), 1.0f);
				DrawWrappedText(drawList, ImVec2(kMenuX + 10.0f, y + 8.0f), C(IM_COL32(220, 214, 201, 255)), description, kMenuWidth - 20.0f, layout.Scale * 0.86f);
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
