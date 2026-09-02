#include "Items.hpp"
#include "core/commands/Commands.hpp"
#include "game/backend/FiberPool.hpp"
#include "game/backend/Players.hpp"
#include "game/commands/PlayerCommand.hpp"

namespace YimMenu
{
	PlayerCommandItem::PlayerCommandItem(joaat_t id, std::optional<std::string> label_override) :
	    m_Command(Commands::GetCommand<PlayerCommand>(id)),
	    m_LabelOverride(label_override)
	{
	}

	void PlayerCommandItem::Draw()
	{
		if (!m_Command)
		{
			ImGui::Text("Unknown!");
			return;
		}

		if (ImGui::Button(m_LabelOverride.has_value() ? m_LabelOverride.value().data() : m_Command->GetLabel().data()))
		{
			FiberPool::Push([this] {
				if (Players::GetSelected().IsValid())
					m_Command->Call(Players::GetSelected());
			});
		}

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip(m_Command->GetDescription().data());
		}
	}

	std::string_view PlayerCommandItem::GetMenuLabel() const
	{
		return m_LabelOverride ? *m_LabelOverride : m_Command->GetLabel();
	}
	std::string_view PlayerCommandItem::GetMenuDescription() const
	{
		return m_Command ? m_Command->GetDescription() : std::string_view{};
	}
	void PlayerCommandItem::HandleMenuAction(MenuAction action)
	{
		if (m_Command && action == MenuAction::Enter)
			FiberPool::Push([this] {
				if (Players::GetSelected().IsValid())
					m_Command->Call(Players::GetSelected());
			});
	}
}
