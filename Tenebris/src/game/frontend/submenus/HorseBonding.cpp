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
		constexpr int kMaxBondingPoints = 2450;
		constexpr int kExpectedMaxBondingLevel = 4;

		int ResolvePlayerHorse()
		{
			const int playerPed = PLAYER::PLAYER_PED_ID();
			if (ENTITY::DOES_ENTITY_EXIST(playerPed) && PED::IS_PED_ON_MOUNT(playerPed))
				return PED::GET_MOUNT(playerPed);
			return PLAYER::_GET_SADDLE_HORSE_FOR_PLAYER(PLAYER::PLAYER_ID());
		}

		void MaximizeHorseBonding()
		{
			FiberPool::Push([] {
				const int horse = ResolvePlayerHorse();
				if (!ENTITY::DOES_ENTITY_EXIST(horse) || ENTITY::IS_ENTITY_DEAD(horse))
				{
					Notifications::Show("Tenebris",
					    Localization::IsPortuguese() ? "Nenhum cavalo montado ou cavalo de sela disponível." : "No mounted or saddle horse is available.",
					    NotificationType::Error);
					return;
				}

				// O jogo trata PA_BONDING (atributo 7) por pontos. Primeiro colocamos
				// os pontos no teto conhecido, depois deixamos o próprio sistema de
				// atributos calcular o rank e sincronizamos a native de bonding.
				ATTRIBUTE::SET_ATTRIBUTE_POINTS(horse, kBondingAttribute, kMaxBondingPoints);
				const int bondingLevel = ATTRIBUTE::GET_ATTRIBUTE_RANK(horse, kBondingAttribute);
				PED::_SET_MOUNT_BONDING_LEVEL(horse, bondingLevel);

				if (bondingLevel < kExpectedMaxBondingLevel)
				{
					Notifications::Show("Tenebris",
					    Localization::IsPortuguese()
					        ? "Os pontos de vínculo foram maximizados, mas o jogo ainda não reportou nível 4."
					        : "Bonding points were maximized, but the game has not reported level 4 yet.",
					    NotificationType::Warning,
					    3500);
					return;
				}

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
