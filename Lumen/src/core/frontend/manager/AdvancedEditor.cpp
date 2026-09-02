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
		const ImVec2 display = ImGui::GetIO().DisplaySize;
		const auto layout = GetResponsiveMenuLayout();
		const float gap = std::max(8.0f, 10.0f * layout.Scale);
		const float editorX = layout.X + layout.Width + gap;
		const bool fitsBesideMenu = display.x - editorX >= 280.0f;
		const ImVec2 editorPosition = fitsBesideMenu ? ImVec2(editorX, layout.Y) : ImVec2(layout.X, layout.Y);
		const float availableWidth = fitsBesideMenu ? display.x - editorX - layout.X : display.x - layout.X * 2.0f;
		const ImVec2 editorSize(std::clamp(availableWidth, 260.0f, 560.0f), std::clamp(display.y - layout.Y * 2.0f, 220.0f, 420.0f));
		ImGui::SetNextWindowPos(editorPosition, ImGuiCond_Always);
		ImGui::SetNextWindowSize(editorSize, ImGuiCond_Always);
		ImGui::SetNextWindowSizeConstraints(ImVec2(260.0f, 220.0f),
		    ImVec2(std::max(260.0f, display.x - layout.X * 2.0f), std::max(220.0f, display.y - layout.Y * 2.0f)));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
		if (ImGui::Begin("Editor avancado do Lumen", &open, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings))
		constexpr ImVec2 editorPosition(410.0f, 82.0f);
		ImGui::SetNextWindowPos(editorPosition, ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(560.0f, 360.0f), ImGuiCond_FirstUseEver);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
		if (ImGui::Begin("Editor avancado do Lumen", &open, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse))
		{
			ImGui::TextDisabled("Esta ferramenta ainda utiliza Dear ImGui por exigir entrada complexa.");
			ImGui::Separator();
			g_Item->Draw();
		}
		ImGui::End();
		ImGui::PopStyleVar(2);

		if (!open)
			g_Item = nullptr;
	}

	bool AdvancedEditor::IsOpen()
	{
		return g_Item != nullptr;
	}
}
