#include "ManualClone.hpp"

#include "core/frontend/Localization.hpp"
#include "core/frontend/Notifications.hpp"
#include "core/frontend/manager/UIItem.hpp"
#include "game/backend/FiberPool.hpp"
#include "game/backend/ScriptMgr.hpp"
#include "game/backend/Self.hpp"
#include "game/frontend/GUI.hpp"
#include "game/rdr/Enums.hpp"
#include "game/rdr/Natives.hpp"
#include "game/rdr/Pools.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace YimMenu::Submenus
{
	namespace
	{
		using Clock = std::chrono::steady_clock;
		using namespace std::chrono_literals;

		struct CloneWeapon
		{
			const char* LabelPt;
			const char* LabelEn;
			const char* HashName;
		};

		struct CloneOptions
		{
			int WeaponIndex{};
			bool Bodyguard{true};
			bool ExtremelyHostile{};
			bool FatalGore{true};
		};

		struct SpawnPlacement
		{
			Vector3 Position{};
			float Heading{};
		};

		struct CachedPed
		{
			int Handle{};
			Vector3 Position{};
			bool Player{};
			bool Dead{};
		};

		constexpr int kCloneHealth = 800;
		constexpr float kHostileScanRadius = 500.0f;
		constexpr float kBodyguardScanRadius = 220.0f;
		constexpr float kTargetLeashMultiplier = 1.15f;
		constexpr std::size_t kMaxManagedClones = 8;
		constexpr float kBleedOutSeconds = 12.0f;
		constexpr int kBleedOutMilliseconds = 12000;
		constexpr int kIncapacitationThreshold = 20;
		constexpr int kHalloweenMaskFamilies = 6;
		constexpr int kHalloweenMaskVariantsPerFamily = 10;
		constexpr auto kPedCacheInterval = 450ms;
		constexpr auto kBrainTick = 150ms;
		constexpr auto kHostileDecisionInterval = 650ms;
		constexpr auto kBodyguardDecisionInterval = 900ms;

		constexpr std::array kCloneWeapons = {
		    CloneWeapon{"Desarmado", "Unarmed", "WEAPON_UNARMED"},
		    CloneWeapon{"Faca", "Knife", "WEAPON_MELEE_KNIFE"},
		    CloneWeapon{"Faca de urso", "Antler Knife", "WEAPON_MELEE_KNIFE_BEAR"},
		    CloneWeapon{"Faca da Guerra Civil", "Civil War Knife", "WEAPON_MELEE_KNIFE_CIVIL_WAR"},
		    CloneWeapon{"Faca Jawbone", "Jawbone Knife", "WEAPON_MELEE_KNIFE_JAWBONE"},
		    CloneWeapon{"Facão", "Machete", "WEAPON_MELEE_MACHETE"},
		    CloneWeapon{"Facão de colecionador", "Collector Machete", "WEAPON_MELEE_MACHETE_COLLECTOR"},
		    CloneWeapon{"Machadinha", "Hatchet", "WEAPON_MELEE_HATCHET"},
		    CloneWeapon{"Machado Viking", "Viking Hatchet", "WEAPON_MELEE_HATCHET_VIKING"},
		    CloneWeapon{"Machado antigo", "Ancient Hatchet", "WEAPON_MELEE_ANCIENT_HATCHET"},
		    CloneWeapon{"Espada quebrada", "Broken Sword", "WEAPON_MELEE_BROKEN_SWORD"},
		    CloneWeapon{"Faca do Bill (História)", "Bill's Knife (Story)", "WEAPON_MELEE_KNIFE_BILL"},
		    CloneWeapon{"Faca do Charles (História)", "Charles' Knife (Story)", "WEAPON_MELEE_KNIFE_CHARLES"},
		    CloneWeapon{"Faca do Dutch (História)", "Dutch's Knife (Story)", "WEAPON_MELEE_KNIFE_DUTCH"},
		    CloneWeapon{"Faca do Javier (História)", "Javier's Knife (Story)", "WEAPON_MELEE_KNIFE_JAVIER"},
		    CloneWeapon{"Faca do John (História)", "John's Knife (Story)", "WEAPON_MELEE_KNIFE_JOHN"},
		    CloneWeapon{"Faca do Micah (História)", "Micah's Knife (Story)", "WEAPON_MELEE_KNIFE_MICAH"},
		    CloneWeapon{"Faca da Sadie (História)", "Sadie's Knife (Story)", "WEAPON_MELEE_KNIFE_SADIE"},
		    CloneWeapon{"Revólver Cattleman", "Cattleman Revolver", "WEAPON_REVOLVER_CATTLEMAN"},
		    CloneWeapon{"Cattleman do Hosea (História)", "Hosea's Cattleman (Story)", "WEAPON_REVOLVER_CATTLEMAN_HOSEA"},
		    CloneWeapon{"Cattleman do Hosea - duplo (História)", "Hosea's Cattleman - Dual (Story)", "WEAPON_REVOLVER_CATTLEMAN_HOSEA_DUALWIELD"},
		    CloneWeapon{"Cattleman do John (História)", "John's Cattleman (Story)", "WEAPON_REVOLVER_CATTLEMAN_JOHN"},
		    CloneWeapon{"Cattleman do Kieran (História)", "Kieran's Cattleman (Story)", "WEAPON_REVOLVER_CATTLEMAN_KIERAN"},
		    CloneWeapon{"Cattleman do Lenny (História)", "Lenny's Cattleman (Story)", "WEAPON_REVOLVER_CATTLEMAN_LENNY"},
		    CloneWeapon{"Revólver do Flaco Hernández (Pistoleiro)", "Flaco Hernandez's Revolver (Gunslinger)", "WEAPON_REVOLVER_CATTLEMAN_MEXICAN"},
		    CloneWeapon{"Revólver do Emmet Granger (Pistoleiro)", "Emmet Granger's Revolver (Gunslinger)", "WEAPON_REVOLVER_CATTLEMAN_PIG"},
		    CloneWeapon{"Cattleman da Sadie (História)", "Sadie's Cattleman (Story)", "WEAPON_REVOLVER_CATTLEMAN_SADIE"},
		    CloneWeapon{"Cattleman da Sadie - duplo (História)", "Sadie's Cattleman - Dual (Story)", "WEAPON_REVOLVER_CATTLEMAN_SADIE_DUALWIELD"},
		    CloneWeapon{"Cattleman do Sean (História)", "Sean's Cattleman (Story)", "WEAPON_REVOLVER_CATTLEMAN_SEAN"},
		    CloneWeapon{"Revólver Double-Action", "Double-Action Revolver", "WEAPON_REVOLVER_DOUBLEACTION"},
		    CloneWeapon{"Revólver do Algernon Wasp (História)", "Algernon Wasp's Revolver (Story)", "WEAPON_REVOLVER_DOUBLEACTION_EXOTIC"},
		    CloneWeapon{"High Roller Double-Action", "High Roller Double-Action", "WEAPON_REVOLVER_DOUBLEACTION_GAMBLER"},
		    CloneWeapon{"Double-Action do Javier (História)", "Javier's Double-Action (Story)", "WEAPON_REVOLVER_DOUBLEACTION_JAVIER"},
		    CloneWeapon{"Revólver do Micah (História)", "Micah's Revolver (Story)", "WEAPON_REVOLVER_DOUBLEACTION_MICAH"},
		    CloneWeapon{"Revólver do Micah - duplo (História)", "Micah's Revolver - Dual (Story)", "WEAPON_REVOLVER_DOUBLEACTION_MICAH_DUALWIELD"},
		    CloneWeapon{"Revólver Schofield", "Schofield Revolver", "WEAPON_REVOLVER_SCHOFIELD"},
		    CloneWeapon{"Schofield do Bill (História)", "Bill's Schofield (Story)", "WEAPON_REVOLVER_SCHOFIELD_BILL"},
		    CloneWeapon{"Revólver do Jim Boy Calloway (Pistoleiro)", "Jim Boy Calloway's Revolver (Gunslinger)", "WEAPON_REVOLVER_SCHOFIELD_CALLOWAY"},
		    CloneWeapon{"Schofield do Dutch (História)", "Dutch's Schofield (Story)", "WEAPON_REVOLVER_SCHOFIELD_DUTCH"},
		    CloneWeapon{"Schofield do Dutch - duplo (História)", "Dutch's Schofield - Dual (Story)", "WEAPON_REVOLVER_SCHOFIELD_DUTCH_DUALWIELD"},
		    CloneWeapon{"Revólver do Otis Miller (História)", "Otis Miller's Revolver (Story)", "WEAPON_REVOLVER_SCHOFIELD_GOLDEN"},
		    CloneWeapon{"Schofield do Uncle (História)", "Uncle's Schofield (Story)", "WEAPON_REVOLVER_SCHOFIELD_UNCLE"},
		    CloneWeapon{"Revólver LeMat", "LeMat Revolver", "WEAPON_REVOLVER_LEMAT"},
		    CloneWeapon{"Revólver Navy", "Navy Revolver", "WEAPON_REVOLVER_NAVY"},
		    CloneWeapon{"Pistola Volcanic", "Volcanic Pistol", "WEAPON_PISTOL_VOLCANIC"},
		    CloneWeapon{"Pistola M1899", "M1899 Pistol", "WEAPON_PISTOL_M1899"},
		    CloneWeapon{"Pistola Mauser", "Mauser Pistol", "WEAPON_PISTOL_MAUSER"},
		    CloneWeapon{"Pistola do Billy Midnight (Pistoleiro)", "Billy Midnight's Pistol (Gunslinger)", "WEAPON_PISTOL_MAUSER_DRUNK"},
		    CloneWeapon{"Pistola Semi-Auto", "Semi-Auto Pistol", "WEAPON_PISTOL_SEMIAUTO"},
		    CloneWeapon{"Carabina", "Carbine Repeater", "WEAPON_REPEATER_CARBINE"},
		    CloneWeapon{"Carabina da Sadie (História)", "Sadie's Carbine Repeater (Story)", "WEAPON_REPEATER_CARBINE_SADIE"},
		    CloneWeapon{"Evans Repeater", "Evans Repeater", "WEAPON_REPEATER_EVANS"},
		    CloneWeapon{"Litchfield Repeater", "Litchfield Repeater", "WEAPON_REPEATER_HENRY"},
		    CloneWeapon{"Lancaster Repeater", "Lancaster Repeater", "WEAPON_REPEATER_WINCHESTER"},
		    CloneWeapon{"Lancaster do John (História)", "John's Lancaster (Story)", "WEAPON_REPEATER_WINCHESTER_JOHN"},
		    CloneWeapon{"Rifle Bolt Action", "Bolt Action Rifle", "WEAPON_RIFLE_BOLTACTION"},
		    CloneWeapon{"Bolt Action do Bill (História)", "Bill's Bolt Action (Story)", "WEAPON_RIFLE_BOLTACTION_BILL"},
		    CloneWeapon{"Rifle Springfield", "Springfield Rifle", "WEAPON_RIFLE_SPRINGFIELD"},
		    CloneWeapon{"Rifle Varmint", "Varmint Rifle", "WEAPON_RIFLE_VARMINT"},
		    CloneWeapon{"Rifle Elephant", "Elephant Rifle", "WEAPON_RIFLE_ELEPHANT"},
		    CloneWeapon{"Rifle Carcano", "Carcano Rifle", "WEAPON_SNIPERRIFLE_CARCANO"},
		    CloneWeapon{"Rolling Block", "Rolling Block Rifle", "WEAPON_SNIPERRIFLE_ROLLINGBLOCK"},
		    CloneWeapon{"Rolling Block raro", "Rare Rolling Block", "WEAPON_SNIPERRIFLE_ROLLINGBLOCK_EXOTIC"},
		    CloneWeapon{"Rolling Block do Lenny (História)", "Lenny's Rolling Block (Story)", "WEAPON_SNIPERRIFLE_ROLLINGBLOCK_LENNY"},
		    CloneWeapon{"Escopeta Pump", "Pump Shotgun", "WEAPON_SHOTGUN_PUMP"},
		    CloneWeapon{"Escopeta Repeating", "Repeating Shotgun", "WEAPON_SHOTGUN_REPEATING"},
		    CloneWeapon{"Escopeta Double Barrel", "Double Barrel Shotgun", "WEAPON_SHOTGUN_DOUBLEBARREL"},
		    CloneWeapon{"Escopeta Double Barrel rara", "Rare Double Barrel", "WEAPON_SHOTGUN_DOUBLEBARREL_EXOTIC"},
		    CloneWeapon{"Double Barrel do Uncle (História)", "Uncle's Double Barrel (Story)", "WEAPON_SHOTGUN_DOUBLEBARREL_UNCLE"},
		    CloneWeapon{"Escopeta Semi-Auto", "Semi-Auto Shotgun", "WEAPON_SHOTGUN_SEMIAUTO"},
		    CloneWeapon{"Semi-Auto do Hosea (História)", "Hosea's Semi-Auto (Story)", "WEAPON_SHOTGUN_SEMIAUTO_HOSEA"},
		    CloneWeapon{"Escopeta serrada", "Sawed-Off Shotgun", "WEAPON_SHOTGUN_SAWEDOFF"},
		    CloneWeapon{"Serrada do Charles (História)", "Charles' Sawed-Off (Story)", "WEAPON_SHOTGUN_SAWEDOFF_CHARLES"},
		    CloneWeapon{"Arco", "Bow", "WEAPON_BOW"},
		    CloneWeapon{"Arco do Charles (História)", "Charles' Bow (Story)", "WEAPON_BOW_CHARLES"},
		    CloneWeapon{"Arco melhorado", "Improved Bow", "WEAPON_BOW_IMPROVED"},
		};

		std::vector<int> g_ManagedClones;
		std::vector<CachedPed> g_PedCache;
		Clock::time_point g_NextPedCacheRefresh{};
		Hash g_LastHalloweenMask{};

		float DistanceSquared(const Vector3& a, const Vector3& b)
		{
			const float dx = a.x - b.x;
			const float dy = a.y - b.y;
			const float dz = a.z - b.z;
			return dx * dx + dy * dy + dz * dz;
		}

		void PruneManagedClones()
		{
			g_ManagedClones.erase(std::remove_if(g_ManagedClones.begin(), g_ManagedClones.end(), [](int ped) {
				return !ped || !ENTITY::DOES_ENTITY_EXIST(ped);
			}), g_ManagedClones.end());
		}

		bool IsManagedClone(int ped)
		{
			return std::find(g_ManagedClones.begin(), g_ManagedClones.end(), ped) != g_ManagedClones.end();
		}

		const std::vector<CachedPed>& GetCachedPeds()
		{
			const auto now = Clock::now();
			if (now < g_NextPedCacheRefresh)
				return g_PedCache;

			g_PedCache.clear();
			g_PedCache.reserve(96);
			for (Ped ped : Pools::GetPeds())
			{
				if (!ped.IsValid())
					continue;
				const int handle = ped.GetHandle();
				if (!handle)
					continue;
				CachedPed entry{};
				entry.Handle = handle;
				entry.Player = PED::IS_PED_A_PLAYER(handle);
				entry.Dead = ENTITY::IS_ENTITY_DEAD(handle);
				entry.Position = ENTITY::GET_ENTITY_COORDS(handle, true, false);
				g_PedCache.push_back(entry);
			}
			g_NextPedCacheRefresh = now + kPedCacheInterval;
			return g_PedCache;
		}

		bool IsSidearm(std::string_view weaponName)
		{
			return weaponName.find("WEAPON_PISTOL_") != std::string_view::npos || weaponName.find("WEAPON_REVOLVER_") != std::string_view::npos;
		}

		bool IsLongGun(std::string_view weaponName)
		{
			return weaponName.find("WEAPON_RIFLE_") != std::string_view::npos || weaponName.find("WEAPON_REPEATER_") != std::string_view::npos || weaponName.find("WEAPON_SNIPERRIFLE_") != std::string_view::npos;
		}

		int AccuracyForWeapon(std::string_view weaponName)
		{
			if (IsSidearm(weaponName))
				return 60;
			if (IsLongGun(weaponName))
				return 89;
			return 74;
		}

		bool EnsureModelLoaded(Hash model)
		{
			if (!model || !STREAMING::IS_MODEL_IN_CDIMAGE(model))
				return false;
			for (int i = 0; i < 80 && !STREAMING::HAS_MODEL_LOADED(model); ++i)
			{
				STREAMING::REQUEST_MODEL(model, false);
				ScriptMgr::Yield(10ms);
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

			int clone = PED::CREATE_PED(model, spawn.x, spawn.y, spawn.z, heading, false, 0, 0, 0);
			STREAMING::SET_MODEL_AS_NO_LONGER_NEEDED(model);
			if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone) || PED::IS_PED_A_PLAYER(clone))
			{
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

			PED::CLONE_PED_TO_TARGET(selfHandle, clone);
			ScriptMgr::Yield();
			if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone) || PED::IS_PED_A_PLAYER(clone))
			{
				if (clone && ENTITY::DOES_ENTITY_EXIST(clone) && !PED::IS_PED_A_PLAYER(clone))
					PED::DELETE_PED(&clone);
				return 0;
			}

			PED::_UPDATE_PED_VARIATION(clone, 0, 1, 1, 1, 0);
			ScriptMgr::Yield();
			return ENTITY::DOES_ENTITY_EXIST(clone) ? clone : 0;
		}

		void ApplyLocalPlayerPromptName(int clone)
		{
			if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone))
				return;
			const char* playerName = PLAYER::GET_PLAYER_NAME(PLAYER::PLAYER_ID());
			if (!playerName || !*playerName)
				return;
			const char* literalString = "LITERAL_STRING";
			PED::_SET_PED_PROMPT_NAME(clone, MISC::VAR_STRING(10, literalString, playerName));
		}

		void ApplyRandomHalloweenMask(int clone)
		{
			if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone))
				return;
			const Hash model = ENTITY::GET_ENTITY_MODEL(clone);
			char gender{};
			if (model == Joaat("mp_male"))
				gender = 'm';
			else if (model == Joaat("mp_female"))
				gender = 'f';
			else
				return;

			static std::mt19937 rng{static_cast<std::mt19937::result_type>(std::chrono::high_resolution_clock::now().time_since_epoch().count())};
			std::uniform_int_distribution<int> familyDist(0, kHalloweenMaskFamilies - 1);
			std::uniform_int_distribution<int> variantDist(1, kHalloweenMaskVariantsPerFamily);
			Hash component{};
			char componentName[80]{};
			for (int attempt = 0; attempt < 8; ++attempt)
			{
				std::snprintf(componentName, sizeof(componentName), "clothing_item_%c_halloween_mask_%03d_var_%03d", gender, familyDist(rng), variantDist(rng));
				component = Joaat(componentName);
				if (component != g_LastHalloweenMask)
					break;
			}
			if (!component)
				return;
			PED::_SET_PED_COMPONENT_ENABLED(clone, component, true, true, true);
			PED::_UPDATE_PED_VARIATION(clone, 0, 1, 1, 1, 0);
			g_LastHalloweenMask = component;
		}

		void ConfigureBleedout(int clone)
		{
			if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone))
				return;
			PED::SET_PED_CAN_BE_INCAPACITATED(clone, true);
			PED::_SET_PED_INCAPACITATION_MODIFIERS(clone, true, kIncapacitationThreshold, kBleedOutMilliseconds, 0);
			PED::_SET_PED_INCAPACITATION_TOTAL_BLEED_OUT_DURATION(clone, kBleedOutSeconds);
			PED::_SET_PED_WRITHING_DURATION(clone, 8.0f, kBleedOutSeconds, 0);
			PED::SET_PAUSE_PED_WRITHE_BLEEDOUT(clone, false);
		}

		int GetPlayerMountHandle()
		{
			auto mount = Self::GetMount();
			return mount.IsValid() ? mount.GetHandle() : 0;
		}

		bool IsValidCombatTarget(int candidate, int clone, int owner)
		{
			if (!candidate || candidate == clone || candidate == owner || !ENTITY::DOES_ENTITY_EXIST(candidate) || ENTITY::IS_ENTITY_DEAD(candidate))
				return false;
			if (PED::IS_PED_A_PLAYER(candidate) || IsManagedClone(candidate))
				return false;
			return candidate != GetPlayerMountHandle();
		}

		bool IsTargetWithinRange(int source, int target, float radius)
		{
			if (!source || !target || !ENTITY::DOES_ENTITY_EXIST(source) || !ENTITY::DOES_ENTITY_EXIST(target))
				return false;
			const Vector3 a = ENTITY::GET_ENTITY_COORDS(source, true, false);
			const Vector3 b = ENTITY::GET_ENTITY_COORDS(target, true, false);
			return DistanceSquared(a, b) <= radius * radius;
		}

		int FindNearestHostileTarget(int clone, int owner, float radius)
		{
			if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone))
				return 0;
			const Vector3 origin = ENTITY::GET_ENTITY_COORDS(clone, true, false);
			const float radiusSq = radius * radius;
			const int mount = GetPlayerMountHandle();
			int best{};
			float bestDistanceSq = radiusSq;
			for (const auto& ped : GetCachedPeds())
			{
				if (!ped.Handle || ped.Handle == clone || ped.Handle == owner || ped.Handle == mount || ped.Player || ped.Dead || IsManagedClone(ped.Handle))
					continue;
				const float distanceSq = DistanceSquared(origin, ped.Position);
				if (distanceSq < bestDistanceSq)
				{
					bestDistanceSq = distanceSq;
					best = ped.Handle;
				}
			}
			return IsValidCombatTarget(best, clone, owner) ? best : 0;
		}

		int FindBodyguardThreat(int clone, int owner)
		{
			if (!owner || !ENTITY::DOES_ENTITY_EXIST(owner))
				return 0;
			const Vector3 ownerPos = ENTITY::GET_ENTITY_COORDS(owner, true, false);
			const float radiusSq = kBodyguardScanRadius * kBodyguardScanRadius;
			const int mount = GetPlayerMountHandle();
			int best{};
			float bestDistanceSq = radiusSq;
			for (const auto& ped : GetCachedPeds())
			{
				if (!ped.Handle || ped.Handle == clone || ped.Handle == owner || ped.Handle == mount || ped.Player || ped.Dead || IsManagedClone(ped.Handle))
					continue;
				if (!PED::IS_PED_IN_COMBAT(ped.Handle, owner) && !PED::IS_PED_IN_COMBAT(owner, ped.Handle))
					continue;
				const float distanceSq = DistanceSquared(ownerPos, ped.Position);
				if (distanceSq < bestDistanceSq)
				{
					bestDistanceSq = distanceSq;
					best = ped.Handle;
				}
			}
			return IsValidCombatTarget(best, clone, owner) ? best : 0;
		}

		template <std::size_t N>
		bool DamageBoneMatches(int ped, int damageBone, const std::array<int, N>& boneTags)
		{
			for (int boneTag : boneTags)
			{
				if (damageBone == boneTag)
					return true;
				const int boneIndex = PED::GET_PED_BONE_INDEX(ped, boneTag);
				if (boneIndex >= 0 && damageBone == boneIndex)
					return true;
			}
			return false;
		}

		bool IsHeadDamageBone(int ped, int damageBone)
		{
			static constexpr std::array<int, 2> kHeadBones{21030, 27981};
			return DamageBoneMatches(ped, damageBone, kHeadBones);
		}

		bool IsTorsoDamageBone(int ped, int damageBone)
		{
			static constexpr std::array<int, 11> kTorsoBones{11569, 14410, 14411, 14412, 14413, 14414, 14415, 14416, 6757, 6758, 57309};
			return DamageBoneMatches(ped, damageBone, kTorsoBones);
		}

		void ApplyFatalGore(int clone, int lastDamageBone, Hash weapon)
		{
			if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone) || !lastDamageBone)
				return;
			if (IsHeadDamageBone(clone, lastDamageBone))
			{
				PED::EXPLODE_PED_HEAD(clone, weapon ? weapon : Joaat("WEAPON_REPEATER_CARBINE"));
				return;
			}
			if (IsTorsoDamageBone(clone, lastDamageBone))
				PED::APPLY_PED_DAMAGE_PACK(clone, "PD_Human_carcass_Hvy", 1.0f, 1.0f);
		}

		void ConfigureCloneCombat(int clone, const CloneOptions& options, std::string_view weaponName)
		{
			PED::SET_PED_KEEP_TASK(clone, true);
			PED::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(clone, !options.Bodyguard && !options.ExtremelyHostile);
			PED::SET_PED_COMBAT_ABILITY(clone, 3);
			PED::SET_PED_COMBAT_MOVEMENT(clone, options.ExtremelyHostile ? 3 : 2);
			PED::SET_PED_COMBAT_RANGE(clone, options.ExtremelyHostile ? 3 : 2);
			PED::SET_PED_ACCURACY(clone, AccuracyForWeapon(weaponName));
			PED::SET_PED_SEEING_RANGE(clone, options.ExtremelyHostile ? 500.0f : 220.0f);
			PED::SET_PED_HEARING_RANGE(clone, options.ExtremelyHostile ? 500.0f : 220.0f);
			PED::SET_PED_MOVE_RATE_OVERRIDE(clone, options.ExtremelyHostile ? 1.45f : 1.15f);
			for (int attribute : {5, 13, 21, 25, 31, 39, 41, 42, 46, 49, 54, 58, 63, 68, 78, 80, 81, 91, 92, 93, 113, 115})
				PED::SET_PED_COMBAT_ATTRIBUTES(clone, attribute, true);
			PED::SET_PED_COMBAT_ATTRIBUTES(clone, 27, false);
		}

		bool RecoverIntoBleedout(int clone)
		{
			if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone))
				return false;
			PED::RESURRECT_PED(clone);
			PED::REVIVE_INJURED_PED(clone);
			if (!ENTITY::DOES_ENTITY_EXIST(clone))
				return false;
			ENTITY::SET_ENTITY_HEALTH(clone, 2, 0);
			ConfigureBleedout(clone);
			PED::SET_PED_TO_RAGDOLL(clone, 900, 900, 0, false, false, false);
			return true;
		}

		void RunCloneBrain(int clone, int owner, CloneOptions options, Hash weapon)
		{
			int currentTarget{};
			int issuedTarget{};
			int lastDamageBone{};
			bool followingOwner = false;
			bool fatalRecoveryUsed = false;
			bool sawIncapacitation = false;
			auto nextDecision = Clock::now();
			auto fallbackDeathAt = Clock::time_point::max();

			while (clone && ENTITY::DOES_ENTITY_EXIST(clone))
			{
				int damageBone{};
				if (PED::GET_PED_LAST_DAMAGE_BONE(clone, &damageBone) && damageBone)
					lastDamageBone = damageBone;

				if (PED::IS_PED_INCAPACITATED(clone))
				{
					if (!sawIncapacitation)
					{
						sawIncapacitation = true;
						currentTarget = 0;
						issuedTarget = 0;
						PED::SET_PAUSE_PED_WRITHE_BLEEDOUT(clone, false);
					}
					ScriptMgr::Yield(kBrainTick);
					continue;
				}

				if (ENTITY::IS_ENTITY_DEAD(clone))
				{
					if (!sawIncapacitation && !fatalRecoveryUsed)
					{
						fatalRecoveryUsed = RecoverIntoBleedout(clone);
						if (fatalRecoveryUsed)
						{
							fallbackDeathAt = Clock::now() + 10s;
							ScriptMgr::Yield(kBrainTick);
							continue;
						}
					}
					if (options.FatalGore)
						ApplyFatalGore(clone, lastDamageBone, weapon);
					break;
				}

				const auto now = Clock::now();
				if (fatalRecoveryUsed && !sawIncapacitation && now >= fallbackDeathAt)
				{
					ENTITY::SET_ENTITY_HEALTH(clone, 0, 0);
					ScriptMgr::Yield(kBrainTick);
					continue;
				}

				if (!fatalRecoveryUsed && now >= nextDecision)
				{
					bool keepCurrent = currentTarget && IsValidCombatTarget(currentTarget, clone, owner);
					if (keepCurrent && options.ExtremelyHostile)
						keepCurrent = IsTargetWithinRange(clone, currentTarget, kHostileScanRadius * kTargetLeashMultiplier);
					else if (keepCurrent && options.Bodyguard)
						keepCurrent = IsTargetWithinRange(owner, currentTarget, kBodyguardScanRadius * kTargetLeashMultiplier) &&
						    (PED::IS_PED_IN_COMBAT(currentTarget, owner) || PED::IS_PED_IN_COMBAT(owner, currentTarget));
					else if (!options.ExtremelyHostile && !options.Bodyguard)
						keepCurrent = false;

					if (!keepCurrent)
					{
						currentTarget = options.ExtremelyHostile ? FindNearestHostileTarget(clone, owner, kHostileScanRadius) :
						    (options.Bodyguard ? FindBodyguardThreat(clone, owner) : 0);
					}

					if (currentTarget && IsValidCombatTarget(currentTarget, clone, owner))
					{
						followingOwner = false;
						if (issuedTarget != currentTarget || !PED::IS_PED_IN_COMBAT(clone, currentTarget))
						{
							TASK::TASK_COMBAT_PED(clone, currentTarget, 0, 16);
							issuedTarget = currentTarget;
						}
					}
					else
					{
						issuedTarget = 0;
						if (options.Bodyguard && owner && ENTITY::DOES_ENTITY_EXIST(owner) && !followingOwner)
						{
							TASK::TASK_FOLLOW_TO_OFFSET_OF_ENTITY(clone, owner, 1.5f, -2.0f, 0.0f, 1.2f, -1, 2.0f, true, true, false, true, true, true);
							followingOwner = true;
						}
					}

					nextDecision = now + (options.ExtremelyHostile ? kHostileDecisionInterval : kBodyguardDecisionInterval);
				}

				ScriptMgr::Yield(kBrainTick);
			}

			g_ManagedClones.erase(std::remove(g_ManagedClones.begin(), g_ManagedClones.end(), clone), g_ManagedClones.end());
		}

		Hash EquipCloneWeapon(int clone, int weaponIndex)
		{
			weaponIndex = std::clamp(weaponIndex, 0, static_cast<int>(kCloneWeapons.size()) - 1);
			WEAPON::REMOVE_ALL_PED_WEAPONS(clone, true, true);
			const Hash weapon = Joaat(kCloneWeapons[weaponIndex].HashName);
			if (weapon != Joaat("WEAPON_UNARMED"))
			{
				WEAPON::GIVE_WEAPON_TO_PED(clone, weapon, 999, true, true, 0, false, 0.5f, 1.0f, 1.0f, false, 0, false);
				WEAPON::SET_CURRENT_PED_WEAPON(clone, weapon, true, 0, false, false);
			}
			return weapon;
		}

		void SpawnManualCloneAt(CloneOptions options, const Vector3& spawn, float heading)
		{
			PruneManagedClones();
			if (g_ManagedClones.size() >= kMaxManagedClones)
			{
				Notifications::Show("Tenebris", Localization::IsPortuguese() ? "Limite de 8 clones manuais ativos atingido." : "The limit of 8 active manual clones has been reached.", NotificationType::Warning, 2600);
				return;
			}

			auto self = Self::GetPed();
			if (!self.IsValid() || self.GetHealth() <= 0)
				return;
			const int selfHandle = self.GetHandle();
			if (!selfHandle || !ENTITY::DOES_ENTITY_EXIST(selfHandle))
				return;

			int clone = CreateLocalCloneShell(selfHandle, spawn, heading);
			if (!clone)
			{
				Notifications::Show("Tenebris", Localization::IsPortuguese() ? "Não foi possível criar o clone com segurança." : "Could not create the clone safely.", NotificationType::Warning, 2600);
				return;
			}

			ENTITY::PLACE_ENTITY_ON_GROUND_PROPERLY(clone, true);
			ENTITY::SET_ENTITY_MAX_HEALTH(clone, kCloneHealth);
			ENTITY::SET_ENTITY_HEALTH(clone, kCloneHealth, 0);
			ApplyLocalPlayerPromptName(clone);
			ApplyRandomHalloweenMask(clone);
			ConfigureBleedout(clone);

			options.WeaponIndex = std::clamp(options.WeaponIndex, 0, static_cast<int>(kCloneWeapons.size()) - 1);
			const Hash weapon = EquipCloneWeapon(clone, options.WeaponIndex);
			if (!ENTITY::DOES_ENTITY_EXIST(clone))
				return;

			ConfigureCloneCombat(clone, options, kCloneWeapons[options.WeaponIndex].HashName);
			g_ManagedClones.push_back(clone);
			g_NextPedCacheRefresh = {};

			if (!options.ExtremelyHostile && !options.Bodyguard)
				TASK::CLEAR_PED_TASKS(clone, true, false);

			FiberPool::Push([clone, selfHandle, options, weapon] { RunCloneBrain(clone, selfHandle, options, weapon); });
			Notifications::Show("Tenebris", Localization::IsPortuguese() ? "Clone criado com máscara Halloween aleatória." : "Clone created with a random Halloween mask.", NotificationType::Success, 2200);
		}

		void SpawnManualCloneNearPlayer(CloneOptions options)
		{
			auto self = Self::GetPed();
			if (!self.IsValid())
				return;
			const int selfHandle = self.GetHandle();
			const Vector3 selfPos = self.GetPosition();
			const float heading = ENTITY::GET_ENTITY_HEADING(selfHandle);
			const float radians = heading * 0.017453292519943295f;
			const Vector3 spawn{selfPos.x - std::sin(radians) * 2.2f + std::cos(radians) * 1.4f, selfPos.y - std::cos(radians) * 2.2f - std::sin(radians) * 1.4f, selfPos.z + 0.3f};
			SpawnManualCloneAt(options, spawn, heading);
		}

		bool PickCloneSpawnWithFreecam(SpawnPlacement& placement)
		{
			auto self = Self::GetPed();
			if (!self.IsValid())
				return false;

			const bool reopenMenu = GUI::IsOpen();
			if (reopenMenu)
				GUI::Toggle();

			int camera = CAM::CREATE_CAM("DEFAULT_SCRIPTED_CAMERA", 0);
			if (!camera)
			{
				if (reopenMenu && !GUI::IsOpen())
					GUI::Toggle();
				return false;
			}

			Vector3 position = CAM::GET_GAMEPLAY_CAM_COORD();
			Vector3 rotation = CAM::GET_GAMEPLAY_CAM_ROT(2);
			CAM::SET_CAM_COORD(camera, position.x, position.y, position.z);
			CAM::SET_CAM_ROT(camera, rotation.x, rotation.y, rotation.z, 2);
			CAM::SET_CAM_ACTIVE(camera, true);
			CAM::RENDER_SCRIPT_CAMS(true, true, 350, true, true, 0);
			self.SetFrozen(true);
			self.SetVisible(false);
			Notifications::Show("Tenebris", Localization::IsPortuguese() ? "POSICIONAMENTO: mova a câmera; ENTER confirma o círculo; BACK cancela." : "PLACEMENT: move the camera; ENTER confirms the marker; BACK cancels.", NotificationType::Info, 5000);

			bool accepted = false;
			float acceleration = 0.0f;
			while (camera && CAM::DOES_CAM_EXIST(camera))
			{
				PAD::DISABLE_ALL_CONTROL_ACTIONS(0);
				for (Hash control : {(Hash)NativeInputs::INPUT_LOOK_LR, (Hash)NativeInputs::INPUT_LOOK_UD, (Hash)NativeInputs::INPUT_LOOK_UP_ONLY, (Hash)NativeInputs::INPUT_LOOK_DOWN_ONLY, (Hash)NativeInputs::INPUT_LOOK_LEFT_ONLY, (Hash)NativeInputs::INPUT_LOOK_RIGHT_ONLY})
					PAD::ENABLE_CONTROL_ACTION(0, control, true);

				Vector3 delta{};
				constexpr float speed = 0.12f;
				if (PAD::IS_DISABLED_CONTROL_PRESSED(0, (Hash)NativeInputs::INPUT_MOVE_UP_ONLY)) delta.y += speed;
				if (PAD::IS_DISABLED_CONTROL_PRESSED(0, (Hash)NativeInputs::INPUT_MOVE_DOWN_ONLY)) delta.y -= speed;
				if (PAD::IS_DISABLED_CONTROL_PRESSED(0, (Hash)NativeInputs::INPUT_MOVE_LEFT_ONLY)) delta.x -= speed;
				if (PAD::IS_DISABLED_CONTROL_PRESSED(0, (Hash)NativeInputs::INPUT_MOVE_RIGHT_ONLY)) delta.x += speed;
				if (PAD::IS_DISABLED_CONTROL_PRESSED(0, (Hash)NativeInputs::INPUT_SPRINT)) delta.z += speed * 0.75f;
				if (PAD::IS_DISABLED_CONTROL_PRESSED(0, (Hash)NativeInputs::INPUT_DUCK) || PAD::IS_DISABLED_CONTROL_PRESSED(0, (Hash)NativeInputs::INPUT_HORSE_STOP)) delta.z -= speed * 0.75f;

				acceleration = (delta.x == 0.0f && delta.y == 0.0f && delta.z == 0.0f) ? 0.0f : std::min(8.0f, acceleration + 0.12f);
				rotation = CAM::GET_GAMEPLAY_CAM_ROT(2);
				const float yaw = rotation.z * 0.017453292519943295f;
				position.x += (delta.x * std::cos(yaw) - delta.y * std::sin(yaw)) * acceleration;
				position.y += (delta.x * std::sin(yaw) + delta.y * std::cos(yaw)) * acceleration;
				position.z += delta.z * acceleration;
				CAM::SET_CAM_COORD(camera, position.x, position.y, position.z);
				CAM::SET_CAM_ROT(camera, rotation.x, rotation.y, rotation.z, 2);
				STREAMING::SET_FOCUS_POS_AND_VEL(position.x, position.y, position.z, 0.0f, 0.0f, 0.0f);

				Vector3 marker = position;
				float groundZ{};
				if (MISC::GET_GROUND_Z_FOR_3D_COORD(position.x, position.y, position.z + 100.0f, &groundZ, 0))
					marker.z = groundZ;
				else
					marker.z -= 1.0f;

				GRAPHICS::_DRAW_MARKER(0x6903B113, marker.x, marker.y, marker.z + 0.03f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 1.25f, 1.25f, 1.25f, 255, 255, 255, 230, false, true, 2, false, nullptr, nullptr, false);
				if (PAD::IS_DISABLED_CONTROL_JUST_PRESSED(0, (Hash)NativeInputs::INPUT_FRONTEND_ACCEPT))
				{
					placement.Position = marker;
					placement.Heading = rotation.z;
					accepted = true;
					break;
				}
				if (PAD::IS_DISABLED_CONTROL_JUST_PRESSED(0, (Hash)NativeInputs::INPUT_FRONTEND_CANCEL))
					break;
				ScriptMgr::Yield();
			}

			CAM::SET_CAM_ACTIVE(camera, false);
			CAM::RENDER_SCRIPT_CAMS(false, true, 350, true, true, 0);
			CAM::DESTROY_CAM(camera, false);
			STREAMING::CLEAR_FOCUS();
			self.SetFrozen(false);
			self.SetVisible(true);
			if (reopenMenu && !GUI::IsOpen())
				GUI::Toggle();
			return accepted;
		}

		class ManualCloneItem final : public UIItem
		{
			int m_WeaponIndex{};
			bool m_Bodyguard{true};
			bool m_ExtremelyHostile{};
			bool m_FatalGore{true};

			CloneOptions GetOptions() const
			{
				return CloneOptions{m_WeaponIndex, m_Bodyguard, m_ExtremelyHostile, m_FatalGore};
			}

		public:
			void Draw() override
			{
				ImGui::TextUnformatted(Localization::IsPortuguese() ? "Criar meu clone" : "Create my clone");
				ImGui::Spacing();
				ImGui::TextUnformatted(Localization::IsPortuguese() ? "Arma do clone" : "Clone weapon");
				const char* preview = Localization::IsPortuguese() ? kCloneWeapons[m_WeaponIndex].LabelPt : kCloneWeapons[m_WeaponIndex].LabelEn;
				ImGui::SetNextItemWidth(-1.0f);
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
				ImGui::SeparatorText(Localization::IsPortuguese() ? "COMPORTAMENTO" : "BEHAVIOR");
				ImGui::Checkbox(Localization::IsPortuguese() ? "Guarda-costas" : "Bodyguard", &m_Bodyguard);
				ImGui::Checkbox(Localization::IsPortuguese() ? "Extremamente hostil com NPCs/animais" : "Extremely hostile to NPCs/animals", &m_ExtremelyHostile);
				ImGui::Checkbox(Localization::IsPortuguese() ? "Gore final após a agonia" : "Final gore after bleedout", &m_FatalGore);
				ImGui::TextWrapped("%s", Localization::IsPortuguese() ? "O clone mantém 800 de vida. Ao chegar ao estado fatal, usa a incapacitação/sangramento nativa do RDR2 e agoniza antes de morrer. Modo hostil procura NPCs/animais em até 500 m; jogadores online nunca são alvo." : "The clone keeps 800 health. At the fatal state it uses RDR2's native incapacitation/bleedout and writhes before dying. Hostile mode searches NPCs/animals up to 500 m; online players are never targeted.");
				ImGui::TextDisabled("%s", Localization::IsPortuguese() ? "Cada clone recebe uma máscara Halloween do Online aleatória: Freak, Horror, Masquerade, Slaughter, Creature ou Swine, com variações de cor." : "Each clone receives a random Online Halloween mask: Freak, Horror, Masquerade, Slaughter, Creature or Swine, including color variants.");
				ImGui::TextDisabled("%s", Localization::IsPortuguese() ? "Rifles/repetidoras/snipers: precisão 89. Pistolas/revólveres: 60. Variantes História/Pistoleiro dependem do asset da sessão." : "Rifles/repeaters/snipers: 89 accuracy. Pistols/revolvers: 60. Story/Gunslinger variants depend on the session asset.");

				ImGui::Spacing();
				ImGui::SeparatorText(Localization::IsPortuguese() ? "POSIÇÃO" : "POSITION");
				if (ImGui::Button(Localization::IsPortuguese() ? "CRIAR AO MEU LADO" : "CREATE NEXT TO ME", ImVec2(205.0f, 34.0f)))
				{
					const CloneOptions options = GetOptions();
					FiberPool::Push([options] { SpawnManualCloneNearPlayer(options); });
				}
				ImGui::SameLine();
				if (ImGui::Button(Localization::IsPortuguese() ? "POSICIONAR COM FREECAM" : "PLACE WITH FREECAM", ImVec2(225.0f, 34.0f)))
				{
					const CloneOptions options = GetOptions();
					FiberPool::Push([options] {
						SpawnPlacement placement{};
						if (PickCloneSpawnWithFreecam(placement))
							SpawnManualCloneAt(options, placement.Position, placement.Heading);
					});
				}
			}

			std::string_view GetMenuLabel() const override { return Localization::IsPortuguese() ? "Criar meu clone" : "Create my clone"; }
			std::string GetMenuValue() const override { return Localization::IsPortuguese() ? kCloneWeapons[m_WeaponIndex].LabelPt : kCloneWeapons[m_WeaponIndex].LabelEn; }
			std::string_view GetMenuDescription() const override { return Localization::IsPortuguese() ? "Cria clones locais com arma, IA, freecam, máscara Halloween aleatória e morte por sangramento." : "Creates local clones with weapon, AI, freecam, random Halloween mask and bleedout death."; }
			bool RequiresImGuiEditor() const override { return true; }
			float GetPreferredEditorHeight() const override { return 690.0f; }
		};
	}

	std::shared_ptr<UIItem> CreateManualCloneItem()
	{
		return std::make_shared<ManualCloneItem>();
	}
}
