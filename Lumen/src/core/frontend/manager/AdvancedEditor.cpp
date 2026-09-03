#include "AdvancedEditor.hpp"

#include "ResponsiveLayout.hpp"
#include "UIItem.hpp"

namespace YimMenu
{
	namespace
	{
		UIItem* g_Item{};
	}

	void AdvancedEditor::Open(UIItem* item)
	{
		g_Item = item;
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
		const auto layout = GetResponsiveMenuLayout();
		const float gap = std::max(8.0f, 10.0f * layout.Scale);
		const float editorX = layout.X + layout.Width + gap;
		const float viewportRight = origin.x + display.x;
		const bool fitsBesideMenu = viewportRight - editorX >= 280.0f;
		const ImVec2 editorPosition = fitsBesideMenu ? ImVec2(editorX, layout.Y) : ImVec2(origin.x + gap, origin.y + gap);
		const float availableWidth = fitsBesideMenu ? viewportRight - editorX - gap : display.x - gap * 2.0f;
		const float availableHeight = display.y - (editorPosition.y - origin.y) - gap;
		const float desiredHeight = g_Item->GetPreferredEditorHeight();
		const ImVec2 editorSize(std::clamp(availableWidth, 260.0f, 520.0f), std::clamp(desiredHeight, 180.0f, availableHeight));
		ImGui::SetNextWindowPos(editorPosition, ImGuiCond_Always);
		ImGui::SetNextWindowSize(editorSize, ImGuiCond_Always);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 4.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(7.0f, 4.0f));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.025f, 0.04f, 0.02f, 0.97f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.30f, 0.055f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.13f, 0.19f, 0.043f, 1.0f));
		if (ImGui::Begin("##LumenAdvancedEditor", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings))
		{
			ImGui::SetWindowFontScale(0.88f);

			// Backspace fecha apenas o editor avancado. Se o usuario estiver digitando
			// em um campo de texto, a tecla continua funcionando normalmente no campo.
			if (!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Backspace, false))
				open = false;

			ImGui::TextUnformatted("EDITOR AVANCADO DO LUMEN");
			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize("X").x - 8.0f);
			if (ImGui::SmallButton("X"))
				open = false;
			ImGui::Separator();
			ImGui::TextDisabled("%s", g_Item->GetMenuLabel().data());
			ImGui::Spacing();
			if (ImGui::BeginChild("##LumenAdvancedContent", ImVec2(0.0f, 0.0f), false))
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
			g_Item = nullptr;
	}

	bool AdvancedEditor::IsOpen()
	{
		return g_Item != nullptr;
	}
}
