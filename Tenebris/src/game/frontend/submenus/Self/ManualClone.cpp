#include "ManualClone.hpp"

#include "core/frontend/Localization.hpp"
#include "core/frontend/Notifications.hpp"
#include "core/frontend/manager/UIItem.hpp"
#include "game/backend/FiberPool.hpp"
#include "game/backend/Self.hpp"
#include "game/rdr/Natives.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace YimMenu::Submenus
{
	namespace
	{
		struct CloneWeapon
		{
			const char* LabelPt;
			const char* LabelEn;
			const char* HashName;
		};

		constexpr std::array kCloneWeapons = {
		    CloneWeapon{"Desarmado", "Unarmed", "WEAPON_UNARMED"},
		    CloneWeapon{"Faca", "Knife", "WEAPON_MELEE_KNIFE"},
		    CloneWeapon{"Facão", "Machete", "WEAPON_MELEE_MACHETE"},
		    CloneWeapon{"Machadinha", "Hatchet", "WEAPON_MELEE_HATCHET"},
		    CloneWeapon{"Revólver Cattleman", "Cattleman Revolver", "WEAPON_REVOLVER_CATTLEMAN"},
		    CloneWeapon{"Revólver Double-Action", "Double-Action Revolver", "WEAPON_REVOLVER_DOUBLEACTION"},
		    CloneWeapon{"Pistola M1899", "M1899 Pistol", "WEAPON_PISTOL_M1899"},
		    CloneWeapon{"Carabina", "Carbine Repeater", "WEAPON_REPEATER_CARBINE"},
		    CloneWeapon{"Escopeta serrada", "Sawed-Off Shotgun", "WEAPON_SHOTGUN_SAWEDOFF"},
		    CloneWeapon{"Rifle Bolt Action", "Bolt Action Rifle", "WEAPON_RIFLE_BOLTACTION"},
		    CloneWeapon{"Arco", "Bow", "WEAPON_BOW"},
		};

		void SpawnManualClone(int weaponIndex)
		{
			Ped self = Self::GetPed();
			if (!self.IsValid())
				return;

			const int selfHandle = self.GetHandle();
			const Vector3 selfPos = self.GetPosition();
			int clone = PED::CLONE_PED(selfHandle, false, false, true);
			if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone))
			{
				Notifications::Show("Tenebris", Localization::IsPortuguese() ? "Não foi possível criar o clone." : "Could not create the clone.", NotificationType::Warning, 2400);
				return;
			}

			ENTITY::SET_ENTITY_AS_MISSION_ENTITY(clone, true, true);
			const float heading = ENTITY::GET_ENTITY_HEADING(selfHandle);
			const float radians = heading * 0.017453292519943295f;
			const Vector3 spawn{
			    selfPos.x - std::sin(radians) * 2.2f + std::cos(radians) * 1.4f,
			    selfPos.y - std::cos(radians) * 2.2f - std::sin(radians) * 1.4f,
			    selfPos.z + 0.3f};
			ENTITY::SET_ENTITY_COORDS_NO_OFFSET(clone, spawn.x, spawn.y, spawn.z, true, true, true);
			ENTITY::PLACE_ENTITY_ON_GROUND_PROPERLY(clone, true);
			ENTITY::SET_ENTITY_MAX_HEALTH(clone, 800);
			ENTITY::SET_ENTITY_HEALTH(clone, 800, 0);

			weaponIndex = std::clamp(weaponIndex, 0, static_cast<int>(kCloneWeapons.size()) - 1);
			WEAPON::REMOVE_ALL_PED_WEAPONS(clone, true, true);
			const Hash weapon = Joaat(kCloneWeapons[weaponIndex].HashName);
			if (weapon != Joaat("WEAPON_UNARMED"))
				WEAPON::GIVE_WEAPON_TO_PED(clone, weapon, 250, true, true, 0, false, 0.5f, 1.0f, 1.0f, false, 0, false);
			WEAPON::SET_CURRENT_PED_WEAPON(clone, weapon, true, 0, false, false);

			PED::SET_PED_KEEP_TASK(clone, true);
			PED::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(clone, false);
			PED::SET_PED_COMBAT_ABILITY(clone, 3);
			PED::SET_PED_COMBAT_MOVEMENT(clone, 2);
			PED::SET_PED_ACCURACY(clone, 72);
			PED::SET_PED_MOVE_RATE_OVERRIDE(clone, 1.15f);
			TASK::TASK_FOLLOW_TO_OFFSET_OF_ENTITY(clone, selfHandle, 1.5f, -2.0f, 0.0f, 1.2f, -1, 2.0f, true, true, false, true, true, true);

			Notifications::Show("Tenebris", Localization::IsPortuguese() ? "Clone criado." : "Clone created.", NotificationType::Success, 2000);
		}

		class ManualCloneItem final : public UIItem
		{
			int m_WeaponIndex{};

		public:
			void Draw() override
			{
				ImGui::TextUnformatted(Localization::IsPortuguese() ? "Criar meu clone" : "Create my clone");
				ImGui::Spacing();
				ImGui::TextUnformatted(Localization::IsPortuguese() ? "Arma do clone" : "Clone weapon");
				const char* preview = Localization::IsPortuguese() ? kCloneWeapons[m_WeaponIndex].LabelPt : kCloneWeapons[m_WeaponIndex].LabelEn;
				if (ImGui::BeginCombo("##ManualCloneWeapon", preview))
				{
					for (int i = 0; i < static_cast<int>(kCloneWeapons.size()); ++i)
					{
						const char* label = Localization::IsPortuguese() ? kCloneWeapons[i].LabelPt : kCloneWeapons[i].LabelEn;
						if (ImGui::Selectable(label, m_WeaponIndex == i))
							m_WeaponIndex = i;
					}
					ImGui::EndCombo();
				}
				ImGui::Spacing();
				if (ImGui::Button(Localization::IsPortuguese() ? "CRIAR CLONE" : "CREATE CLONE", ImVec2(180.0f, 34.0f)))
				{
					const int selected = m_WeaponIndex;
					FiberPool::Push([selected] { SpawnManualClone(selected); });
				}
			}

			std::string_view GetMenuLabel() const override
			{
				return Localization::IsPortuguese() ? "Criar meu clone" : "Create my clone";
			}

			std::string GetMenuValue() const override
			{
				return Localization::IsPortuguese() ? kCloneWeapons[m_WeaponIndex].LabelPt : kCloneWeapons[m_WeaponIndex].LabelEn;
			}

			std::string_view GetMenuDescription() const override
			{
				return Localization::IsPortuguese() ? "Cria um clone separado do Player Must Die e permite escolher a arma que ele carregará." : "Creates a clone independent from Player Must Die and lets you choose its weapon.";
			}

			bool RequiresImGuiEditor() const override
			{
				return true;
			}

			float GetPreferredEditorHeight() const override
			{
				return 500.0f;
			}
		};
	}

	std::shared_ptr<UIItem> CreateManualCloneItem()
	{
		return std::make_shared<ManualCloneItem>();
	}
}
