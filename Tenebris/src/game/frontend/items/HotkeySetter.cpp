#include "Items.hpp"
#include "core/commands/Command.hpp"
#include "core/commands/Commands.hpp"
#include "core/commands/HotkeySystem.hpp"
#include "core/frontend/Localization.hpp"

namespace YimMenu
{
	HotkeySetter::HotkeySetter(joaat_t command_id) :
	    m_Id(command_id)
	{
	}

	void HotkeySetter::Draw()
	{
		auto Command = Commands::GetCommand(m_Id);

		if (!Command)
			ImGui::Text("%s", Localization::IsPortuguese() ? "Comando desconhecido" : "Unknown command");
		else
		{
			CommandLink* CommandHotkeyLink = &g_HotkeySystem.m_CommandHotkeys.at(Command->GetHash());

			if (!CommandHotkeyLink)
			{
				ImGui::Text("%s", Localization::IsPortuguese() ? "Atalho de comando desconhecido" : "Unknown command link");
			}
			else
			{
				const std::string commandLabel = Localization::Text(Command->GetLabel());
				ImGui::Button(commandLabel.c_str());
				CommandHotkeyLink->m_BeingModified = ImGui::IsItemActive();

				if (CommandHotkeyLink->m_BeingModified)
				{
					g_HotkeySystem.CreateHotkey(CommandHotkeyLink->m_Chain);
				}

				ImGui::SameLine(200);
				ImGui::BeginGroup();

				if (CommandHotkeyLink->m_Chain.empty())
				{
					if (CommandHotkeyLink->m_BeingModified)
						ImGui::Text("%s", Localization::IsPortuguese() ? "Pressione uma tecla..." : "Press any key...");
					else
						ImGui::Text("%s", Localization::IsPortuguese() ? "Nenhum atalho definido" : "No hotkey assigned");
				}
				else
				{
					ImGui::PushItemWidth(35);
					for (auto HotkeyModifier : CommandHotkeyLink->m_Chain)
					{
						char KeyLabel[32];
						strcpy(KeyLabel, g_HotkeySystem.GetHotkeyLabel(HotkeyModifier).data());
						ImGui::InputText("##keylabel", KeyLabel, 32, ImGuiInputTextFlags_ReadOnly);
						if (ImGui::IsItemClicked())
							std::erase_if(CommandHotkeyLink->m_Chain, [HotkeyModifier](int i) {
								return i == HotkeyModifier;
							});

						ImGui::SameLine();
					}
					ImGui::PopItemWidth();

					ImGui::SameLine();
					if (ImGui::Button(Localization::IsPortuguese() ? "Limpar" : "Clear"))
					{
						CommandHotkeyLink->m_Chain.clear();
					}
				}

				ImGui::EndGroup();
			}
		}
	}
}
