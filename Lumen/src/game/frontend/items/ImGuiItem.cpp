#include "Items.hpp"
#include "game/backend/FiberPool.hpp"

namespace YimMenu
{
	ImGuiItem::ImGuiItem(std::function<void()> callback, std::string label, std::string description) :
	    m_Callback(std::move(callback)),
	    m_Label(std::move(label)),
	    m_Description(std::move(description))
	{
	}

	void ImGuiItem::Draw()
	{
		m_Callback();
	}
}
