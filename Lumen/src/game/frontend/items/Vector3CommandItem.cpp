#include "Items.hpp"
#include "core/commands/Command.hpp"
#include "core/commands/Commands.hpp"
#include "core/commands/Vector3Command.hpp"
#include "game/backend/SavedLocations.hpp"
#include "game/backend/Self.hpp"

namespace YimMenu
{
	Vector3CommandItem::Vector3CommandItem(joaat_t id, std::optional<std::string> label_override) :
	    m_Command(Commands::GetCommand<Vector3Command>(id)),
	    m_LabelOverride(label_override)
	{
	}

	void Vector3CommandItem::Draw()
	{
		if (!m_Command)
		{
			ImGui::Text("Unknown!");
			return;
		}

		auto value = m_Command->GetState();
		ImGui::PushID(m_Command);
		ImGui::SetNextItemWidth(180);
		if (ImGui::InputFloat3("##coord_inp", &value.x, "%.1f"))
			m_Command->SetState(value);
		if (Self::GetPed())
		{
			ImGui::SameLine();
			if (ImGui::Button("Current"))
				m_Command->SetState(Self::GetPed().GetPosition());
		}
		ImGui::SameLine();
		if (ImGui::Button("Saved..."))
			ImGui::OpenPopup("##saved");

		if (ImGui::BeginPopup("##saved", ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
		{
			ImGui::Text("Click on a location to select it. Add more at Teleport > Saved");
			InputTextWithHint("##filter", "Search", &m_CurrentFilter).Draw();

			const float max_length = *Pointers.ScreenResY / 3.2;

			// TODO: duplicated code
			ImGui::BeginGroup();
			ImGui::Text("Categories");

			if (ImGui::BeginListBox("##categories", {200, max_length}))
			{
				for (auto& l : SavedLocations::GetAllSavedLocations() | std::ranges::views::keys)
				{
					if (ImGui::Selectable(l.data(), l == m_CurrentCategory))
					{
						m_CurrentCategory = l;
					}

					if (m_CurrentCategory.empty())
					{
						m_CurrentCategory = l;
					}
				}
				ImGui::EndListBox();
			}
			ImGui::EndGroup();
			ImGui::SameLine();
			ImGui::BeginGroup();
			ImGui::Text("Locations");
			if (ImGui::BeginListBox("##saved_locs", {200, max_length}))
			{
				if (SavedLocations::GetAllSavedLocations().find(m_CurrentCategory)
				    != SavedLocations::GetAllSavedLocations().end())
				{
					std::vector<SavedLocation> current_list{};

					if (!m_CurrentFilter.empty())
						current_list = SavedLocations::SavedLocationsFilteredList(m_CurrentFilter);
					else
						current_list = SavedLocations::GetAllSavedLocations().at(m_CurrentCategory);

					for (const auto& l : current_list)
					{
						if (ImGui::Selectable(l.name.data(), false, ImGuiSelectableFlags_AllowDoubleClick))
						{
							m_Command->SetState({l.x, l.y, l.z});
							ImGui::CloseCurrentPopup();
						}

						if (ImGui::IsItemHovered() && l.name.length() > 27)
						{
							ImGui::BeginTooltip();
							ImGui::Text(l.name.data());
							ImGui::EndTooltip();
						}
					}
				}

				ImGui::EndListBox();
			}

			ImGui::EndGroup();

			if (ImGui::Button("Close"))
			{
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		auto& label = m_LabelOverride.has_value() ? m_LabelOverride.value() : m_Command->GetLabel();
		if (!label.empty())
		{
			ImGui::SameLine();
			ImGui::Text(label.c_str());
		}

		ImGui::PopID();
	}

	std::string_view Vector3CommandItem::GetMenuLabel() const
	{
		return m_LabelOverride ? *m_LabelOverride : m_Command->GetLabel();
	}
	std::string Vector3CommandItem::GetMenuValue() const
	{
		if (!m_Command)
			return {};
		const auto value = m_Command->GetState();
		return std::format("{:.0f}, {:.0f}, {:.0f}", value.x, value.y, value.z);
	}
	std::string_view Vector3CommandItem::GetMenuDescription() const
	{
		return m_Command ? m_Command->GetDescription() : std::string_view{};
	}
	void Vector3CommandItem::HandleMenuAction(MenuAction)
	{
	}
}
