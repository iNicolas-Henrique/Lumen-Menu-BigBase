#include "Items.hpp"
#include "core/commands/Command.hpp"
#include "core/commands/Commands.hpp"
#include "core/commands/ListCommand.hpp"
#include "core/frontend/widgets/toggle/imgui_toggle.hpp"

namespace YimMenu
{
	ListCommandItem::ListCommandItem(joaat_t id, std::optional<std::string> label_override) :
	    m_Command(Commands::GetCommand<ListCommand>(id)),
	    m_LabelOverride(label_override)
	{
	}

	void ListCommandItem::Draw()
	{
		if (!m_Command)
		{
			ImGui::Text("Unknown list!");
			return;
		}

		int current_val = m_Command->GetState();
		auto& list = m_Command->GetList();
		const char* largest_string = "";
		std::size_t largest_string_len = 0;

		if (!m_SelectedItem.has_value() || !m_ItemWidth.has_value())
		{
			for (auto& item : list)
			{
				if (item.first == current_val)
				{
					m_SelectedItem = item.second;
				}

				int length = strlen(item.second);
				if (length > largest_string_len)
				{
					largest_string = item.second;
					largest_string_len = length;
				}
			}

			if (!m_SelectedItem.has_value())
				m_SelectedItem = "";

			auto size = ImGui::CalcTextSize(largest_string);
			m_ItemWidth = size.x + 25.0f;
		}

		ImGui::SetNextItemWidth(m_ItemWidth.value());
		if (ImGui::BeginCombo(m_LabelOverride.value_or(m_Command->GetLabel()).c_str(), m_SelectedItem.value().c_str()))
		{
			for (auto& el : list)
			{
				if (ImGui::Selectable(el.second, el.first == current_val))
				{
					current_val = el.first;
					m_Command->SetState(el.first);
				}

				if (el.first == current_val)
				{
					m_SelectedItem = el.second; // just in case
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
	}

	std::string_view ListCommandItem::GetMenuLabel() const
	{
		return m_LabelOverride ? *m_LabelOverride : m_Command->GetLabel();
	}
	std::string ListCommandItem::GetMenuValue() const
	{
		if (!m_Command)
			return {};
		for (const auto& [value, label] : m_Command->GetList())
			if (value == m_Command->GetState())
				return label;
		return {};
	}
	std::string_view ListCommandItem::GetMenuDescription() const
	{
		return m_Command ? m_Command->GetDescription() : std::string_view{};
	}
	void ListCommandItem::HandleMenuAction(MenuAction action)
	{
		if (!m_Command || m_Command->GetList().empty())
			return;
		auto& list = m_Command->GetList();
		auto current = std::find_if(list.begin(), list.end(), [this](const auto& entry) {
			return entry.first == m_Command->GetState();
		});
		std::size_t index = current == list.end() ? 0 : static_cast<std::size_t>(std::distance(list.begin(), current));
		if (action == MenuAction::Left)
			index = index == 0 ? list.size() - 1 : index - 1;
		else
			index = (index + 1) % list.size();
		m_Command->SetState(list[index].first);
	}
}
