#include "HorseBonding.hpp"

#include "core/frontend/Localization.hpp"
#include "core/frontend/Notifications.hpp"
#include "core/frontend/manager/Category.hpp"
#include "core/frontend/manager/Submenu.hpp"
#include "game/backend/FiberPool.hpp"
#include "game/frontend/items/Items.hpp"
#include "game/rdr/Natives.hpp"

namespace YimMenu::Submenus
{
	namespace
	{
		constexpr int kBondingAttribute = 7; // ePedAttribute::PA_BONDING
		constexpr int kMaxBondingLevel = 4;

		Ped ResolvePlayerHorse()
		{
			const Ped playerPed = PLAYER::PLAYER_PED_ID();
			if (ENTITY::DOES_ENTITY_EXIST(playerPed) && PED::IS_PED_ON_MOUNT(playerPed))
				return PED::GET_MOUNT(playerPed);
			return PLAYER::_GET_SADDLE_HORSE_FOR_PLAYER(PLAYER::PLAYER_ID());
		}

		void MaximizeHorseBonding()
		{
			FiberPool::Push([] {
				const Ped horse = ResolvePlayerHorse();
				if (!ENTITY::DOES_ENTITY_EXIST(horse) || ENTITY::IS_ENTITY_DEAD(horse))
				{
					Notifications::Show("Tenebris",
					    Localization::IsPortuguese() ? "Nenhum cavalo montado ou cavalo de sela disponível." : "No mounted or saddle horse is available.",
					    NotificationType::Error);
					return;
				}

				// A native dedicada atualiza o nível de vínculo do mount; o atributo
				// PA_BONDING mantém o rank correspondente coerente no próprio PED.
				PED::_SET_MOUNT_BONDING_LEVEL(horse, kMaxBondingLevel);
				ATTRIBUTE::SET_ATTRIBUTE_BASE_RANK(horse, kBondingAttribute, kMaxBondingLevel);

				Notifications::Show("Tenebris",
				    Localization::IsPortuguese() ? "Vínculo do cavalo maximizado para o nível 4." : "Horse bonding maximized to level 4.",
				    NotificationType::Success,
				    3000);
			});
		}
	}

	void InstallHorseBonding(const std::shared_ptr<Submenu>& selfSubmenu)
	{
		if (!selfSubmenu)
			return;

		auto item = std::make_shared<ImGuiItem>([] {
			ImGui::TextWrapped("%s", Localization::IsPortuguese()
			    ? "Maximiza imediatamente o vínculo do cavalo para o nível 4. Se você estiver montado, usa a montaria atual; caso contrário, tenta usar seu cavalo de sela."
			    : "Immediately maximizes horse bonding to level 4. If mounted, it uses the current mount; otherwise it tries your saddle horse.");
			ImGui::Separator();
			if (ImGui::Button(Localization::IsPortuguese() ? "MAXIMIZAR VÍNCULO - NÍVEL 4" : "MAX BONDING - LEVEL 4", ImVec2(-FLT_MIN, 0.0f)))
				MaximizeHorseBonding();
		},
		    "Maximizar vínculo",
		    "Maximiza o vínculo da montaria atual ou do cavalo de sela para o nível 4.",
		    260.0f);

		for (auto& category : selfSubmenu->m_Categories)
		{
			if (category && category->m_Name == "Cavalo")
			{
				category->AddItem(std::move(item));
				return;
			}
		}

		auto category = std::make_shared<Category>("Cavalo");
		category->AddItem(std::move(item));
		selfSubmenu->AddCategory(std::move(category));
	}
}
