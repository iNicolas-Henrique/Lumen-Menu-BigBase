#include "Category.hpp"

namespace YimMenu
{
	void Category::Draw()
	{
		for (auto& item : m_Items)
			item->Draw();
	}

	int Category::GetLength()
	{
		if (m_Length.has_value())
			return m_Length.value();

		m_Length = static_cast<int>(std::max(ImGui::CalcTextSize(m_Name.c_str()).x + 28.0f, 86.0f));
		return m_Length.value();
	}
}
