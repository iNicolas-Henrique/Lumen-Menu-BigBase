#include "AdvancedEditor.hpp"

#include "ResponsiveLayout.hpp"
#include "UIItem.hpp"
#include "game/frontend/GUI.hpp"

#include <Windows.h>

namespace YimMenu
{
	namespace
	{
		UIItem* g_Item{};
	}

	void AdvancedEditor::Open(UIItem* item)
	{
		if (!item || g_Item == item)
			return;

		if (g_Item)
			g_Item->OnEditorClosed();

		g_Item = item;
		g_Item->OnEditorOpened();
	}

	void AdvancedEditor::Close()
	{
		if (!g_Item)
			return;

		g_Item->OnEditorClosed();
		g_Item = nullptr;
	}

	bool AdvancedEditor::HandleKey(int key)
	{
		if (!g_Item)
			return false;

		// BACK e Esc fecham o editor avancado e devolvem o foco ao menu classico.
		// O WndProc filtra auto-repeat, portanto um toque fecha apenas uma vez.
		if (key == VK_BACK || key == VK_ESCAPE)
		{
			Close();
			return true;
		}

		return g_Item->HandleEditorKey(key);
	}

	void AdvancedEditor::Draw()
	{
		if (!g_Item)
			return;

		bool open = true;
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		if (!viewport || viewport->WorkSize.x < 320.0f || viewport->WorkSize.y < 240.0f)
			return;

		const ImVec2 display = viewport->WorkSize;
		const ImVec2 origin = viewport->WorkPos;
		const float gap = std::clamp(display.x * 0.0125f, 8.0f, 14.0f);

		// Editor lateral compacto. Com o menu classico aberto ele fica a direita;
		// quando o menu principal some, muda automaticamente para a esquerda para
		// liberar a area de visualizacao do personagem/mundo.
		const float desiredWidth = std::clamp(display.x * 0.32f, 300.0f, 410.0f);
		const float editorWidth = std::min(desiredWidth, display.x - gap * 2.0f);
		const float availableHeight = std::max(180.0f, display.y - gap * 2.0f);
		const float desiredHeight = g_Item->GetPreferredEditorHeight();
		const float editorHeight = std::clamp(desiredHeight, 180.0f, availableHeight);
		const bool mainMenuOpen = GUI::IsOpen();
		const ImVec2 editorPosition(
		    mainMenuOpen ? (origin.x + display.x - editorWidth - gap) : (origin.x + gap),
		    origin.y + gap);
		const ImVec2 editorSize(editorWidth, editorHeight);

		ImGui::SetNextWindowPos(editorPosition, ImGuiCond_Always);
		ImGui::SetNextWindowSize(editorSize, ImGuiCond_Always);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 4.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(7.0f, 4.0f));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.025f, 0.04f, 0.02f, 0.95f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.30f, 0.055f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.13f, 0.19f, 0.043f, 1.0f));
		if (ImGui::Begin("##TenebrisAdvancedEditor", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings))
		{
			ImGui::SetWindowFontScale(0.88f);

			ImGui::TextUnformatted("EDITOR AVANCADO DO TENEBRIS");
			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize("X").x - 8.0f);
			if (ImGui::SmallButton("X"))
				open = false;
			ImGui::Separator();
			ImGui::TextDisabled("%s", g_Item->GetMenuLabel().data());
			ImGui::SameLine();
			ImGui::TextDisabled("| BACK: voltar");
			ImGui::Spacing();
			if (ImGui::BeginChild("##TenebrisAdvancedContent", ImVec2(0.0f, 0.0f), false))
			{
				ImGui::PushItemWidth(-FLT_MIN);
				g_Item->Draw();
				ImGui::PopItemWidth();
			}
			ImGui::EndChild();
		}
		ImGui::End();
		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar(5);

		if (!open)
			Close();
	}

	bool AdvancedEditor::IsOpen()
	{
		return g_Item != nullptr;
	}
}
