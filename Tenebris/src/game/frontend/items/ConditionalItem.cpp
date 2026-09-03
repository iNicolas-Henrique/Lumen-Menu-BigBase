#include "Items.hpp"
#include "core/commands/BoolCommand.hpp"
#include "core/commands/Command.hpp"
#include "core/commands/Commands.hpp"
#include "core/frontend/widgets/toggle/imgui_toggle.hpp"

namespace YimMenu
{
	ConditionalItem::ConditionalItem(joaat_t bool_cmd_id, std::shared_ptr<UIItem> to_draw) :
	    m_Condition(Commands::GetCommand<BoolCommand>(bool_cmd_id)),
	    m_Item(to_draw)
	{
	}

	void ConditionalItem::Draw()
	{
		if (!m_Condition)
		{
			return;
		}

		if (m_Condition->GetState())
			m_Item->Draw();
	}

	bool ConditionalItem::IsVisible() const
	{
		return m_Condition && m_Condition->GetState();
	}
	void ConditionalItem::CollectMenuItems(std::vector<UIItem*>& items)
	{
		if (IsVisible() && m_Item)
			m_Item->CollectMenuItems(items);
	}
}
