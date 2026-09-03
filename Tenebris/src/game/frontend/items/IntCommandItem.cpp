#include "Items.hpp"
#include "core/commands/Command.hpp"
#include "core/commands/Commands.hpp"
#include "core/commands/IntCommand.hpp"
#include "core/frontend/widgets/toggle/imgui_toggle.hpp"

namespace YimMenu
{
	IntCommandItem::IntCommandItem(joaat_t id, std::optional<std::string> label_override) :
	    m_Command(Commands::GetCommand<IntCommand>(id)),
	    m_LabelOverride(label_override)
	{
	}

	void IntCommandItem::Draw()
	{
		if (!m_Command)
		{
			ImGui::Text("Unknown!");
			return;
		}

		int value = m_Command->GetState();
		auto label = m_LabelOverride.has_value() ? m_LabelOverride.value().c_str() : m_Command->GetLabel().c_str();
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::InputInt(label, &value, 1, 1000))
		{
			value = std::clamp(value, m_Command->GetMinimum().value_or(INT_MIN), m_Command->GetMaximum().value_or(INT_MAX));
			m_Command->SetState(value);
		}
		ImGui::TextDisabled("Digite o valor ou use +/- (Ctrl acelera a edicao).");
	}

	std::string_view IntCommandItem::GetMenuLabel() const
	{
		return m_LabelOverride ? *m_LabelOverride : m_Command->GetLabel();
	}
	std::string IntCommandItem::GetMenuValue() const
	{
		return m_Command ? std::to_string(m_Command->GetState()) : "";
	}
	std::string_view IntCommandItem::GetMenuDescription() const
	{
		return m_Command ? m_Command->GetDescription() : std::string_view{};
	}
	void IntCommandItem::HandleMenuAction(MenuAction action)
	{
		if (!m_Command || action == MenuAction::Enter)
			return;
		const int delta = action == MenuAction::Right ? 1 : -1;
		const int minimum = m_Command->GetMinimum().value_or(INT_MIN);
		const int maximum = m_Command->GetMaximum().value_or(INT_MAX);
		const int current = m_Command->GetState();
		if ((delta > 0 && current >= maximum) || (delta < 0 && current <= minimum))
			return;
		m_Command->SetState(current + delta);
	}
}
