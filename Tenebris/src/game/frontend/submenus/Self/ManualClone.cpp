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

		enum class CloneMode : int
		{
			Bodyguard = 0,
			FrenzyNpcs = 1,
			AttackOwner = 2,
		};

		struct CloneWeapon
		{
			const char* LabelPt;
			const char* LabelEn;
			const char* HashName;
		};

		struct CloneOptions
		{
			int WeaponIndex{};
			CloneMode Mode{CloneMode::Bodyguard};
			bool FatalGore{true};
			int SourcePlayerId{-1};
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

		struct ManagedClone
		{
			int Ped{};
			int SourcePlayerId{-1};
			CloneMode Mode{CloneMode::Bodyguard};
		};

		constexpr int kCloneHealth = 800;
		constexpr float kEngageRadius = 135.0f;
		constexpr float kGuardRadius = 150.0f;
		constexpr float kTargetLeashRadius = 165.0f;
		constexpr float kApproachDistance = 72.0f;
		constexpr float kHorseSearchRadius = 36.0f;
		constexpr float kSocialRadius = 11.0f;
		constexpr std::size_t kMaxActiveClones = 8;
		constexpr float kBleedOutSeconds = 12.0f;
		constexpr int kBleedOutMilliseconds = 12000;
		constexpr int kIncapacitationThreshold = 20;
		constexpr int kHalloweenMaskFamilies = 6;
		constexpr int kHalloweenMaskVariantsPerFamily = 10;
		constexpr auto kPedCacheInterval = 450ms;
		constexpr auto kBrainTick = 150ms;
		constexpr auto kCombatDecisionInterval = 650ms;
		constexpr auto kSocialDecisionInterval = 900ms;
		constexpr auto kHorseDecisionInterval = 2500ms;
		constexpr auto kIdleSmokeDelay = 10s;
		constexpr auto kIdleLeaveDelay = 25s;

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

		std::vector<ManagedClone> g_ManagedClones;
		std::vector<CachedPed> g_PedCache;
		Clock::time_point g_NextPedCacheRefresh{};
		Hash g_LastHalloweenMask{};
		bool g_BodyguardsBetrayed{};
		std::uint32_t g_TotalSpawned{};

		std::mt19937& Rng()
		{
			static std::mt19937 rng{static_cast<std::mt19937::result_type>(Clock::now().time_since_epoch().count())};
			return rng;
		}

		int RandomInt(int min, int max)
		{
			return std::uniform_int_distribution<int>(min, max)(Rng());
		}

		float DistanceSquared(const Vector3& a, const Vector3& b)
		{
			const float dx = a.x - b.x;
			const float dy = a.y - b.y;
			const float dz = a.z - b.z;
			return dx * dx + dy * dy + dz * dz;
		}

		bool IsManagedClone(int ped)
		{
			return std::any_of(g_ManagedClones.begin(), g_ManagedClones.end(), [ped](const ManagedClone& c) { return c.Ped == ped; });
		}

		std::size_t ActiveCloneCount()
		{
			return static_cast<std::size_t>(std::count_if(g_ManagedClones.begin(), g_ManagedClones.end(), [](const ManagedClone& c) {
				return c.Ped && ENTITY::DOES_ENTITY_EXIST(c.Ped) && !ENTITY::IS_ENTITY_DEAD(c.Ped);
			}));
		}

		void PruneManagedClones(bool deleteDead)
		{
			int deadDeleted{};
			g_ManagedClones.erase(std::remove_if(g_ManagedClones.begin(), g_ManagedClones.end(), [&](const ManagedClone& c) {
				if (!c.Ped || !ENTITY::DOES_ENTITY_EXIST(c.Ped))
					return true;
				if (deleteDead && ENTITY::IS_ENTITY_DEAD(c.Ped) && deadDeleted < 4)
				{
					int ped = c.Ped;
					PED::DELETE_PED(&ped);
					++deadDeleted;
					return true;
				}
				return false;
			}), g_ManagedClones.end());

			const bool hasLivingGuard = std::any_of(g_ManagedClones.begin(), g_ManagedClones.end(), [](const ManagedClone& c) {
				return c.Mode == CloneMode::Bodyguard && c.Ped && ENTITY::DOES_ENTITY_EXIST(c.Ped) && !ENTITY::IS_ENTITY_DEAD(c.Ped);
			});
			if (!hasLivingGuard)
				g_BodyguardsBetrayed = false;
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
				g_PedCache.push_back({handle, ENTITY::GET_ENTITY_COORDS(handle, true, false), PED::IS_PED_A_PLAYER(handle), ENTITY::IS_ENTITY_DEAD(handle)});
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
			if (IsSidearm(weaponName)) return 60;
			if (IsLongGun(weaponName)) return 89;
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

		int ResolveSourcePed(int sourcePlayerId)
		{
			if (sourcePlayerId < 0)
				sourcePlayerId = PLAYER::PLAYER_ID();
			const int ped = PLAYER::GET_PLAYER_PED_SCRIPT_INDEX(sourcePlayerId);
			return ped && ENTITY::DOES_ENTITY_EXIST(ped) ? ped : 0;
		}

		int CreateLocalCloneShell(int sourceHandle, const Vector3& spawn, float heading)
		{
			if (!sourceHandle || !ENTITY::DOES_ENTITY_EXIST(sourceHandle))
				return 0;
			const Hash model = ENTITY::GET_ENTITY_MODEL(sourceHandle);
			if (!EnsureModelLoaded(model))
				return 0;

			int clone = PED::CREATE_PED(model, spawn.x, spawn.y, spawn.z, heading, false, 0, 0, 0);
			STREAMING::SET_MODEL_AS_NO_LONGER_NEEDED(model);
			if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone) || PED::IS_PED_A_PLAYER(clone))
			{
				if (clone && ENTITY::DOES_ENTITY_EXIST(clone) && !PED::IS_PED_A_PLAYER(clone)) PED::DELETE_PED(&clone);
				return 0;
			}

			ENTITY::SET_ENTITY_AS_MISSION_ENTITY(clone, true, true);
			ScriptMgr::Yield();
			if (!ENTITY::DOES_ENTITY_EXIST(sourceHandle) || !ENTITY::DOES_ENTITY_EXIST(clone))
			{
				if (ENTITY::DOES_ENTITY_EXIST(clone)) PED::DELETE_PED(&clone);
				return 0;
			}

			PED::CLONE_PED_TO_TARGET(sourceHandle, clone);
			ScriptMgr::Yield();
			if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone) || PED::IS_PED_A_PLAYER(clone))
			{
				if (clone && ENTITY::DOES_ENTITY_EXIST(clone) && !PED::IS_PED_A_PLAYER(clone)) PED::DELETE_PED(&clone);
				return 0;
			}
			PED::_UPDATE_PED_VARIATION(clone, 0, 1, 1, 1, 0);
			ScriptMgr::Yield();
			return ENTITY::DOES_ENTITY_EXIST(clone) ? clone : 0;
		}

		void ApplyPlayerPromptName(int clone, int sourcePlayerId)
		{
			if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone)) return;
			if (sourcePlayerId < 0) sourcePlayerId = PLAYER::PLAYER_ID();
			const char* playerName = PLAYER::GET_PLAYER_NAME(sourcePlayerId);
			if (!playerName || !*playerName) return;
			const char* literalString = "LITERAL_STRING";
			PED::_SET_PED_PROMPT_NAME(clone, MISC::VAR_STRING(10, literalString, playerName));
		}

		void ApplyRandomHalloweenMask(int clone)
		{
			if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone)) return;
			const Hash model = ENTITY::GET_ENTITY_MODEL(clone);
			char gender{};
			if (model == Joaat("mp_male")) gender = 'm';
			else if (model == Joaat("mp_female")) gender = 'f';
			else return;

			std::uniform_int_distribution<int> familyDist(0, kHalloweenMaskFamilies - 1);
			std::uniform_int_distribution<int> variantDist(1, kHalloweenMaskVariantsPerFamily);
			Hash component{};
			char componentName[80]{};
			for (int attempt = 0; attempt < 8; ++attempt)
			{
				std::snprintf(componentName, sizeof(componentName), "clothing_item_%c_halloween_mask_%03d_var_%03d", gender, familyDist(Rng()), variantDist(Rng()));
				component = Joaat(componentName);
				if (component != g_LastHalloweenMask) break;
			}
			if (!component) return;
			PED::_SET_PED_COMPONENT_ENABLED(clone, component, true, true, true);
			PED::_UPDATE_PED_VARIATION(clone, 0, 1, 1, 1, 0);
			g_LastHalloweenMask = component;
		}

		void ConfigureBleedout(int clone)
		{
			if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone)) return;
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

		bool IsValidNpcTarget(int candidate, int clone, int owner)
		{
			if (!candidate || candidate == clone || candidate == owner || !ENTITY::DOES_ENTITY_EXIST(candidate) || ENTITY::IS_ENTITY_DEAD(candidate)) return false;
			if (PED::IS_PED_A_PLAYER(candidate) || IsManagedClone(candidate)) return false;
			return candidate != GetPlayerMountHandle();
		}

		bool IsWithinRange(int a, int b, float radius)
		{
			if (!a || !b || !ENTITY::DOES_ENTITY_EXIST(a) || !ENTITY::DOES_ENTITY_EXIST(b)) return false;
			return DistanceSquared(ENTITY::GET_ENTITY_COORDS(a, true, false), ENTITY::GET_ENTITY_COORDS(b, true, false)) <= radius * radius;
		}

		bool HasClearShot(int clone, int target)
		{
			return clone && target && ENTITY::DOES_ENTITY_EXIST(clone) && ENTITY::DOES_ENTITY_EXIST(target) && ENTITY::HAS_ENTITY_CLEAR_LOS_TO_ENTITY(clone, target, 17);
		}

		int FindNearestNpcTarget(int clone, int owner, float radius)
		{
			const Vector3 origin = ENTITY::GET_ENTITY_COORDS(clone, true, false);
			const float radiusSq = radius * radius;
			const int ownerMount = GetPlayerMountHandle();
			int best{};
			float bestDistanceSq = radiusSq;
			for (const auto& ped : GetCachedPeds())
			{
				if (!ped.Handle || ped.Handle == clone || ped.Handle == owner || ped.Handle == ownerMount || ped.Player || ped.Dead || IsManagedClone(ped.Handle)) continue;
				const float d = DistanceSquared(origin, ped.Position);
				if (d < bestDistanceSq)
				{
					bestDistanceSq = d;
					best = ped.Handle;
				}
			}
			return IsValidNpcTarget(best, clone, owner) ? best : 0;
		}

		int FindBodyguardThreat(int clone, int owner)
		{
			if (!owner || !ENTITY::DOES_ENTITY_EXIST(owner)) return 0;
			const Vector3 ownerPos = ENTITY::GET_ENTITY_COORDS(owner, true, false);
			const float radiusSq = kGuardRadius * kGuardRadius;
			int best{};
			float bestDistanceSq = radiusSq;
			for (const auto& ped : GetCachedPeds())
			{
				if (!ped.Handle || ped.Player || ped.Dead || !IsValidNpcTarget(ped.Handle, clone, owner)) continue;
				if (!PED::IS_PED_IN_COMBAT(ped.Handle, owner) && !PED::IS_PED_IN_COMBAT(owner, ped.Handle) && !ENTITY::HAS_ENTITY_BEEN_DAMAGED_BY_ENTITY(owner, ped.Handle, true, true)) continue;
				const float d = DistanceSquared(ownerPos, ped.Position);
				if (d < bestDistanceSq)
				{
					bestDistanceSq = d;
					best = ped.Handle;
				}
			}
			return IsValidNpcTarget(best, clone, owner) ? best : 0;
		}

		int FindNearestFreeHorse(int clone, int owner)
		{
			if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone)) return 0;
			const Vector3 origin = ENTITY::GET_ENTITY_COORDS(clone, true, false);
			const float radiusSq = kHorseSearchRadius * kHorseSearchRadius;
			const int ownerMount = GetPlayerMountHandle();
			int best{};
			float bestDistanceSq = radiusSq;
			for (const auto& ped : GetCachedPeds())
			{
				if (!ped.Handle || ped.Handle == owner || ped.Handle == ownerMount || ped.Player || ped.Dead || IsManagedClone(ped.Handle)) continue;
				if (!PED::_IS_THIS_MODEL_A_HORSE(ENTITY::GET_ENTITY_MODEL(ped.Handle)) || !PED::_IS_MOUNT_SEAT_FREE(ped.Handle, -1)) continue;
				const float d = DistanceSquared(origin, ped.Position);
				if (d < bestDistanceSq)
				{
					bestDistanceSq = d;
					best = ped.Handle;
				}
			}
			return best;
		}

		bool IsLocalCameraLookingAt(int target, float maxDistance)
		{
			if (!target || !ENTITY::DOES_ENTITY_EXIST(target)) return false;
			const Vector3 cam = CAM::GET_GAMEPLAY_CAM_COORD();
			const Vector3 rot = CAM::GET_GAMEPLAY_CAM_ROT(2);
			const Vector3 targetPos = ENTITY::GET_ENTITY_COORDS(target, true, false);
			const float dx = targetPos.x - cam.x;
			const float dy = targetPos.y - cam.y;
			const float dz = targetPos.z - cam.z;
			const float lenSq = dx * dx + dy * dy + dz * dz;
			if (lenSq > maxDistance * maxDistance || lenSq < 0.001f) return false;
			const float len = std::sqrt(lenSq);
			const float pitch = rot.x * 0.017453292519943295f;
			const float yaw = rot.z * 0.017453292519943295f;
			const float cp = std::cos(pitch);
			const float fx = -std::sin(yaw) * cp;
			const float fy = std::cos(yaw) * cp;
			const float fz = std::sin(pitch);
			return fx * (dx / len) + fy * (dy / len) + fz * (dz / len) > 0.90f;
		}

		bool IsPedFacingClone(int sourcePed, int clone, float maxDistance)
		{
			if (!sourcePed || !clone || !ENTITY::DOES_ENTITY_EXIST(sourcePed) || !ENTITY::DOES_ENTITY_EXIST(clone)) return false;
			const Vector3 a = ENTITY::GET_ENTITY_COORDS(sourcePed, true, false);
			const Vector3 b = ENTITY::GET_ENTITY_COORDS(clone, true, false);
			const float dx = b.x - a.x;
			const float dy = b.y - a.y;
			const float lenSq = dx * dx + dy * dy;
			if (lenSq > maxDistance * maxDistance || lenSq < 0.001f) return false;
			const float len = std::sqrt(lenSq);
			const float heading = ENTITY::GET_ENTITY_HEADING(sourcePed) * 0.017453292519943295f;
			const float fx = -std::sin(heading);
			const float fy = std::cos(heading);
			return fx * (dx / len) + fy * (dy / len) > 0.84f;
		}

		int FindInteractingEmotePlayer(int clone)
		{
			const int localId = PLAYER::PLAYER_ID();
			const int localPed = PLAYER::PLAYER_PED_ID();
			if (TASK::IS_EMOTE_TASK_RUNNING(localPed, 0) && IsLocalCameraLookingAt(clone, kSocialRadius)) return localPed;
			for (int id = 0; id < 32; ++id)
			{
				if (id == localId) continue;
				const int ped = PLAYER::GET_PLAYER_PED_SCRIPT_INDEX(id);
				if (!ped || !ENTITY::DOES_ENTITY_EXIST(ped) || ENTITY::IS_ENTITY_DEAD(ped)) continue;
				if (TASK::IS_EMOTE_TASK_RUNNING(ped, 0) && IsPedFacingClone(ped, clone, kSocialRadius)) return ped;
			}
			return 0;
		}

		void PlayRareSocialReaction(int clone, bool insult = false)
		{
			if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone) || PED::IS_PED_IN_COMBAT(clone, 0)) return;
			static constexpr std::array<Hash, 4> social{
			    Joaat("KIT_EMOTE_GREET_WAVENEAR_1"),
			    Joaat("KIT_EMOTE_GREET_THUMBSUP_1"),
			    Joaat("KIT_EMOTE_REACTION_DISAGREE_1"),
			    Joaat("KIT_EMOTE_TAUNT_WAR_CRY_1")};
			const Hash emote = insult ? Joaat("KIT_EMOTE_TAUNT_INSULT_1") : social[RandomInt(0, static_cast<int>(social.size()) - 1)];
			TASK::TASK_PLAY_EMOTE_WITH_HASH(clone, 2, 0, emote, true, true, false, false, false);
		}

		template <std::size_t N>
		bool DamageBoneMatches(int ped, int damageBone, const std::array<int, N>& boneTags)
		{
			for (int boneTag : boneTags)
			{
				if (damageBone == boneTag) return true;
				const int boneIndex = PED::GET_PED_BONE_INDEX(ped, boneTag);
				if (boneIndex >= 0 && damageBone == boneIndex) return true;
			}
			return false;
		}

		void ApplyFatalGore(int clone, int lastDamageBone, Hash weapon)
		{
			if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone) || !lastDamageBone) return;
			static constexpr std::array<int, 2> head{21030, 27981};
			static constexpr std::array<int, 11> torso{11569, 14410, 14411, 14412, 14413, 14414, 14415, 14416, 6757, 6758, 57309};
			if (DamageBoneMatches(clone, lastDamageBone, head))
				PED::EXPLODE_PED_HEAD(clone, weapon ? weapon : Joaat("WEAPON_REPEATER_CARBINE"));
			else if (DamageBoneMatches(clone, lastDamageBone, torso))
				PED::APPLY_PED_DAMAGE_PACK(clone, "PD_Human_carcass_Hvy", 1.0f, 1.0f);
		}

		void ConfigureCloneCombat(int clone, CloneMode mode, std::string_view weaponName)
		{
			PED::SET_PED_KEEP_TASK(clone, true);
			PED::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(clone, false);
			PED::SET_PED_COMBAT_ABILITY(clone, 3);
			PED::SET_PED_COMBAT_MOVEMENT(clone, 2);
			PED::SET_PED_COMBAT_RANGE(clone, 2);
			PED::SET_PED_ACCURACY(clone, AccuracyForWeapon(weaponName));
			PED::SET_PED_SEEING_RANGE(clone, 165.0f);
			PED::SET_PED_HEARING_RANGE(clone, 150.0f);
			PED::SET_PED_MOVE_RATE_OVERRIDE(clone, mode == CloneMode::Bodyguard ? 1.25f : 1.35f);
			for (int attribute : {0, 1, 5, 13, 21, 25, 31, 39, 41, 42, 46, 49, 54, 58, 63, 68, 78, 80, 81, 91, 92, 93, 113, 115})
				PED::SET_PED_COMBAT_ATTRIBUTES(clone, attribute, true);
			PED::SET_PED_COMBAT_ATTRIBUTES(clone, 27, false);
		}

		bool RecoverIntoBleedout(int clone)
		{
			if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone)) return false;
			PED::RESURRECT_PED(clone);
			PED::REVIVE_INJURED_PED(clone);
			if (!ENTITY::DOES_ENTITY_EXIST(clone)) return false;
			ENTITY::SET_ENTITY_HEALTH(clone, 2, 0);
			ConfigureBleedout(clone);
			PED::SET_PED_TO_RAGDOLL(clone, 900, 900, 0, false, false, false);
			return true;
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

		void TaskTacticalCombat(int clone, int target)
		{
			if (!IsValidNpcTarget(target, clone, PLAYER::PLAYER_PED_ID()) && target != PLAYER::PLAYER_PED_ID()) return;
			const float dSq = DistanceSquared(ENTITY::GET_ENTITY_COORDS(clone, true, false), ENTITY::GET_ENTITY_COORDS(target, true, false));
			if (dSq > kApproachDistance * kApproachDistance)
			{
				TASK::TASK_GO_TO_ENTITY(clone, target, 2600, 42.0f, 2.2f, 0.0f, 0);
				return;
			}

			if (!HasClearShot(clone, target))
			{
				TASK::TASK_SEEK_COVER_FROM_PED(clone, target, 2200, true, 0, 0);
				return;
			}

			if (ENTITY::HAS_ENTITY_BEEN_DAMAGED_BY_ENTITY(clone, target, true, true))
			{
				ENTITY::CLEAR_ENTITY_LAST_DAMAGE_ENTITY(clone);
				TASK::TASK_SEEK_COVER_FROM_PED(clone, target, 1800, true, 0, 0);
				return;
			}
			TASK::TASK_COMBAT_PED(clone, target, 0, 16);
		}

		bool TryMountForFollow(int clone, int owner)
		{
			if (!clone || !owner || !ENTITY::DOES_ENTITY_EXIST(clone) || !ENTITY::DOES_ENTITY_EXIST(owner)) return false;
			if (PED::IS_PED_ON_MOUNT(clone))
			{
				TASK::TASK_FOLLOW_TO_OFFSET_OF_ENTITY(clone, owner, 2.0f, -3.5f, 0.0f, 3.0f, -1, 5.0f, true, true, false, true, true, true);
				return true;
			}

			const int ownerMount = GetPlayerMountHandle();
			if (ownerMount && ENTITY::DOES_ENTITY_EXIST(ownerMount) && PED::_IS_MOUNT_SEAT_FREE(ownerMount, 0))
			{
				TASK::TASK_MOUNT_ANIMAL(clone, ownerMount, 5000, 0, 2.0f, 1, 0, 0);
				return true;
			}

			const int horse = FindNearestFreeHorse(clone, owner);
			if (horse)
			{
				TASK::TASK_MOUNT_ANIMAL(clone, horse, 5000, -1, 2.0f, 1, 0, 0);
				return true;
			}
			return false;
		}

		void RunCloneBrain(int clone, int owner, CloneOptions options, Hash weapon)
		{
			int currentTarget{};
			int issuedTarget{};
			int lastDamageBone{};
			bool fatalRecoveryUsed{};
			bool sawIncapacitation{};
			bool insultedOnBetrayal{};
			bool smoking{};
			bool wandering{};
			auto nextDecision = Clock::now();
			auto nextSocial = Clock::now() + 3s;
			auto nextHorse = Clock::now();
			auto idleSince = Clock::now();
			auto fallbackDeathAt = Clock::time_point::max();

			while (clone && ENTITY::DOES_ENTITY_EXIST(clone))
			{
				int damageBone{};
				if (PED::GET_PED_LAST_DAMAGE_BONE(clone, &damageBone) && damageBone) lastDamageBone = damageBone;

				if (options.Mode == CloneMode::Bodyguard && !g_BodyguardsBetrayed && ENTITY::HAS_ENTITY_BEEN_DAMAGED_BY_ENTITY(clone, owner, true, true))
				{
					g_BodyguardsBetrayed = true;
					ENTITY::CLEAR_ENTITY_LAST_DAMAGE_ENTITY(clone);
				}

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
					if (options.FatalGore) ApplyFatalGore(clone, lastDamageBone, weapon);
					break;
				}

				const auto now = Clock::now();
				if (fatalRecoveryUsed && !sawIncapacitation && now >= fallbackDeathAt)
				{
					ENTITY::SET_ENTITY_HEALTH(clone, 0, 0);
					ScriptMgr::Yield(kBrainTick);
					continue;
				}

				CloneMode effectiveMode = options.Mode;
				if (options.Mode == CloneMode::Bodyguard && g_BodyguardsBetrayed)
				{
					effectiveMode = CloneMode::AttackOwner;
					if (!insultedOnBetrayal && !PED::IS_PED_IN_COMBAT(clone, owner))
					{
						PlayRareSocialReaction(clone, true);
						insultedOnBetrayal = true;
					}
				}

				if (now >= nextDecision)
				{
					bool keepCurrent = currentTarget && ENTITY::DOES_ENTITY_EXIST(currentTarget) && !ENTITY::IS_ENTITY_DEAD(currentTarget) && IsWithinRange(clone, currentTarget, kTargetLeashRadius);
					if (effectiveMode != CloneMode::AttackOwner && keepCurrent) keepCurrent = IsValidNpcTarget(currentTarget, clone, owner);
					if (effectiveMode == CloneMode::Bodyguard && keepCurrent)
						keepCurrent = PED::IS_PED_IN_COMBAT(currentTarget, owner) || PED::IS_PED_IN_COMBAT(owner, currentTarget) || ENTITY::HAS_ENTITY_BEEN_DAMAGED_BY_ENTITY(owner, currentTarget, true, true);

					if (!keepCurrent)
					{
						currentTarget = effectiveMode == CloneMode::AttackOwner ? owner :
						    (effectiveMode == CloneMode::Bodyguard ? FindBodyguardThreat(clone, owner) : FindNearestNpcTarget(clone, owner, kEngageRadius));
						issuedTarget = 0;
					}

					if (currentTarget && ENTITY::DOES_ENTITY_EXIST(currentTarget) && !ENTITY::IS_ENTITY_DEAD(currentTarget))
					{
						smoking = false;
						wandering = false;
						idleSince = now;
						if (issuedTarget != currentTarget || !PED::IS_PED_IN_COMBAT(clone, currentTarget) || !HasClearShot(clone, currentTarget))
						{
							TaskTacticalCombat(clone, currentTarget);
							issuedTarget = currentTarget;
						}
					}
					else if (effectiveMode == CloneMode::Bodyguard)
					{
						issuedTarget = 0;
						const bool ownerMoving = ENTITY::GET_ENTITY_SPEED(owner) > 0.7f || GetPlayerMountHandle();
						if (ownerMoving)
						{
							smoking = false;
							idleSince = now;
							if (now >= nextHorse)
							{
								if (!TryMountForFollow(clone, owner))
									TASK::TASK_FOLLOW_TO_OFFSET_OF_ENTITY(clone, owner, 1.8f, -2.8f, 0.0f, 2.2f, -1, 2.5f, true, true, false, true, true, true);
								nextHorse = now + kHorseDecisionInterval;
							}
						}
						else if (ENTITY::GET_ENTITY_SPEED(clone) < 0.25f)
						{
							const auto idleFor = now - idleSince;
							const Vector3 pos = ENTITY::GET_ENTITY_COORDS(clone, true, false);
							if (idleFor >= kIdleSmokeDelay && !smoking && !PATHFIND::IS_POINT_ON_ROAD(pos.x, pos.y, pos.z, 0))
							{
								TASK::TASK_START_SCENARIO_IN_PLACE_HASH(clone, Joaat("WORLD_HUMAN_SMOKE"), 10000, true, 0, ENTITY::GET_ENTITY_HEADING(clone), false);
								smoking = true;
							}
						}
						else idleSince = now;
					}
					else
					{
						issuedTarget = 0;
						if (ENTITY::GET_ENTITY_SPEED(clone) < 0.25f)
						{
							const auto idleFor = now - idleSince;
							const Vector3 pos = ENTITY::GET_ENTITY_COORDS(clone, true, false);
							if (idleFor >= kIdleSmokeDelay && !smoking && !PATHFIND::IS_POINT_ON_ROAD(pos.x, pos.y, pos.z, 0))
							{
								TASK::TASK_START_SCENARIO_IN_PLACE_HASH(clone, Joaat("WORLD_HUMAN_SMOKE"), 9000, true, 0, ENTITY::GET_ENTITY_HEADING(clone), false);
								smoking = true;
							}
							if (idleFor >= kIdleLeaveDelay && !wandering)
							{
								const int horse = FindNearestFreeHorse(clone, owner);
								if (horse) TASK::TASK_MOUNT_ANIMAL(clone, horse, 5000, -1, 2.0f, 1, 0, 0);
								else TASK::TASK_WANDER_STANDARD(clone, 1.0f, 0);
								wandering = true;
							}
						}
						else idleSince = now;
					}

					nextDecision = now + kCombatDecisionInterval;
				}

				if (now >= nextSocial && !currentTarget && !PED::IS_PED_IN_COMBAT(clone, 0))
				{
					if (FindInteractingEmotePlayer(clone))
						PlayRareSocialReaction(clone, false);
					else if (!smoking && RandomInt(1, 100) <= 8)
						TASK::TASK_PLAY_EMOTE_WITH_HASH(clone, 2, 0, Joaat("KIT_EMOTE_TAUNT_WAR_CRY_1"), true, true, false, false, false);
					nextSocial = now + kSocialDecisionInterval;
				}

				ScriptMgr::Yield(kBrainTick);
			}
		}

		void SpawnManualCloneAt(CloneOptions options, const Vector3& spawn, float heading)
		{
			PruneManagedClones(false);
			if (ActiveCloneCount() >= kMaxActiveClones)
			{
				PruneManagedClones(true);
				if (ActiveCloneCount() >= kMaxActiveClones)
				{
					Notifications::Show("Tenebris", Localization::IsPortuguese() ? "Já existem 8 clones vivos. Mate/afaste algum antes de criar outro." : "There are already 8 living clones. Remove one before creating another.", NotificationType::Warning, 2600);
					return;
				}
			}

			auto self = Self::GetPed();
			if (!self.IsValid() || self.GetHealth() <= 0) return;
			const int owner = self.GetHandle();
			if (options.SourcePlayerId < 0) options.SourcePlayerId = PLAYER::PLAYER_ID();
			int sourcePed = ResolveSourcePed(options.SourcePlayerId);
			if (!sourcePed)
			{
				Notifications::Show("Tenebris", Localization::IsPortuguese() ? "O jogador escolhido não está mais disponível na sessão." : "The selected player is no longer available in the session.", NotificationType::Warning, 2600);
				return;
			}

			int clone = CreateLocalCloneShell(sourcePed, spawn, heading);
			if (!clone)
			{
				Notifications::Show("Tenebris", Localization::IsPortuguese() ? "Não foi possível criar o clone com segurança." : "Could not create the clone safely.", NotificationType::Warning, 2600);
				return;
			}

			ENTITY::PLACE_ENTITY_ON_GROUND_PROPERLY(clone, true);
			ENTITY::SET_ENTITY_MAX_HEALTH(clone, kCloneHealth);
			ENTITY::SET_ENTITY_HEALTH(clone, kCloneHealth, 0);
			ApplyPlayerPromptName(clone, options.SourcePlayerId);
			ApplyRandomHalloweenMask(clone);
			ConfigureBleedout(clone);
			options.WeaponIndex = std::clamp(options.WeaponIndex, 0, static_cast<int>(kCloneWeapons.size()) - 1);
			const Hash weapon = EquipCloneWeapon(clone, options.WeaponIndex);
			ConfigureCloneCombat(clone, options.Mode, kCloneWeapons[options.WeaponIndex].HashName);
			ENTITY::CLEAR_ENTITY_LAST_DAMAGE_ENTITY(clone);
			g_ManagedClones.push_back({clone, options.SourcePlayerId, options.Mode});
			g_NextPedCacheRefresh = {};

			++g_TotalSpawned;
			if (g_TotalSpawned % 5 == 0) PruneManagedClones(true);

			FiberPool::Push([clone, owner, options, weapon] { RunCloneBrain(clone, owner, options, weapon); });
			const char* sourceName = PLAYER::GET_PLAYER_NAME(options.SourcePlayerId);
			std::string msg = Localization::IsPortuguese() ? "Clone criado" : "Clone created";
			if (sourceName && *sourceName) msg += std::string(" - ") + sourceName;
			Notifications::Show("Tenebris", msg, NotificationType::Success, 2200);
		}

		void SpawnManualCloneNearPlayer(CloneOptions options)
		{
			auto self = Self::GetPed();
			if (!self.IsValid()) return;
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
			if (!self.IsValid()) return false;
			const bool reopenMenu = GUI::IsOpen();
			if (reopenMenu) GUI::Toggle();
			int camera = CAM::CREATE_CAM("DEFAULT_SCRIPTED_CAMERA", 0);
			if (!camera)
			{
				if (reopenMenu && !GUI::IsOpen()) GUI::Toggle();
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
			Notifications::Show("Tenebris", Localization::IsPortuguese() ? "POSICIONAMENTO: mova a câmera; ENTER confirma; BACK cancela." : "PLACEMENT: move camera; ENTER confirms; BACK cancels.", NotificationType::Info, 5000);

			bool accepted{};
			float acceleration{};
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
				if (MISC::GET_GROUND_Z_FOR_3D_COORD(position.x, position.y, position.z + 100.0f, &groundZ, 0)) marker.z = groundZ;
				else marker.z -= 1.0f;
				GRAPHICS::_DRAW_MARKER(0x6903B113, marker.x, marker.y, marker.z + 0.03f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 1.25f, 1.25f, 1.25f, 255, 255, 255, 230, false, true, 2, false, nullptr, nullptr, false);
				if (PAD::IS_DISABLED_CONTROL_JUST_PRESSED(0, (Hash)NativeInputs::INPUT_FRONTEND_ACCEPT))
				{
					placement.Position = marker;
					placement.Heading = rotation.z;
					accepted = true;
					break;
				}
				if (PAD::IS_DISABLED_CONTROL_JUST_PRESSED(0, (Hash)NativeInputs::INPUT_FRONTEND_CANCEL)) break;
				ScriptMgr::Yield();
			}

			CAM::SET_CAM_ACTIVE(camera, false);
			CAM::RENDER_SCRIPT_CAMS(false, true, 350, true, true, 0);
			CAM::DESTROY_CAM(camera, false);
			STREAMING::CLEAR_FOCUS();
			self.SetFrozen(false);
			self.SetVisible(true);
			if (reopenMenu && !GUI::IsOpen()) GUI::Toggle();
			return accepted;
		}

		const char* ModeLabelPt(CloneMode mode)
		{
			switch (mode)
			{
			case CloneMode::Bodyguard: return "Guarda-costas";
			case CloneMode::FrenzyNpcs: return "Frenético contra NPCs";
			case CloneMode::AttackOwner: return "Atacar somente eu";
			}
			return "Guarda-costas";
		}

		const char* ModeLabelEn(CloneMode mode)
		{
			switch (mode)
			{
			case CloneMode::Bodyguard: return "Bodyguard";
			case CloneMode::FrenzyNpcs: return "Frenzy against NPCs";
			case CloneMode::AttackOwner: return "Attack only me";
			}
			return "Bodyguard";
		}

		class ManualCloneItem final : public UIItem
		{
			int m_WeaponIndex{};
			CloneMode m_Mode{CloneMode::Bodyguard};
			bool m_FatalGore{true};
			int m_SourcePlayerId{-1};

			CloneOptions GetOptions() const
			{
				CloneOptions out{m_WeaponIndex, m_Mode, m_FatalGore, m_SourcePlayerId};
				if (out.SourcePlayerId < 0) out.SourcePlayerId = PLAYER::PLAYER_ID();
				return out;
			}

			std::string SourcePreview() const
			{
				const int id = m_SourcePlayerId < 0 ? PLAYER::PLAYER_ID() : m_SourcePlayerId;
				const char* name = PLAYER::GET_PLAYER_NAME(id);
				if (!name || !*name) return Localization::IsPortuguese() ? "Eu" : "Me";
				if (id == PLAYER::PLAYER_ID()) return std::string(Localization::IsPortuguese() ? "Eu - " : "Me - ") + name;
				return name;
			}

		public:
			void Draw() override
			{
				ImGui::TextUnformatted(Localization::IsPortuguese() ? "Criar clone inteligente" : "Create smart clone");
				ImGui::Spacing();

				ImGui::TextUnformatted(Localization::IsPortuguese() ? "Aparência / jogador clonado" : "Appearance / cloned player");
				const std::string sourcePreview = SourcePreview();
				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::BeginCombo("##CloneSourcePlayer", sourcePreview.c_str()))
				{
					const int localId = PLAYER::PLAYER_ID();
					const char* localName = PLAYER::GET_PLAYER_NAME(localId);
					std::string localLabel = std::string(Localization::IsPortuguese() ? "Eu" : "Me") + (localName && *localName ? std::string(" - ") + localName : "");
					if (ImGui::Selectable(localLabel.c_str(), m_SourcePlayerId < 0 || m_SourcePlayerId == localId)) m_SourcePlayerId = localId;
					for (int id = 0; id < 32; ++id)
					{
						if (id == localId) continue;
						const int ped = PLAYER::GET_PLAYER_PED_SCRIPT_INDEX(id);
						const char* name = PLAYER::GET_PLAYER_NAME(id);
						if (!ped || !ENTITY::DOES_ENTITY_EXIST(ped) || !name || !*name) continue;
						if (ImGui::Selectable(name, m_SourcePlayerId == id)) m_SourcePlayerId = id;
					}
					ImGui::EndCombo();
				}

				ImGui::TextUnformatted(Localization::IsPortuguese() ? "Arma" : "Weapon");
				const char* weaponPreview = Localization::IsPortuguese() ? kCloneWeapons[m_WeaponIndex].LabelPt : kCloneWeapons[m_WeaponIndex].LabelEn;
				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::BeginCombo("##ManualCloneWeapon", weaponPreview))
				{
					for (int i = 0; i < static_cast<int>(kCloneWeapons.size()); ++i)
					{
						const char* label = Localization::IsPortuguese() ? kCloneWeapons[i].LabelPt : kCloneWeapons[i].LabelEn;
						if (ImGui::Selectable(label, m_WeaponIndex == i)) m_WeaponIndex = i;
					}
					ImGui::EndCombo();
				}

				ImGui::Spacing();
				ImGui::SeparatorText(Localization::IsPortuguese() ? "COMPORTAMENTO" : "BEHAVIOR");
				const char* modePreview = Localization::IsPortuguese() ? ModeLabelPt(m_Mode) : ModeLabelEn(m_Mode);
				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::BeginCombo("##CloneBehavior", modePreview))
				{
					for (int i = 0; i < 3; ++i)
					{
						const auto mode = static_cast<CloneMode>(i);
						const char* label = Localization::IsPortuguese() ? ModeLabelPt(mode) : ModeLabelEn(mode);
						if (ImGui::Selectable(label, m_Mode == mode)) m_Mode = mode;
					}
					ImGui::EndCombo();
				}
				ImGui::Checkbox(Localization::IsPortuguese() ? "Gore final após a agonia" : "Final gore after bleedout", &m_FatalGore);
				ImGui::TextWrapped("%s", Localization::IsPortuguese() ?
				    "Guarda-costas reage a NPCs/animais que entrarem em combate com você, usa cobertura e não mira jogadores reais. Frenético caça somente NPCs em alcance moderado. Atacar somente eu faz o clone atacar apenas seu personagem." :
				    "Bodyguard reacts to NPCs/animals fighting you, uses cover, and never targets real players. Frenzy hunts only NPCs at moderate range. Attack only me targets only your character.");
				ImGui::TextDisabled("%s", Localization::IsPortuguese() ?
				    "Alcance de combate reduzido (~135 m), linha de visão obrigatória para atirar, cobertura quando bloqueado/ferido e IA com cavalo/ociosidade." :
				    "Reduced combat range (~135 m), clear line of sight before firing, cover when blocked/hurt, plus horse/idle AI.");
				ImGui::TextDisabled("%s", Localization::IsPortuguese() ?
				    "Se você bater em um guarda-costas, todos os guarda-costas vivos insultam e traem você. A cada 5 clones, até 4 cadáveres antigos são removidos." :
				    "If you hit a bodyguard, all living bodyguards insult and betray you. Every 5 spawns, up to 4 old clone corpses are removed.");

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
						if (PickCloneSpawnWithFreecam(placement)) SpawnManualCloneAt(options, placement.Position, placement.Heading);
					});
				}
			}

			std::string_view GetMenuLabel() const override { return Localization::IsPortuguese() ? "Criar meu clone" : "Create my clone"; }
			std::string GetMenuValue() const override { return Localization::IsPortuguese() ? ModeLabelPt(m_Mode) : ModeLabelEn(m_Mode); }
			std::string_view GetMenuDescription() const override { return Localization::IsPortuguese() ? "Cria clones locais seus ou de jogadores da sessão com IA tática, cobertura, cavalo, freecam e bleedout." : "Creates local clones of you or session players with tactical AI, cover, horse following, freecam and bleedout."; }
			bool RequiresImGuiEditor() const override { return true; }
			float GetPreferredEditorHeight() const override { return 760.0f; }
		};
	}

	std::shared_ptr<UIItem> CreateManualCloneItem()
	{
		return std::make_shared<ManualCloneItem>();
	}
}
