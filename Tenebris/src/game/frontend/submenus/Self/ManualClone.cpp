#include "ManualClone.hpp"

#include "core/frontend/Localization.hpp"
#include "core/frontend/Notifications.hpp"
#include "core/frontend/manager/UIItem.hpp"
#include "game/backend/FiberPool.hpp"
#include "game/backend/ScriptMgr.hpp"
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

		bool EnsureModelLoaded(Hash model)
		{
			if (!model || !STREAMING::IS_MODEL_IN_CDIMAGE(model))
				return false;

			for (int i = 0; i < 40 && !STREAMING::HAS_MODEL_LOADED(model); ++i)
			{
				STREAMING::REQUEST_MODEL(model, false);
				ScriptMgr::Yield();
			}

			return STREAMING::HAS_MODEL_LOADED(model);
		}

		int CreateLocalCloneShell(int selfHandle, const Vector3& spawn, float heading)
		{
			const Hash model = ENTITY::GET_ENTITY_MODEL(selfHandle);
			LOG(INFO) << "[ManualClone] begin; model=" << model;

			if (!EnsureModelLoaded(model))
			{
				LOG(WARNING) << "[ManualClone] player model could not be loaded";
				return 0;
			}

			// Do not use CLONE_PED on the live RDO player. A player ped owns extra
			// metaped/network state and cloning that entity directly can take the game
			// down before our exception handler gets a chance to write a crash entry.
			// Instead create a plain, local-only ped first and copy only the visual
			// components/props onto that already valid target.
			LOG(INFO) << "[ManualClone] creating local shell";
			int clone = PED::CREATE_PED(model, spawn.x, spawn.y, spawn.z, heading, false, 0, 0, 0);
			STREAMING::SET_MODEL_AS_NO_LONGER_NEEDED(model);

			if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone) || PED::IS_PED_A_PLAYER(clone))
			{
				LOG(WARNING) << "[ManualClone] CREATE_PED returned an invalid target";
				if (clone && ENTITY::DOES_ENTITY_EXIST(clone) && !PED::IS_PED_A_PLAYER(clone))
					PED::DELETE_PED(&clone);
				return 0;
			}

			ENTITY::SET_ENTITY_AS_MISSION_ENTITY(clone, true, true);
			ScriptMgr::Yield();

			if (!ENTITY::DOES_ENTITY_EXIST(selfHandle) || !ENTITY::DOES_ENTITY_EXIST(clone))
			{
				if (clone && ENTITY::DOES_ENTITY_EXIST(clone))
					PED::DELETE_PED(&clone);
				return 0;
			}

			LOG(INFO) << "[ManualClone] copying appearance to shell";
			PED::CLONE_PED_TO_TARGET(selfHandle, clone);
			ScriptMgr::Yield();

			if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone) || PED::IS_PED_A_PLAYER(clone))
			{
				LOG(WARNING) << "[ManualClone] target became invalid after appearance copy";
				if (clone && ENTITY::DOES_ENTITY_EXIST(clone) && !PED::IS_PED_A_PLAYER(clone))
					PED::DELETE_PED(&clone);
				return 0;
			}

			// RDO freemode characters are MetaPeds. Rebuild the copied variation once
			// after CLONE_PED_TO_TARGET so the copied head/body/clothes are committed
			// on the local shell instead of leaving the default bald mp_male/mp_female.
			PED::_UPDATE_PED_VARIATION(clone, 0, 1, 1, 1, 0);
			ScriptMgr::Yield();

			if (!ENTITY::DOES_ENTITY_EXIST(clone))
				return 0;

			LOG(INFO) << "[ManualClone] appearance copied and refreshed; handle=" << clone;
			return clone;
		}

		void ApplyLocalPlayerPromptName(int clone)
		{
			if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone))
				return;

			const char* playerName = PLAYER::GET_PLAYER_NAME(PLAYER::PLAYER_ID());
			if (!playerName || !*playerName)
				return;

			// NativeInvoker stores each argument in a 64-bit slot. Passing the string
			// literal directly deduces char[15], which trips its sizeof(T) assertion.
			// Force pointer semantics, matching the VAR_STRING calls already used by
			// the project's native drawing code.
			const char* literalString = "LITERAL_STRING";
			PED::_SET_PED_PROMPT_NAME(clone, MISC::VAR_STRING(10, literalString, playerName));
			LOG(INFO) << "[ManualClone] prompt name set to local player: " << playerName;
		}

		void SpawnManualClone(int weaponIndex)
		{
			Ped self = Self::GetPed();
			if (!self.IsValid() || self.GetHealth() <= 0)
				return;

			const int selfHandle = self.GetHandle();
			if (!selfHandle || !ENTITY::DOES_ENTITY_EXIST(selfHandle))
				return;

			const Vector3 selfPos = self.GetPosition();
			const float heading = ENTITY::GET_ENTITY_HEADING(selfHandle);
			const float radians = heading * 0.017453292519943295f;
			const Vector3 spawn{
			    selfPos.x - std::sin(radians) * 2.2f + std::cos(radians) * 1.4f,
			    selfPos.y - std::cos(radians) * 2.2f - std::sin(radians) * 1.4f,
			    selfPos.z + 0.3f};

			int clone = CreateLocalCloneShell(selfHandle, spawn, heading);
			if (!clone)
			{
				Notifications::Show("Tenebris", Localization::IsPortuguese() ? "Não foi possível criar o clone com segurança." : "Could not create the clone safely.", NotificationType::Warning, 2600);
				return;
			}

			ENTITY::PLACE_ENTITY_ON_GROUND_PROPERLY(clone, true);
			ENTITY::SET_ENTITY_MAX_HEALTH(clone, 800);
			ENTITY::SET_ENTITY_HEALTH(clone, 800, 0);
			ApplyLocalPlayerPromptName(clone);

			weaponIndex = std::clamp(weaponIndex, 0, static_cast<int>(kCloneWeapons.size()) - 1);
			WEAPON::REMOVE_ALL_PED_WEAPONS(clone, true, true);
			const Hash weapon = Joaat(kCloneWeapons[weaponIndex].HashName);
			if (weapon != Joaat("WEAPON_UNARMED"))
			{
				WEAPON::GIVE_WEAPON_TO_PED(clone, weapon, 250, true, true, 0, false, 0.5f, 1.0f, 1.0f, false, 0, false);
				WEAPON::SET_CURRENT_PED_WEAPON(clone, weapon, true, 0, false, false);
			}

			if (!ENTITY::DOES_ENTITY_EXIST(clone))
				return;

			PED::SET_PED_KEEP_TASK(clone, true);
			PED::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(clone, false);
			PED::SET_PED_COMBAT_ABILITY(clone, 3);
			PED::SET_PED_COMBAT_MOVEMENT(clone, 2);
			PED::SET_PED_ACCURACY(clone, 72);
			PED::SET_PED_MOVE_RATE_OVERRIDE(clone, 1.15f);
			TASK::TASK_FOLLOW_TO_OFFSET_OF_ENTITY(clone, selfHandle, 1.5f, -2.0f, 0.0f, 1.2f, -1, 2.0f, true, true, false, true, true, true);

			LOG(INFO) << "[ManualClone] ready; handle=" << clone << "; weapon=" << weapon;
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
				return Localization::IsPortuguese() ? "Cria um clone local separado do Player Must Die e permite escolher a arma que ele carregará." : "Creates a local clone independent from Player Must Die and lets you choose its weapon.";
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
