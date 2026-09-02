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
		ImGui::SetNextWindowSize(ImVec2(560.0f, 360.0f), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Editor avancado do Lumen", &open))
		{
			ImGui::TextDisabled("Esta ferramenta ainda utiliza Dear ImGui por exigir entrada complexa.");
			ImGui::Separator();
			g_Item->Draw();
		}
		ImGui::End();

		if (!open)
			g_Item = nullptr;
	}

	bool AdvancedEditor::IsOpen()
	{
		return g_Item != nullptr;
	}
}
