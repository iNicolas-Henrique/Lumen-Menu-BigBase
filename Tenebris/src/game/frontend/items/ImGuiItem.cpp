#include "Items.hpp"
#include "game/backend/FiberPool.hpp"

namespace YimMenu
{
	ImGuiItem::ImGuiItem(std::function<void()> callback, std::string label, std::string description, float preferred_editor_height) :
	    m_Callback(std::move(callback)),
	    m_Label(std::move(label)),
	    m_Description(std::move(description)),
	    m_PreferredEditorHeight(preferred_editor_height)
	{
	}

	void ImGuiItem::Draw()
	{
		m_Callback();
	}
}
