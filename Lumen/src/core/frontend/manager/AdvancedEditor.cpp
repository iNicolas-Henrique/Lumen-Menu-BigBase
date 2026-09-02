#include "AdvancedEditor.hpp"

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
