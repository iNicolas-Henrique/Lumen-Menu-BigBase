#include "Items.hpp"
#include "core/commands/Command.hpp"
#include "core/commands/Commands.hpp"
#include "core/commands/FloatCommand.hpp"
#include "core/frontend/widgets/toggle/imgui_toggle.hpp"

namespace YimMenu
{
	FloatCommandItem::FloatCommandItem(joaat_t id, std::optional<std::string> label_override) :
	    m_Command(Commands::GetCommand<FloatCommand>(id)),
	    m_LabelOverride(label_override)
	{
	}

	void FloatCommandItem::Draw()
	{
		if (!m_Command)
		{
			ImGui::Text("Unknown!");
			return;
		}

		auto value = m_Command->GetState();
		auto label = m_LabelOverride.has_value() ? m_LabelOverride.value().c_str() : m_Command->GetLabel().c_str();
		if (!m_Command->GetMinimum().has_value() || !m_Command->GetMaximum().has_value())
		{
			ImGui::SetNextItemWidth(150);
			if (ImGui::InputFloat(label, &value))
			{
				m_Command->SetState(value);
			}
		}
		else
		{
			ImGui::SetNextItemWidth(150);
			if (ImGui::SliderFloat(label, &value, m_Command->GetMinimum().value(), m_Command->GetMaximum().value()))
			{
				m_Command->SetState(value);
			}
		}
	}

	std::string_view FloatCommandItem::GetMenuLabel() const
	{
		return m_LabelOverride ? *m_LabelOverride : m_Command->GetLabel();
	}
	std::string FloatCommandItem::GetMenuValue() const
	{
		return m_Command ? std::format("{:.2f}", m_Command->GetState()) : "";
	}
	std::string_view FloatCommandItem::GetMenuDescription() const
	{
		return m_Command ? m_Command->GetDescription() : std::string_view{};
	}
	void FloatCommandItem::HandleMenuAction(MenuAction action)
	{
		if (!m_Command || action == MenuAction::Enter)
			return;
		const float delta = action == MenuAction::Right ? 0.1f : -0.1f;
		const float minimum = m_Command->GetMinimum().value_or(-FLT_MAX);
		const float maximum = m_Command->GetMaximum().value_or(FLT_MAX);
		m_Command->SetState(std::clamp(m_Command->GetState() + delta, minimum, maximum));
	}
}
