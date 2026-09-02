#include "Info.hpp"

#include "core/frontend/Notifications.hpp"
#include "game/backend/FiberPool.hpp"
#include "game/backend/PlayerDatabase.hpp"
#include "game/backend/Players.hpp"
#include "game/backend/Self.hpp"
#include "game/features/Features.hpp"
#include "game/rdr/Natives.hpp"
#include "game/rdr/Network.hpp"

#include <network/rlGamerInfo.hpp>

namespace YimMenu::Submenus
{
	std::string BuildIPStr(int field1, int field2, int field3, int field4)
	{
		std::ostringstream oss;
		oss << field1 << '.' << field2 << '.' << field3 << '.' << field4;
		return oss.str();
	}

	std::shared_ptr<Category> BuildInfoMenu()
	{
		auto menu = std::make_shared<Category>("Informacoes");

		auto teleportGroup = std::make_shared<Group>("Teleporte");
		auto playerOptionsGroup = std::make_shared<Group>("Informacoes");

		playerOptionsGroup->AddItem(std::make_shared<ImGuiItem>([] {
			if (Players::GetSelected().IsValid())
			{
				ImGui::Text(Players::GetSelected().GetName());
				ImGui::Checkbox("Observar", &YimMenu::g_Spectating);
				ImGui::Checkbox("Bloquear explosoes", &Players::GetSelected().GetData().m_BlockExplosions);
				ImGui::Checkbox("Bloquear particulas", &Players::GetSelected().GetData().m_BlockParticles);
				if (ImGui::Checkbox("Modo fantasma", &Players::GetSelected().GetData().m_GhostMode))
				{
					if (Players::GetSelected().GetData().m_GhostMode)
					{
						FiberPool::Push([] {
							if (Players::GetSelected().IsValid())
							{
								Network::ForceRemoveNetworkEntity(Self::GetPed().GetNetworkObject(), false, Players::GetSelected());
							}
						});
					}
				}

				ImGui::Text("Nivel: %d", Players::GetSelected().GetRank());

				if (Players::GetSelected().GetPed())
				{
					auto health = Players::GetSelected().GetPed().GetHealth();
					auto maxHealth = Players::GetSelected().GetPed().GetMaxHealth();
					const float healthPercent = maxHealth > 0 ? static_cast<float>(health) / maxHealth * 100.0f : 0.0f;
					std::string healthStr = std::format("Vida: {}/{} ({:.2f}%)", health, maxHealth, healthPercent);
					ImGui::Text("%s", healthStr.c_str());

					auto coords = Players::GetSelected().GetPed().GetPosition();
					ImGui::Text("Coordenadas: %.2f, %.2f, %.2f", coords.x, coords.y, coords.z);

					auto distance = Players::GetSelected().GetPed().GetPosition().GetDistance(Self::GetPed().GetPosition());
					ImGui::Text("Distancia: %.2f", distance);
				}
				else
				{
					ImGui::Text("Personagem ausente ou removido");
				}

				auto rid = Players::GetSelected().GetGamerInfo()->m_GamerHandle.m_RockstarId;
				auto rid1 = Players::GetSelected().GetRID();
				bool spoofedRid = (rid != rid1);

				if (!spoofedRid)
				{
					std::string ridStr = std::to_string(rid1);

					ImGui::Text("RID:");
					ImGui::SameLine();
					if (ImGui::Button(std::to_string(rid1).c_str()))
					{
						ImGui::SetClipboardText(std::to_string(rid1).c_str());
					}
				}
				else
				{
					std::string spoofedRidStr = std::to_string(rid);
					std::string ridStr = std::to_string(rid1);

					ImGui::Text("RID mascarado:");
					ImGui::SameLine();
					if (ImGui::Button(spoofedRidStr.c_str()))
					{
						ImGui::SetClipboardText(spoofedRidStr.c_str());
					}

					ImGui::Text("RID real:");
					ImGui::SameLine();
					if (ImGui::Button(ridStr.c_str()))
					{
						ImGui::SetClipboardText(ridStr.c_str());
					}
				}

				auto ip = Players::GetSelected().GetExternalAddress();
				auto ip2 = Players::GetSelected().GetAddress()->m_external_ip;
				auto ip1 = Players::GetSelected().GetGamerInfo()->m_ExternalAddress;
				bool spoofedIp = (ip.m_packed != ip1.m_packed);

				auto addr2 = BuildIPStr(ip2.m_field1, ip2.m_field2, ip2.m_field3, ip2.m_field4);

				ImGui::Text("IP do endpoint:");
				ImGui::SameLine();
				if (ImGui::Button(addr2.c_str()))
				{
					ImGui::SetClipboardText(addr2.c_str());
				}

				if (!spoofedIp)
				{
					auto ipStr = BuildIPStr(ip.m_field1, ip.m_field2, ip.m_field3, ip.m_field4);

					ImGui::Text("Endereco IP:");
					ImGui::SameLine();
					if (ImGui::Button(ipStr.c_str()))
					{
						ImGui::SetClipboardText(ipStr.c_str());
					}
				}
				else
				{
					auto spoofedIpStr = BuildIPStr(ip1.m_field1, ip1.m_field2, ip1.m_field3, ip1.m_field4);
					auto realIpStr = BuildIPStr(ip.m_field1, ip.m_field2, ip.m_field3, ip.m_field4);

					ImGui::Text("IP mascarado:");
					ImGui::SameLine();
					if (ImGui::Button(spoofedIpStr.c_str()))
					{
						ImGui::SetClipboardText(spoofedIpStr.c_str());
					}

					ImGui::Text("IP real:");
					ImGui::SameLine();
					if (ImGui::Button(realIpStr.c_str()))
					{
						ImGui::SetClipboardText(realIpStr.c_str());
					}
				}

				if (ImGui::Button("Ver perfil Social Club"))
					FiberPool::Push([] {
						uint64_t handle[18];
						NETWORK::NETWORK_HANDLE_FROM_PLAYER(Players::GetSelected().GetId(), (Any*)&handle);
						NETWORK::NETWORK_SHOW_PROFILE_UI((Any*)&handle);
					});
				ImGui::SameLine();
				if (ImGui::Button("Adicionar amigo"))
					FiberPool::Push([] {
						uint64_t handle[18];
						NETWORK::NETWORK_HANDLE_FROM_PLAYER(Players::GetSelected().GetId(), (Any*)&handle);
						NETWORK::NETWORK_ADD_FRIEND((Any*)&handle, "");
					});
				ImGui::SameLine();
				if (ImGui::Button("Adicionar ao banco"))
				{
					auto plyr = Players::GetSelected();
					g_PlayerDatabase->AddPlayer(plyr.GetRID(), plyr.GetName());
				}

				if (ImGui::Button("Mais informacoes"))
					ImGui::OpenPopup("Mais informacoes");

				ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
				if (ImGui::BeginPopupModal("Mais informacoes", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_Modal | ImGuiWindowFlags_AlwaysAutoResize))
				{
					ImGui::Text("Idioma: %s", g_LanguageMap[Players::GetSelected().GetLanguage()].c_str());

					auto honor = Players::GetSelected().GetHonor();
					std::string honorLevel;

					if (honor >= 0 && honor <= 7)
					{
						honorLevel = "Baixa";
					}
					else if (honor > 7 && honor <= 10)
					{
						honorLevel = "Moderada";
					}
					else if (honor > 10 && honor <= 15)
					{
						honorLevel = "Alta";
					}
					else
					{
						honorLevel = "Invalida";
					}

					honorLevel += " (" + std::to_string(honor) + "/15)";
					ImGui::Text("Honra: %s", honorLevel.c_str());

					std::string model = std::format("0x{:08X}", (joaat_t)Players::GetSelected().GetPed().GetModel());
					ImGui::Text("Modelo: %s", model.c_str());
					ImGui::SameLine();
					if (ImGui::Button("Copiar"))
						ImGui::SetClipboardText(model.c_str());

					if (auto it = g_DistrictMap.find(Players::GetSelected().GetDistrict()); it != g_DistrictMap.end())
						ImGui::Text("Distrito: %s", it->second.c_str());

					if (auto it = g_RegionMap.find(Players::GetSelected().GetRegion()); it != g_RegionMap.end())
						ImGui::Text("Regiao: %s", it->second.c_str());

					auto internalIp = Players::GetSelected().GetInternalAddress();
					ImGui::Text("IP interno: %s",
					    std::format("{}.{}.{}.{}:{}",
					        static_cast<int>(internalIp.m_field1),
					        static_cast<int>(internalIp.m_field2),
					        static_cast<int>(internalIp.m_field3),
					        static_cast<int>(internalIp.m_field4),
					        Players::GetSelected().GetInternalPort())
					        .c_str());

					auto relayIp = Players::GetSelected().GetRelayAddress();
					ImGui::Text("IP de retransmissao: %s",
					    std::format("{}.{}.{}.{}:{}",
					        static_cast<int>(relayIp.m_field1),
					        static_cast<int>(relayIp.m_field2),
					        static_cast<int>(relayIp.m_field3),
					        static_cast<int>(relayIp.m_field4),
					        Players::GetSelected().GetRelayPort())
					        .c_str());


					ImGui::Text("Tipo de conexao: %u", Players::GetSelected().GetConnectionType());

					ImGui::Text("Latencia media: %.2f", Players::GetSelected().GetAverageLatency());
					ImGui::Text("Perda de pacotes: %.2f", Players::GetSelected().GetAveragePacketLoss());

					ImGui::Spacing();

					if (ImGui::Button("Fechar") || ((!ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered()) && ImGui::IsMouseClicked(ImGuiMouseButton_Left)))
						ImGui::CloseCurrentPopup();

					ImGui::EndPopup();
				}
			}
			else
			{
				Players::SetSelected(Self::GetPlayer());
				ImGui::Text("Nenhum jogador disponivel");
			}
		}));

		teleportGroup->AddItem(std::make_shared<PlayerCommandItem>("tptoplayer"_J));
		teleportGroup->AddItem(std::make_shared<PlayerCommandItem>("tptoplayercamp"_J));
		teleportGroup->AddItem(std::make_shared<PlayerCommandItem>("tpbehindplayer"_J));
		teleportGroup->AddItem(std::make_shared<PlayerCommandItem>("tpintovehicle"_J));
		teleportGroup->AddItem(std::make_shared<PlayerCommandItem>("bring"_J));
		teleportGroup->AddItem(std::make_shared<PlayerCommandItem>("tpplayertowaypoint"_J));
		teleportGroup->AddItem(std::make_shared<PlayerCommandItem>("tpplayertomadamnazar"_J));
		teleportGroup->AddItem(std::make_shared<PlayerCommandItem>("tpplayertojail"_J));

		menu->AddItem(playerOptionsGroup);
		menu->AddItem(teleportGroup);

		return menu;
	}
}
