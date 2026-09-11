#include "ManualClone.hpp"
#include "ManualCloneEmotes.hpp"

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
#include <cstdint>
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

		enum class CloneMode : int { Bodyguard = 0, FrenzyNpcs = 1, AttackOwner = 2, BrawlExpedition = 3 };

		struct CloneWeapon { const char* LabelPt; const char* LabelEn; const char* HashName; };
		struct CloneOptions
		{
			int WeaponIndex{};
			CloneMode Mode{CloneMode::Bodyguard};
			int SourcePlayerId{-1};
			bool Networked{};
			bool EmoteFlourish{true};
		};
		struct CachedPed { int Handle{}; Vector3 Position{}; bool Player{}; bool Dead{}; };
		struct ManagedClone
		{
			int Ped{};
			int SourcePlayerId{-1};
			CloneMode Mode{CloneMode::Bodyguard};
			bool Networked{};
			Clock::time_point DeadAt{};
		};

		constexpr int kCloneHealth = 800;
		constexpr float kFrenzySearchRadius = 260.0f;
		constexpr float kGuardRadius = 170.0f;
		constexpr float kTargetLeashRadius = 220.0f;
		constexpr float kHorseSearchRadius = 48.0f;
		constexpr float kSocialRadius = 11.0f;
		constexpr float kMourningRadius = 80.0f;
		constexpr std::size_t kMaxActiveClones = 8;
		constexpr float kBleedOutSeconds = 12.0f;
		constexpr int kBleedOutMilliseconds = 12000;
		constexpr int kIncapacitationThreshold = 20;
		constexpr int kMourningChancePercent = 22;
		constexpr auto kPedCacheInterval = 400ms;
		constexpr auto kBrainTick = 150ms;
		constexpr auto kCombatDecisionInterval = 550ms;
		constexpr auto kSocialDecisionInterval = 900ms;
		constexpr auto kHorseDecisionInterval = 2200ms;
		constexpr auto kFollowRefreshNear = 3000ms;
		constexpr auto kFollowRefreshFar = 1000ms;
		constexpr auto kFrenzyRoamRefresh = 4500ms;
		constexpr auto kIdleSmokeDelay = 10s;
		constexpr auto kMourningEmoteTime = 4500ms;
		constexpr auto kMourningFleeTime = 7000ms;
		constexpr auto kMourningCooldown = 45s;
		constexpr auto kCorpseKeepTime = 45s;
		constexpr auto kFlourishTime = 10s;
		constexpr auto kNormalEmoteTime = 3500ms;
		constexpr auto kDisarmReactionCooldown = 20s;

		inline constexpr ManualCloneEmotes::Definition kIdleSmokeEmote{"KIT_EMOTE_ACTION_SMOKE_CIGARETTE_1", 1};
		inline constexpr ManualCloneEmotes::Definition kDisarmCryEmote{"KIT_EMOTE_TAUNT_BOOHOO_1", 2};

		constexpr std::array kCloneWeapons = {
		    CloneWeapon{"Desarmado", "Unarmed", "WEAPON_UNARMED"},
		    CloneWeapon{"Faca", "Knife", "WEAPON_MELEE_KNIFE"},
		    CloneWeapon{"Facão", "Machete", "WEAPON_MELEE_MACHETE"},
		    CloneWeapon{"Facão de colecionador", "Collector Machete", "WEAPON_MELEE_MACHETE_COLLECTOR"},
		    CloneWeapon{"Machadinha", "Hatchet", "WEAPON_MELEE_HATCHET"},
		    CloneWeapon{"Cutelo", "Cleaver", "WEAPON_MELEE_CLEAVER"},
		    CloneWeapon{"Revólver Cattleman", "Cattleman Revolver", "WEAPON_REVOLVER_CATTLEMAN"},
		    CloneWeapon{"Revólver Double-Action", "Double-Action Revolver", "WEAPON_REVOLVER_DOUBLEACTION"},
		    CloneWeapon{"High Roller Double-Action", "High Roller Double-Action", "WEAPON_REVOLVER_DOUBLEACTION_GAMBLER"},
		    CloneWeapon{"Revólver Schofield", "Schofield Revolver", "WEAPON_REVOLVER_SCHOFIELD"},
		    CloneWeapon{"Revólver LeMat", "LeMat Revolver", "WEAPON_REVOLVER_LEMAT"},
		    CloneWeapon{"Revólver Navy", "Navy Revolver", "WEAPON_REVOLVER_NAVY"},
		    CloneWeapon{"Pistola Volcanic", "Volcanic Pistol", "WEAPON_PISTOL_VOLCANIC"},
		    CloneWeapon{"Pistola Mauser", "Mauser Pistol", "WEAPON_PISTOL_MAUSER"},
		    CloneWeapon{"Pistola Semi-Auto", "Semi-Auto Pistol", "WEAPON_PISTOL_SEMIAUTO"},
		    CloneWeapon{"Carabina", "Carbine Repeater", "WEAPON_REPEATER_CARBINE"},
		    CloneWeapon{"Evans Repeater", "Evans Repeater", "WEAPON_REPEATER_EVANS"},
		    CloneWeapon{"Litchfield Repeater", "Litchfield Repeater", "WEAPON_REPEATER_HENRY"},
		    CloneWeapon{"Lancaster Repeater", "Lancaster Repeater", "WEAPON_REPEATER_WINCHESTER"},
		    CloneWeapon{"Rifle Bolt Action", "Bolt Action Rifle", "WEAPON_RIFLE_BOLTACTION"},
		    CloneWeapon{"Rifle Springfield", "Springfield Rifle", "WEAPON_RIFLE_SPRINGFIELD"},
		    CloneWeapon{"Rifle Varmint", "Varmint Rifle", "WEAPON_RIFLE_VARMINT"},
		    CloneWeapon{"Rifle Elephant", "Elephant Rifle", "WEAPON_RIFLE_ELEPHANT"},
		    CloneWeapon{"Rifle Carcano", "Carcano Rifle", "WEAPON_SNIPERRIFLE_CARCANO"},
		    CloneWeapon{"Rolling Block", "Rolling Block Rifle", "WEAPON_SNIPERRIFLE_ROLLINGBLOCK"},
		    CloneWeapon{"Escopeta Pump", "Pump Shotgun", "WEAPON_SHOTGUN_PUMP"},
		    CloneWeapon{"Escopeta Repeating", "Repeating Shotgun", "WEAPON_SHOTGUN_REPEATING"},
		    CloneWeapon{"Escopeta Double Barrel", "Double Barrel Shotgun", "WEAPON_SHOTGUN_DOUBLEBARREL"},
		    CloneWeapon{"Escopeta Semi-Auto", "Semi-Auto Shotgun", "WEAPON_SHOTGUN_SEMIAUTO"},
		    CloneWeapon{"Escopeta serrada", "Sawed-Off Shotgun", "WEAPON_SHOTGUN_SAWEDOFF"},
		    CloneWeapon{"Arco", "Bow", "WEAPON_BOW"},
		    CloneWeapon{"Arco melhorado", "Improved Bow", "WEAPON_BOW_IMPROVED"},
		};

		std::vector<ManagedClone> g_ManagedClones;
		std::vector<CachedPed> g_PedCache;
		Clock::time_point g_NextPedCacheRefresh{};
		bool g_BodyguardsBetrayed{};
		std::uint32_t g_TotalSpawned{};
		bool g_SessionBrawlRunning{};
		std::uint32_t g_BrawlGeneration{};
		Clock::time_point g_NextMourningAllowed{};
		int g_MourningClone{};
		int g_MourningDeadClone{};
		Clock::time_point g_MourningEmoteUntil{};
		Clock::time_point g_MourningFleeUntil{};
		bool g_MourningFleeIssued{};

		std::mt19937& Rng() { static std::mt19937 rng{static_cast<std::mt19937::result_type>(Clock::now().time_since_epoch().count())}; return rng; }
		int RandomInt(int min, int max) { return std::uniform_int_distribution<int>(min, max)(Rng()); }
		float DistanceSquared(const Vector3& a, const Vector3& b)
		{
			const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
			return dx * dx + dy * dy + dz * dz;
		}

		ManagedClone* FindManagedClone(int ped)
		{
			auto it = std::find_if(g_ManagedClones.begin(), g_ManagedClones.end(), [ped](const ManagedClone& c) { return c.Ped == ped; });
			return it == g_ManagedClones.end() ? nullptr : &*it;
		}
		bool IsManagedClone(int ped) { return FindManagedClone(ped) != nullptr; }
		bool IsHostileManagedCloneForOwner(int ped)
		{
			auto* managed = FindManagedClone(ped);
			return managed && managed->Mode == CloneMode::AttackOwner && managed->Ped && ENTITY::DOES_ENTITY_EXIST(managed->Ped) && !ENTITY::IS_ENTITY_DEAD(managed->Ped);
		}
		void MarkCloneDead(int ped)
		{
			if (auto* managed = FindManagedClone(ped); managed && managed->DeadAt == Clock::time_point{}) managed->DeadAt = Clock::now();
		}
		void ResetMourning()
		{
			g_MourningClone = 0; g_MourningDeadClone = 0; g_MourningEmoteUntil = {}; g_MourningFleeUntil = {}; g_MourningFleeIssued = false;
		}
		std::size_t ActiveCloneCount()
		{
			return static_cast<std::size_t>(std::count_if(g_ManagedClones.begin(), g_ManagedClones.end(), [](const ManagedClone& c) {
				return c.Ped && ENTITY::DOES_ENTITY_EXIST(c.Ped) && !ENTITY::IS_ENTITY_DEAD(c.Ped);
			}));
		}

		void DeleteAllManagedClones()
		{
			++g_BrawlGeneration;
			g_SessionBrawlRunning = false;
			for (auto& managed : g_ManagedClones)
			{
				if (!managed.Ped || !ENTITY::DOES_ENTITY_EXIST(managed.Ped)) continue;
				if (managed.Networked)
				{
					for (int attempt = 0; attempt < 4 && !NETWORK::NETWORK_HAS_CONTROL_OF_ENTITY(managed.Ped); ++attempt)
					{
						NETWORK::NETWORK_REQUEST_CONTROL_OF_ENTITY(managed.Ped);
						ScriptMgr::Yield();
					}
				}
				int ped = managed.Ped;
				PED::DELETE_PED(&ped);
				managed.Ped = 0;
			}
			g_ManagedClones.clear(); g_PedCache.clear(); g_NextPedCacheRefresh = {}; g_BodyguardsBetrayed = false; g_TotalSpawned = 0; ResetMourning();
			Notifications::Show("Tenebris", Localization::IsPortuguese() ? "Todos os clones foram removidos." : "All clones were removed.", NotificationType::Success, 2200);
		}

		void PruneManagedClones(bool deleteDead)
		{
			const auto now = Clock::now();
			int deadDeleted{};
			g_ManagedClones.erase(std::remove_if(g_ManagedClones.begin(), g_ManagedClones.end(), [&](const ManagedClone& c) {
				if (!c.Ped || !ENTITY::DOES_ENTITY_EXIST(c.Ped)) return true;
				if (deleteDead && ENTITY::IS_ENTITY_DEAD(c.Ped) && c.DeadAt != Clock::time_point{} && now - c.DeadAt >= kCorpseKeepTime && deadDeleted < 4)
				{
					if (c.Ped == g_MourningDeadClone && now < g_MourningFleeUntil) return false;
					int ped = c.Ped; PED::DELETE_PED(&ped); ++deadDeleted; return true;
				}
				return false;
			}), g_ManagedClones.end());
			if (g_MourningClone && (!ENTITY::DOES_ENTITY_EXIST(g_MourningClone) || ENTITY::IS_ENTITY_DEAD(g_MourningClone) || now >= g_MourningFleeUntil)) ResetMourning();
			const bool hasLivingGuard = std::any_of(g_ManagedClones.begin(), g_ManagedClones.end(), [](const ManagedClone& c) {
				return c.Mode == CloneMode::Bodyguard && c.Ped && ENTITY::DOES_ENTITY_EXIST(c.Ped) && !ENTITY::IS_ENTITY_DEAD(c.Ped);
			});
			if (!hasLivingGuard) g_BodyguardsBetrayed = false;
		}

		const std::vector<CachedPed>& GetCachedPeds()
		{
			const auto now = Clock::now();
			if (now < g_NextPedCacheRefresh) return g_PedCache;
			g_PedCache.clear(); g_PedCache.reserve(96);
			for (Ped ped : Pools::GetPeds())
			{
				if (!ped.IsValid()) continue;
				const int handle = ped.GetHandle();
				if (!handle) continue;
				g_PedCache.push_back({handle, ENTITY::GET_ENTITY_COORDS(handle, true, false), PED::IS_PED_A_PLAYER(handle) != 0, ENTITY::IS_ENTITY_DEAD(handle) != 0});
			}
			g_NextPedCacheRefresh = now + kPedCacheInterval;
			return g_PedCache;
		}

		bool IsSidearm(std::string_view name) { return name.find("WEAPON_PISTOL_") != std::string_view::npos || name.find("WEAPON_REVOLVER_") != std::string_view::npos; }
		bool IsLongGun(std::string_view name) { return name.find("WEAPON_RIFLE_") != std::string_view::npos || name.find("WEAPON_REPEATER_") != std::string_view::npos || name.find("WEAPON_SNIPERRIFLE_") != std::string_view::npos; }
		int AccuracyForWeapon(std::string_view name) { if (IsSidearm(name)) return 60; if (IsLongGun(name)) return 82; return 70; }

		bool EnsureModelLoaded(Hash model)
		{
			if (!model || !STREAMING::IS_MODEL_IN_CDIMAGE(model)) return false;
			for (int i = 0; i < 80 && !STREAMING::HAS_MODEL_LOADED(model); ++i) { STREAMING::REQUEST_MODEL(model, false); ScriptMgr::Yield(10ms); }
			return STREAMING::HAS_MODEL_LOADED(model);
		}
		bool IsSourcePlayerStable(int sourcePlayerId, int expectedPed = 0)
		{
			if (sourcePlayerId < 0) sourcePlayerId = PLAYER::PLAYER_ID();
			if (sourcePlayerId != PLAYER::PLAYER_ID() && !NETWORK::NETWORK_IS_PLAYER_ACTIVE(sourcePlayerId)) return false;
			const int ped = PLAYER::GET_PLAYER_PED_SCRIPT_INDEX(sourcePlayerId);
			if (!ped || !ENTITY::DOES_ENTITY_EXIST(ped) || ENTITY::IS_ENTITY_DEAD(ped)) return false;
			return !expectedPed || expectedPed == ped;
		}
		int ResolveSourcePed(int sourcePlayerId)
		{
			if (sourcePlayerId < 0) sourcePlayerId = PLAYER::PLAYER_ID();
			return IsSourcePlayerStable(sourcePlayerId) ? PLAYER::GET_PLAYER_PED_SCRIPT_INDEX(sourcePlayerId) : 0;
		}

		int CreateCloneShell(int sourcePlayerId, int sourceHandle, const Vector3& spawn, float heading, bool networked)
		{
			if (!IsSourcePlayerStable(sourcePlayerId, sourceHandle)) return 0;
			const Hash model = ENTITY::GET_ENTITY_MODEL(sourceHandle);
			if (!EnsureModelLoaded(model) || !IsSourcePlayerStable(sourcePlayerId, sourceHandle)) return 0;
			LOG(INFO) << "[ManualClone] creating shell; sourcePlayer=" << sourcePlayerId << "; model=" << model << "; networked=" << (networked ? "true" : "false");
			int clone = PED::CREATE_PED(model, spawn.x, spawn.y, spawn.z, heading, networked, 0, 0, 0);
			STREAMING::SET_MODEL_AS_NO_LONGER_NEEDED(model);
			if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone) || PED::IS_PED_A_PLAYER(clone))
			{
				if (clone && ENTITY::DOES_ENTITY_EXIST(clone) && !PED::IS_PED_A_PLAYER(clone)) PED::DELETE_PED(&clone);
				return 0;
			}
			ENTITY::SET_ENTITY_AS_MISSION_ENTITY(clone, true, true);
			if (!IsSourcePlayerStable(sourcePlayerId, sourceHandle) || !ENTITY::DOES_ENTITY_EXIST(clone)) { if (ENTITY::DOES_ENTITY_EXIST(clone)) PED::DELETE_PED(&clone); return 0; }
			LOG(INFO) << "[ManualClone] copying appearance to shell; handle=" << clone;
			PED::CLONE_PED_TO_TARGET(sourceHandle, clone);
			ScriptMgr::Yield();
			if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone) || PED::IS_PED_A_PLAYER(clone))
			{
				if (clone && ENTITY::DOES_ENTITY_EXIST(clone) && !PED::IS_PED_A_PLAYER(clone)) PED::DELETE_PED(&clone);
				return 0;
			}
			PED::_UPDATE_PED_VARIATION(clone, 0, 1, 1, 1, 0);
			LOG(INFO) << "[ManualClone] appearance copied; handle=" << clone;
			return clone;
		}

		void ApplyPlayerPromptName(int clone, int sourcePlayerId)
		{
			if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone)) return;
			if (sourcePlayerId < 0) sourcePlayerId = PLAYER::PLAYER_ID();
			const char* playerName = PLAYER::GET_PLAYER_NAME(sourcePlayerId);
			if (playerName && *playerName) PED::_SET_PED_PROMPT_NAME(clone, MISC::VAR_STRING(10, "LITERAL_STRING", playerName));
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
		void ConfigureCloneLoot(int clone)
		{
			if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone)) return;
			PED::SET_LOOTING_FLAG(clone, 0, true); PED::SET_LOOTING_FLAG(clone, 1, true);
			WEAPON::SET_PED_DROPS_WEAPONS_WHEN_DEAD(clone, true); WEAPON::SET_PED_AMMO_TO_DROP(clone, 1, 1);
		}
		int GetPlayerMountHandle() { auto mount = Self::GetMount(); return mount.IsValid() ? mount.GetHandle() : 0; }
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

		int FindNearestNpcTarget(int clone, int owner, float radius)
		{
			const Vector3 origin = ENTITY::GET_ENTITY_COORDS(clone, true, false);
			const float radiusSq = radius * radius;
			const int ownerMount = GetPlayerMountHandle();
			int best{}; float bestDistanceSq = radiusSq;
			for (const auto& ped : GetCachedPeds())
			{
				if (!ped.Handle || ped.Handle == clone || ped.Handle == owner || ped.Handle == ownerMount || ped.Player || ped.Dead || IsManagedClone(ped.Handle)) continue;
				const float d = DistanceSquared(origin, ped.Position);
				if (d < bestDistanceSq) { bestDistanceSq = d; best = ped.Handle; }
			}
			return IsValidNpcTarget(best, clone, owner) ? best : 0;
		}
		int FindBodyguardThreat(int clone, int owner)
		{
			if (!owner || !ENTITY::DOES_ENTITY_EXIST(owner)) return 0;
			const Vector3 ownerPos = ENTITY::GET_ENTITY_COORDS(owner, true, false);
			int best{}; float bestDistanceSq = kGuardRadius * kGuardRadius;
			for (const auto& managed : g_ManagedClones)
			{
				if (!managed.Ped || managed.Ped == clone || managed.Mode != CloneMode::AttackOwner || !ENTITY::DOES_ENTITY_EXIST(managed.Ped) || ENTITY::IS_ENTITY_DEAD(managed.Ped)) continue;
				const float d = DistanceSquared(ownerPos, ENTITY::GET_ENTITY_COORDS(managed.Ped, true, false));
				if (d < bestDistanceSq) { bestDistanceSq = d; best = managed.Ped; }
			}
			for (const auto& ped : GetCachedPeds())
			{
				if (!ped.Handle || ped.Player || ped.Dead || !IsValidNpcTarget(ped.Handle, clone, owner)) continue;
				const float d = DistanceSquared(ownerPos, ped.Position);
				if (d >= bestDistanceSq) continue;
				if (!PED::IS_PED_IN_COMBAT(ped.Handle, owner) && !PED::IS_PED_IN_COMBAT(owner, ped.Handle) && !ENTITY::HAS_ENTITY_BEEN_DAMAGED_BY_ENTITY(owner, ped.Handle, true, true)) continue;
				bestDistanceSq = d; best = ped.Handle;
			}
			if (IsHostileManagedCloneForOwner(best)) return best;
			return IsValidNpcTarget(best, clone, owner) ? best : 0;
		}
		int FindNearestFreeHorse(int clone, int owner)
		{
			if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone)) return 0;
			const Vector3 origin = ENTITY::GET_ENTITY_COORDS(clone, true, false);
			const int ownerMount = GetPlayerMountHandle();
			int best{}; float bestDistanceSq = kHorseSearchRadius * kHorseSearchRadius;
			for (const auto& ped : GetCachedPeds())
			{
				if (!ped.Handle || ped.Handle == owner || ped.Handle == ownerMount || ped.Player || ped.Dead || IsManagedClone(ped.Handle)) continue;
				if (!PED::_IS_THIS_MODEL_A_HORSE(ENTITY::GET_ENTITY_MODEL(ped.Handle)) || !PED::_IS_MOUNT_SEAT_FREE(ped.Handle, -1)) continue;
				const float d = DistanceSquared(origin, ped.Position);
				if (d < bestDistanceSq) { bestDistanceSq = d; best = ped.Handle; }
			}
			return best;
		}

		bool IsLocalCameraLookingAt(int target, float maxDistance)
		{
			if (!target || !ENTITY::DOES_ENTITY_EXIST(target)) return false;
			const Vector3 cam = CAM::GET_GAMEPLAY_CAM_COORD(), rot = CAM::GET_GAMEPLAY_CAM_ROT(2), targetPos = ENTITY::GET_ENTITY_COORDS(target, true, false);
			const float dx = targetPos.x - cam.x, dy = targetPos.y - cam.y, dz = targetPos.z - cam.z;
			const float lenSq = dx * dx + dy * dy + dz * dz;
			if (lenSq > maxDistance * maxDistance || lenSq < 0.001f) return false;
			const float len = std::sqrt(lenSq), pitch = rot.x * 0.017453292519943295f, yaw = rot.z * 0.017453292519943295f, cp = std::cos(pitch);
			return (-std::sin(yaw) * cp) * (dx / len) + (std::cos(yaw) * cp) * (dy / len) + std::sin(pitch) * (dz / len) > 0.90f;
		}
		bool IsPedFacingClone(int sourcePed, int clone, float maxDistance)
		{
			if (!sourcePed || !clone || !ENTITY::DOES_ENTITY_EXIST(sourcePed) || !ENTITY::DOES_ENTITY_EXIST(clone)) return false;
			const Vector3 a = ENTITY::GET_ENTITY_COORDS(sourcePed, true, false), b = ENTITY::GET_ENTITY_COORDS(clone, true, false);
			const float dx = b.x - a.x, dy = b.y - a.y, lenSq = dx * dx + dy * dy;
			if (lenSq > maxDistance * maxDistance || lenSq < 0.001f) return false;
			const float len = std::sqrt(lenSq), heading = ENTITY::GET_ENTITY_HEADING(sourcePed) * 0.017453292519943295f;
			return -std::sin(heading) * (dx / len) + std::cos(heading) * (dy / len) > 0.84f;
		}
		int FindInteractingEmotePlayer(int clone)
		{
			const int localId = PLAYER::PLAYER_ID(), localPed = PLAYER::PLAYER_PED_ID();
			if (TASK::IS_EMOTE_TASK_RUNNING(localPed, 0) && IsLocalCameraLookingAt(clone, kSocialRadius)) return localPed;
			for (int id = 0; id < 32; ++id)
			{
				if (id == localId || !NETWORK::NETWORK_IS_PLAYER_ACTIVE(id)) continue;
				const int ped = PLAYER::GET_PLAYER_PED_SCRIPT_INDEX(id);
				if (ped && ENTITY::DOES_ENTITY_EXIST(ped) && !ENTITY::IS_ENTITY_DEAD(ped) && TASK::IS_EMOTE_TASK_RUNNING(ped, 0) && IsPedFacingClone(ped, clone, kSocialRadius)) return ped;
			}
			return 0;
		}
		void FaceEntity(int ped, int target)
		{
			if (!ped || !target || !ENTITY::DOES_ENTITY_EXIST(ped) || !ENTITY::DOES_ENTITY_EXIST(target)) return;
			const Vector3 from = ENTITY::GET_ENTITY_COORDS(ped, true, false), to = ENTITY::GET_ENTITY_COORDS(target, true, false);
			const float dx = to.x - from.x, dy = to.y - from.y;
			if (std::abs(dx) + std::abs(dy) < 0.001f) return;
			float heading = std::atan2(-dx, dy) * 57.29577951308232f;
			if (heading < 0.0f) heading += 360.0f;
			ENTITY::SET_ENTITY_HEADING(ped, heading);
		}
		bool GetCurrentWeapon(int ped, Hash& weapon)
		{
			weapon = Joaat("WEAPON_UNARMED");
			return ped && ENTITY::DOES_ENTITY_EXIST(ped) && WEAPON::GET_CURRENT_PED_WEAPON(ped, &weapon, true, 0, false);
		}
		void PlayFullBodyEmote(int clone, const ManualCloneEmotes::Definition& emote)
		{
			TASK::TASK_PLAY_EMOTE_WITH_HASH(clone, emote.Category, 2, Joaat(emote.Name), false, false, false, false, false);
		}
		void PlayTimedEmote(int clone, int faceTarget, const ManualCloneEmotes::Definition& emote, bool flourish, bool force, bool restoreWeapon = true)
		{
			if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone) || ENTITY::IS_ENTITY_DEAD(clone)) return;
			if (!force && PED::IS_PED_IN_COMBAT(clone, 0)) return;
			Hash previousWeapon{};
			const bool hadWeapon = GetCurrentWeapon(clone, previousWeapon) && previousWeapon != Joaat("WEAPON_UNARMED");
			if (hadWeapon) WEAPON::SET_CURRENT_PED_WEAPON(clone, Joaat("WEAPON_UNARMED"), true, 0, false, false);
			TASK::CLEAR_PED_TASKS(clone, true, false); FaceEntity(clone, faceTarget); PlayFullBodyEmote(clone, emote);
			const auto endAt = Clock::now() + (flourish ? kFlourishTime : kNormalEmoteTime);
			auto nextFace = Clock::now(), nextReplay = Clock::now() + 3200ms;
			while (Clock::now() < endAt && ENTITY::DOES_ENTITY_EXIST(clone) && !ENTITY::IS_ENTITY_DEAD(clone) && !PED::IS_PED_INCAPACITATED(clone))
			{
				const auto now = Clock::now();
				if (faceTarget && ENTITY::DOES_ENTITY_EXIST(faceTarget) && now >= nextFace) { FaceEntity(clone, faceTarget); nextFace = now + 500ms; }
				if (flourish && now >= nextReplay) { PlayFullBodyEmote(clone, emote); nextReplay = now + 3200ms; }
				ScriptMgr::Yield(kBrainTick);
			}
			if (restoreWeapon && hadWeapon && ENTITY::DOES_ENTITY_EXIST(clone) && !ENTITY::IS_ENTITY_DEAD(clone))
			{
				Hash current{};
				if (GetCurrentWeapon(clone, current) && current == Joaat("WEAPON_UNARMED")) WEAPON::SET_CURRENT_PED_WEAPON(clone, previousWeapon, true, 0, false, false);
			}
		}
		void PlayRareSocialReaction(int clone, int faceTarget, bool flourish, bool insult = false)
		{
			if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone) || PED::IS_PED_IN_COMBAT(clone, 0)) return;
			if (insult)
			{
				const auto& emotes = ManualCloneEmotes::kInsultEmotes;
				PlayTimedEmote(clone, faceTarget, emotes[RandomInt(0, static_cast<int>(emotes.size()) - 1)], flourish, false);
			}
			else
			{
				const auto& emotes = ManualCloneEmotes::kAllHumanPlayerEmotes;
				PlayTimedEmote(clone, faceTarget, emotes[RandomInt(0, static_cast<int>(emotes.size()) - 1)], flourish, false);
			}
		}

		void MaybeStartMourning(int deadClone, CloneMode deadMode)
		{
			if (!deadClone || !ENTITY::DOES_ENTITY_EXIST(deadClone)) return;
			const auto now = Clock::now();
			if ((g_MourningClone && now < g_MourningFleeUntil) || now < g_NextMourningAllowed || RandomInt(1, 100) > kMourningChancePercent) return;
			const Vector3 deadPos = ENTITY::GET_ENTITY_COORDS(deadClone, true, false);
			int mourner{}; float bestDistanceSq = kMourningRadius * kMourningRadius;
			for (const auto& managed : g_ManagedClones)
			{
				if (!managed.Ped || managed.Ped == deadClone || managed.Mode != deadMode || !ENTITY::DOES_ENTITY_EXIST(managed.Ped) || ENTITY::IS_ENTITY_DEAD(managed.Ped) || PED::IS_PED_INCAPACITATED(managed.Ped)) continue;
				const float d = DistanceSquared(deadPos, ENTITY::GET_ENTITY_COORDS(managed.Ped, true, false));
				if (d < bestDistanceSq) { bestDistanceSq = d; mourner = managed.Ped; }
			}
			if (!mourner) return;
			g_MourningClone = mourner; g_MourningDeadClone = deadClone; g_MourningEmoteUntil = now + kMourningEmoteTime; g_MourningFleeUntil = g_MourningEmoteUntil + kMourningFleeTime; g_MourningFleeIssued = false; g_NextMourningAllowed = now + kMourningCooldown;
			TASK::CLEAR_PED_TASKS(mourner, true, false); FaceEntity(mourner, deadClone); PlayFullBodyEmote(mourner, ManualCloneEmotes::kMourningEmote);
		}

		void ConfigureCloneCombat(int clone, CloneMode mode, std::string_view weaponName)
		{
			PED::SET_PED_KEEP_TASK(clone, true); PED::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(clone, mode == CloneMode::Bodyguard);
			PED::SET_PED_COMBAT_ABILITY(clone, 2); PED::SET_PED_ACCURACY(clone, AccuracyForWeapon(weaponName)); PED::SET_PED_SEEING_RANGE(clone, 220.0f); PED::SET_PED_HEARING_RANGE(clone, 180.0f);
			for (int attr : {0, 5, 21, 23, 24, 46, 50, 58}) PED::SET_PED_COMBAT_ATTRIBUTES(clone, attr, true);
			PED::SET_PED_COMBAT_ATTRIBUTES(clone, 27, false);
			if (mode == CloneMode::Bodyguard) PED::SET_PED_AS_GROUP_MEMBER(clone, PLAYER::GET_PLAYER_GROUP(PLAYER::PLAYER_ID()));
		}
		Hash EquipCloneWeapon(int clone, int weaponIndex)
		{
			weaponIndex = std::clamp(weaponIndex, 0, static_cast<int>(kCloneWeapons.size()) - 1);
			WEAPON::REMOVE_ALL_PED_WEAPONS(clone, true, true);
			const Hash weapon = Joaat(kCloneWeapons[weaponIndex].HashName);
			if (weapon != Joaat("WEAPON_UNARMED")) { WEAPON::GIVE_WEAPON_TO_PED(clone, weapon, 999, true, true, 0, false, 0.5f, 1.0f, 1.0f, false, 0, false); WEAPON::SET_CURRENT_PED_WEAPON(clone, weapon, true, 0, false, false); }
			return weapon;
		}
		void TaskVanillaCombat(int clone, int target, int owner)
		{
			const bool validTarget = target == owner || IsHostileManagedCloneForOwner(target) || IsValidNpcTarget(target, clone, owner);
			if (!validTarget) return;
			PED::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(clone, false); PED::SET_PED_MOVE_RATE_OVERRIDE(clone, 1.0f); TASK::TASK_COMBAT_PED(clone, target, 0, 16);
		}
		bool TryMountForFollow(int clone, int owner)
		{
			if (!clone || !owner || !ENTITY::DOES_ENTITY_EXIST(clone) || !ENTITY::DOES_ENTITY_EXIST(owner)) return false;
			if (PED::IS_PED_ON_MOUNT(clone)) return true;
			const int ownerMount = GetPlayerMountHandle();
			if (ownerMount && ENTITY::DOES_ENTITY_EXIST(ownerMount) && PED::_IS_MOUNT_SEAT_FREE(ownerMount, 0)) { TASK::TASK_MOUNT_ANIMAL(clone, ownerMount, 5000, 0, 2.0f, 1, 0, 0); return true; }
			const float dSq = DistanceSquared(ENTITY::GET_ENTITY_COORDS(clone, true, false), ENTITY::GET_ENTITY_COORDS(owner, true, false));
			if (!ownerMount && dSq < 45.0f * 45.0f) return false;
			const int horse = FindNearestFreeHorse(clone, owner);
			if (horse) { TASK::TASK_MOUNT_ANIMAL(clone, horse, 5000, -1, 2.0f, 1, 0, 0); return true; }
			return false;
		}
		void MaintainBodyguardFollow(int clone, int owner, Clock::time_point now, Clock::time_point& nextHorse, Clock::time_point& nextFollow)
		{
			if (!clone || !owner || !ENTITY::DOES_ENTITY_EXIST(clone) || !ENTITY::DOES_ENTITY_EXIST(owner)) return;
			PED::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(clone, true);
			const float dSq = DistanceSquared(ENTITY::GET_ENTITY_COORDS(clone, true, false), ENTITY::GET_ENTITY_COORDS(owner, true, false));
			const bool isFar = dSq > 70.0f * 70.0f, isVeryFar = dSq > 180.0f * 180.0f;
			if (now >= nextHorse && (GetPlayerMountHandle() || isFar)) { TryMountForFollow(clone, owner); nextHorse = now + kHorseDecisionInterval; }
			if (now < nextFollow || dSq <= 7.0f * 7.0f) return;
			PED::SET_PED_MOVE_RATE_OVERRIDE(clone, isVeryFar ? 1.65f : (isFar ? 1.45f : 1.1f));
			TASK::TASK_FOLLOW_TO_OFFSET_OF_ENTITY(clone, owner, 1.8f, -2.8f, 0.0f, isVeryFar ? 5.0f : (isFar ? 4.0f : 2.4f), -1, 3.0f, true, true, false, true, true, true);
			nextFollow = now + (isFar ? kFollowRefreshFar : kFollowRefreshNear);
		}

#include "ManualCloneBrawl.inc"

		void RunCloneBrain(int clone, int owner, CloneOptions options, Hash equippedWeapon)
		{
			int currentTarget{}, issuedTarget{}, lastHealth = ENTITY::GET_ENTITY_HEALTH(clone);
			bool sawIncapacitation{}, insultedOnBetrayal{}, smoking{}, frenzyRoaming{}, armedSeen = equippedWeapon != Joaat("WEAPON_UNARMED");
			auto nextDecision = Clock::now(), nextSocial = Clock::now() + 3s, nextIdleEmote = Clock::now() + 18s, nextHorse = Clock::now(), nextFollow = Clock::now(), nextRoam = Clock::now(), nextDisarmReaction = Clock::time_point{}, idleSince = Clock::now();
			while (clone && ENTITY::DOES_ENTITY_EXIST(clone))
			{
				if (!owner || !ENTITY::DOES_ENTITY_EXIST(owner)) break;
				const auto now = Clock::now();
				const int health = ENTITY::GET_ENTITY_HEALTH(clone);
				const bool damagedByOwner = ENTITY::HAS_ENTITY_BEEN_DAMAGED_BY_ENTITY(clone, owner, true, true);
				if (options.Mode == CloneMode::Bodyguard && !g_BodyguardsBetrayed && health < lastHealth && damagedByOwner) g_BodyguardsBetrayed = true;
				lastHealth = health;

				Hash currentWeapon{};
				if (GetCurrentWeapon(clone, currentWeapon))
				{
					if (currentWeapon != Joaat("WEAPON_UNARMED")) armedSeen = true;
					else if (armedSeen && equippedWeapon != Joaat("WEAPON_UNARMED") && damagedByOwner && PED::IS_PED_IN_COMBAT(clone, 0) && now >= nextDisarmReaction)
					{
						armedSeen = false; currentTarget = 0; issuedTarget = 0;
						PlayTimedEmote(clone, owner, kDisarmCryEmote, options.EmoteFlourish, true, false);
						nextDisarmReaction = Clock::now() + kDisarmReactionCooldown; nextDecision = Clock::now();
					}
				}
				if (damagedByOwner) ENTITY::CLEAR_ENTITY_LAST_DAMAGE_ENTITY(clone);

				if (PED::IS_PED_INCAPACITATED(clone))
				{
					if (!sawIncapacitation) { sawIncapacitation = true; currentTarget = 0; issuedTarget = 0; PED::SET_PAUSE_PED_WRITHE_BLEEDOUT(clone, false); }
					ScriptMgr::Yield(kBrainTick); continue;
				}
				if (ENTITY::IS_ENTITY_DEAD(clone)) { MarkCloneDead(clone); if (clone == g_MourningClone) ResetMourning(); MaybeStartMourning(clone, options.Mode); break; }

				if (clone == g_MourningClone)
				{
					if (now < g_MourningEmoteUntil) { ScriptMgr::Yield(kBrainTick); continue; }
					if (now < g_MourningFleeUntil)
					{
						if (!g_MourningFleeIssued) { TASK::CLEAR_PED_TASKS(clone, true, false); if (g_MourningDeadClone && ENTITY::DOES_ENTITY_EXIST(g_MourningDeadClone)) TASK::TASK_SMART_FLEE_PED(clone, g_MourningDeadClone, 55.0f, 7000, 0, 3.0f, 0); else TASK::TASK_WANDER_STANDARD(clone, 1.0f, 0); g_MourningFleeIssued = true; }
						ScriptMgr::Yield(kBrainTick); continue;
					}
					ResetMourning(); currentTarget = 0; issuedTarget = 0; nextDecision = now;
				}

				CloneMode effectiveMode = options.Mode;
				if (options.Mode == CloneMode::Bodyguard && g_BodyguardsBetrayed)
				{
					effectiveMode = CloneMode::AttackOwner; PED::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(clone, false);
					if (!insultedOnBetrayal && !PED::IS_PED_IN_COMBAT(clone, owner)) { PlayRareSocialReaction(clone, owner, options.EmoteFlourish, true); insultedOnBetrayal = true; nextDecision = Clock::now(); }
				}

				if (now >= nextDecision)
				{
					const bool targetWasPresent = currentTarget != 0;
					bool keepCurrent = currentTarget && ENTITY::DOES_ENTITY_EXIST(currentTarget) && !ENTITY::IS_ENTITY_DEAD(currentTarget) && IsWithinRange(clone, currentTarget, kTargetLeashRadius);
					if (effectiveMode == CloneMode::Bodyguard && keepCurrent && !IsHostileManagedCloneForOwner(currentTarget)) keepCurrent = IsValidNpcTarget(currentTarget, clone, owner) && (PED::IS_PED_IN_COMBAT(currentTarget, owner) || PED::IS_PED_IN_COMBAT(owner, currentTarget) || ENTITY::HAS_ENTITY_BEEN_DAMAGED_BY_ENTITY(owner, currentTarget, true, true));
					else if (effectiveMode == CloneMode::FrenzyNpcs && keepCurrent) keepCurrent = IsValidNpcTarget(currentTarget, clone, owner);
					else if (effectiveMode == CloneMode::AttackOwner && keepCurrent) keepCurrent = currentTarget == owner;
					if (!keepCurrent)
					{
						if (targetWasPresent) TASK::CLEAR_PED_TASKS(clone, true, false);
						currentTarget = effectiveMode == CloneMode::AttackOwner ? owner : (effectiveMode == CloneMode::Bodyguard ? FindBodyguardThreat(clone, owner) : FindNearestNpcTarget(clone, owner, kFrenzySearchRadius)); issuedTarget = 0;
					}
					if (currentTarget && ENTITY::DOES_ENTITY_EXIST(currentTarget) && !ENTITY::IS_ENTITY_DEAD(currentTarget))
					{
						smoking = false; frenzyRoaming = false; idleSince = now;
						if (issuedTarget != currentTarget || !PED::IS_PED_IN_COMBAT(clone, currentTarget)) { TaskVanillaCombat(clone, currentTarget, owner); issuedTarget = currentTarget; }
					}
					else if (effectiveMode == CloneMode::Bodyguard)
					{
						issuedTarget = 0; MaintainBodyguardFollow(clone, owner, now, nextHorse, nextFollow);
						const float dSq = DistanceSquared(ENTITY::GET_ENTITY_COORDS(clone, true, false), ENTITY::GET_ENTITY_COORDS(owner, true, false));
						if (dSq <= 9.0f * 9.0f && ENTITY::GET_ENTITY_SPEED(clone) < 0.25f && ENTITY::GET_ENTITY_SPEED(owner) < 0.25f)
						{
							const Vector3 pos = ENTITY::GET_ENTITY_COORDS(clone, true, false);
							if (now - idleSince >= kIdleSmokeDelay && !smoking && !PATHFIND::IS_POINT_ON_ROAD(pos.x, pos.y, pos.z, 0)) { PlayTimedEmote(clone, owner, kIdleSmokeEmote, false, false); smoking = true; nextFollow = Clock::now(); }
						}
						else { idleSince = now; smoking = false; }
					}
					else if (effectiveMode == CloneMode::FrenzyNpcs)
					{
						issuedTarget = 0; smoking = false; PED::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(clone, false); PED::SET_PED_MOVE_RATE_OVERRIDE(clone, 1.35f);
						if (!frenzyRoaming || now >= nextRoam) { TASK::CLEAR_PED_TASKS(clone, true, false); TASK::TASK_WANDER_STANDARD(clone, 1.0f, 0); frenzyRoaming = true; nextRoam = now + kFrenzyRoamRefresh; }
					}
					else { issuedTarget = 0; currentTarget = owner; }
					nextDecision = Clock::now() + kCombatDecisionInterval;
				}

				if (Clock::now() >= nextSocial && !currentTarget && !PED::IS_PED_IN_COMBAT(clone, 0))
				{
					const int interactingPlayer = FindInteractingEmotePlayer(clone);
					if (interactingPlayer) { PlayRareSocialReaction(clone, interactingPlayer, options.EmoteFlourish, false); nextDecision = Clock::now(); }
					nextSocial = Clock::now() + kSocialDecisionInterval;
				}
				if (Clock::now() >= nextIdleEmote && !currentTarget && !smoking && !PED::IS_PED_IN_COMBAT(clone, 0))
				{
					if (RandomInt(1, 100) <= 4 && IsWithinRange(clone, owner, kSocialRadius)) { PlayRareSocialReaction(clone, owner, options.EmoteFlourish, false); nextDecision = Clock::now(); }
					nextIdleEmote = Clock::now() + 24s;
				}
				ScriptMgr::Yield(kBrainTick);
			}
		}

		void SpawnManualCloneAt(CloneOptions options, const Vector3& spawn, float heading)
		{
			PruneManagedClones(false);
			if (ActiveCloneCount() >= kMaxActiveClones) { PruneManagedClones(true); if (ActiveCloneCount() >= kMaxActiveClones) { Notifications::Show("Tenebris", Localization::IsPortuguese() ? "Já existem 8 clones vivos. Apague ou mate algum antes de criar outro." : "There are already 8 living clones. Delete or kill one before creating another.", NotificationType::Warning, 2600); return; } }
			auto self = Self::GetPed(); if (!self.IsValid() || self.GetHealth() <= 0) return;
			const int owner = self.GetHandle(); if (options.SourcePlayerId < 0) options.SourcePlayerId = PLAYER::PLAYER_ID();
			const int sourcePed = ResolveSourcePed(options.SourcePlayerId);
			if (!sourcePed) { Notifications::Show("Tenebris", Localization::IsPortuguese() ? "O jogador escolhido não está mais disponível na sessão." : "The selected player is no longer available in the session.", NotificationType::Warning, 2600); return; }
			int clone = CreateCloneShell(options.SourcePlayerId, sourcePed, spawn, heading, options.Networked);
			if (!clone) { Notifications::Show("Tenebris", Localization::IsPortuguese() ? "Não foi possível criar o clone com segurança." : "Could not create the clone safely.", NotificationType::Warning, 2600); return; }
			ENTITY::PLACE_ENTITY_ON_GROUND_PROPERLY(clone, true); ENTITY::SET_ENTITY_MAX_HEALTH(clone, kCloneHealth); ENTITY::SET_ENTITY_HEALTH(clone, kCloneHealth, 0);
			ApplyPlayerPromptName(clone, options.SourcePlayerId); ConfigureBleedout(clone); ConfigureCloneLoot(clone);
			options.WeaponIndex = std::clamp(options.WeaponIndex, 0, static_cast<int>(kCloneWeapons.size()) - 1);
			const Hash weapon = EquipCloneWeapon(clone, options.WeaponIndex);
			ConfigureCloneCombat(clone, options.Mode, kCloneWeapons[options.WeaponIndex].HashName); ENTITY::CLEAR_ENTITY_LAST_DAMAGE_ENTITY(clone);
			g_ManagedClones.push_back({clone, options.SourcePlayerId, options.Mode, options.Networked, {}}); g_NextPedCacheRefresh = {};
			++g_TotalSpawned; if (g_TotalSpawned % 5 == 0) PruneManagedClones(true);
			FiberPool::Push([clone, owner, options, weapon] { RunCloneBrain(clone, owner, options, weapon); });
			const char* sourceName = PLAYER::GET_PLAYER_NAME(options.SourcePlayerId);
			std::string msg = Localization::IsPortuguese() ? (options.Networked ? "Clone de rede criado" : "Clone local criado") : (options.Networked ? "Network clone created" : "Local clone created");
			if (sourceName && *sourceName) msg += std::string(" - ") + sourceName;
			Notifications::Show("Tenebris", msg, NotificationType::Success, 1800);
		}
		void SpawnManualCloneNearPlayer(CloneOptions options)
		{
			auto self = Self::GetPed(); if (!self.IsValid()) return;
			const int handle = self.GetHandle();
			SpawnManualCloneAt(options, ENTITY::GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS(handle, 0.0f, 3.0f, 0.25f), ENTITY::GET_ENTITY_HEADING(handle));
		}
		bool GetFreecamAimPoint(const Vector3& position, const Vector3& rotation, int ignoreEntity, Vector3& out)
		{
			constexpr float kDegToRad = 0.017453292519943295f;
			const float pitch = rotation.x * kDegToRad, yaw = rotation.z * kDegToRad, cp = std::cos(pitch);
			const Vector3 direction{-std::sin(yaw) * cp, std::cos(yaw) * cp, std::sin(pitch)};
			const Vector3 farPoint{position.x + direction.x * 1000.0f, position.y + direction.y * 1000.0f, position.z + direction.z * 1000.0f};
			BOOL hit{}; Vector3 endCoords{}, normal{}; int hitEntity{};
			const int ray = SHAPETEST::START_EXPENSIVE_SYNCHRONOUS_SHAPE_TEST_LOS_PROBE(position.x, position.y, position.z, farPoint.x, farPoint.y, farPoint.z, 511, ignoreEntity, 7);
			SHAPETEST::GET_SHAPE_TEST_RESULT(ray, &hit, &endCoords, &normal, &hitEntity);
			if (!hit) return false; out = endCoords; return true;
		}
		void RunCloneSpawnFreecam(CloneOptions options)
		{
			auto self = Self::GetPed(); if (!self.IsValid()) return;
			const int selfHandle = self.GetHandle(); const bool reopenMenu = GUI::IsOpen(); if (reopenMenu) GUI::Toggle();
			int camera = CAM::CREATE_CAM("DEFAULT_SCRIPTED_CAMERA", 0);
			if (!camera) { if (reopenMenu && !GUI::IsOpen()) GUI::Toggle(); return; }
			Vector3 position = CAM::GET_GAMEPLAY_CAM_COORD(), rotation = CAM::GET_GAMEPLAY_CAM_ROT(2);
			CAM::SET_CAM_COORD(camera, position.x, position.y, position.z); CAM::SET_CAM_ROT(camera, rotation.x, rotation.y, rotation.z, 2); CAM::SET_CAM_ACTIVE(camera, true); CAM::RENDER_SCRIPT_CAMS(true, true, 350, true, true, 0);
			self.SetFrozen(true); self.SetVisible(false);
			Notifications::Show("Tenebris", Localization::IsPortuguese() ? "FREECAM: ENTER cria e continua aqui. Crie quantos quiser; BACK sai." : "FREECAM: ENTER spawns and stays here. Create as many as you want; BACK exits.", NotificationType::Info, 6500);
			float acceleration{};
			while (camera && CAM::DOES_CAM_EXIST(camera))
			{
				PAD::DISABLE_ALL_CONTROL_ACTIONS(0);
				for (Hash control : {(Hash)NativeInputs::INPUT_LOOK_LR, (Hash)NativeInputs::INPUT_LOOK_UD, (Hash)NativeInputs::INPUT_LOOK_UP_ONLY, (Hash)NativeInputs::INPUT_LOOK_DOWN_ONLY, (Hash)NativeInputs::INPUT_LOOK_LEFT_ONLY, (Hash)NativeInputs::INPUT_LOOK_RIGHT_ONLY}) PAD::ENABLE_CONTROL_ACTION(0, control, true);
				Vector3 delta{}; constexpr float speed = 0.12f;
				if (PAD::IS_DISABLED_CONTROL_PRESSED(0, (Hash)NativeInputs::INPUT_MOVE_UP_ONLY)) delta.y += speed; if (PAD::IS_DISABLED_CONTROL_PRESSED(0, (Hash)NativeInputs::INPUT_MOVE_DOWN_ONLY)) delta.y -= speed; if (PAD::IS_DISABLED_CONTROL_PRESSED(0, (Hash)NativeInputs::INPUT_MOVE_LEFT_ONLY)) delta.x -= speed; if (PAD::IS_DISABLED_CONTROL_PRESSED(0, (Hash)NativeInputs::INPUT_MOVE_RIGHT_ONLY)) delta.x += speed; if (PAD::IS_DISABLED_CONTROL_PRESSED(0, (Hash)NativeInputs::INPUT_SPRINT)) delta.z += speed * 0.75f; if (PAD::IS_DISABLED_CONTROL_PRESSED(0, (Hash)NativeInputs::INPUT_DUCK) || PAD::IS_DISABLED_CONTROL_PRESSED(0, (Hash)NativeInputs::INPUT_HORSE_STOP)) delta.z -= speed * 0.75f;
				acceleration = (delta.x == 0.0f && delta.y == 0.0f && delta.z == 0.0f) ? 0.0f : std::min(8.0f, acceleration + 0.12f);
				rotation = CAM::GET_GAMEPLAY_CAM_ROT(2); const float yaw = rotation.z * 0.017453292519943295f;
				position.x += (delta.x * std::cos(yaw) - delta.y * std::sin(yaw)) * acceleration; position.y += (delta.x * std::sin(yaw) + delta.y * std::cos(yaw)) * acceleration; position.z += delta.z * acceleration;
				CAM::SET_CAM_COORD(camera, position.x, position.y, position.z); CAM::SET_CAM_ROT(camera, rotation.x, rotation.y, rotation.z, 2); STREAMING::SET_FOCUS_POS_AND_VEL(position.x, position.y, position.z, 0.0f, 0.0f, 0.0f);
				Vector3 marker{}; const bool markerValid = GetFreecamAimPoint(position, rotation, selfHandle, marker);
				if (markerValid) GRAPHICS::_DRAW_MARKER(0x6903B113, marker.x, marker.y, marker.z + 0.04f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 1.65f, 1.65f, 1.65f, 255, 255, 255, 245, false, true, 2, false, nullptr, nullptr, false);
				if (PAD::IS_DISABLED_CONTROL_JUST_PRESSED(0, (Hash)NativeInputs::INPUT_FRONTEND_ACCEPT) && markerValid) { SpawnManualCloneAt(options, marker, rotation.z); ScriptMgr::Yield(250ms); }
				if (PAD::IS_DISABLED_CONTROL_JUST_PRESSED(0, (Hash)NativeInputs::INPUT_FRONTEND_CANCEL)) break;
				ScriptMgr::Yield();
			}
			CAM::SET_CAM_ACTIVE(camera, false); CAM::RENDER_SCRIPT_CAMS(false, true, 350, true, true, 0); CAM::DESTROY_CAM(camera, false); STREAMING::CLEAR_FOCUS(); self.SetFrozen(false); self.SetVisible(true); if (reopenMenu && !GUI::IsOpen()) GUI::Toggle();
		}

		const char* ModeLabelPt(CloneMode mode) { switch (mode) { case CloneMode::Bodyguard: return "Guarda-costas"; case CloneMode::FrenzyNpcs: return "Frenético contra NPCs"; case CloneMode::AttackOwner: return "Atacar somente eu"; case CloneMode::BrawlExpedition: return "Batalha da sessão"; } return "Guarda-costas"; }
		const char* ModeLabelEn(CloneMode mode) { switch (mode) { case CloneMode::Bodyguard: return "Bodyguard"; case CloneMode::FrenzyNpcs: return "Frenzy against NPCs"; case CloneMode::AttackOwner: return "Attack only me"; case CloneMode::BrawlExpedition: return "Session brawl"; } return "Bodyguard"; }

		class ManualCloneItem final : public UIItem
		{
			int m_WeaponIndex{}; CloneMode m_Mode{CloneMode::Bodyguard}; int m_SourcePlayerId{-1}; bool m_Networked{}; bool m_EmoteFlourish{true};
			CloneOptions GetOptions() const { CloneOptions out{m_WeaponIndex, m_Mode, m_SourcePlayerId, m_Networked, m_EmoteFlourish}; if (out.SourcePlayerId < 0) out.SourcePlayerId = PLAYER::PLAYER_ID(); return out; }
			std::string SourcePreview() const
			{
				const int id = m_SourcePlayerId < 0 ? PLAYER::PLAYER_ID() : m_SourcePlayerId; const char* name = PLAYER::GET_PLAYER_NAME(id);
				if (!name || !*name) return Localization::IsPortuguese() ? "Eu" : "Me";
				if (id == PLAYER::PLAYER_ID()) return std::string(Localization::IsPortuguese() ? "Eu - " : "Me - ") + name;
				return name;
			}
		public:
			void Draw() override
			{
				ImGui::TextUnformatted(Localization::IsPortuguese() ? "Criar clone inteligente" : "Create smart clone"); ImGui::Spacing();
				ImGui::TextUnformatted(Localization::IsPortuguese() ? "Aparência / jogador clonado" : "Appearance / cloned player"); const std::string sourcePreview = SourcePreview(); ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::BeginCombo("##CloneSourcePlayer", sourcePreview.c_str()))
				{
					const int localId = PLAYER::PLAYER_ID(); const char* localName = PLAYER::GET_PLAYER_NAME(localId); std::string localLabel = std::string(Localization::IsPortuguese() ? "Eu" : "Me") + (localName && *localName ? std::string(" - ") + localName : "");
					if (ImGui::Selectable(localLabel.c_str(), m_SourcePlayerId < 0 || m_SourcePlayerId == localId)) m_SourcePlayerId = localId;
					for (int id = 0; id < 32; ++id) { if (id == localId || !NETWORK::NETWORK_IS_PLAYER_ACTIVE(id)) continue; const int ped = PLAYER::GET_PLAYER_PED_SCRIPT_INDEX(id); const char* name = PLAYER::GET_PLAYER_NAME(id); if (ped && ENTITY::DOES_ENTITY_EXIST(ped) && name && *name && ImGui::Selectable(name, m_SourcePlayerId == id)) m_SourcePlayerId = id; }
					ImGui::EndCombo();
				}
				ImGui::TextUnformatted(Localization::IsPortuguese() ? "Arma (Red Dead Online)" : "Weapon (Red Dead Online)"); const char* weaponPreview = Localization::IsPortuguese() ? kCloneWeapons[m_WeaponIndex].LabelPt : kCloneWeapons[m_WeaponIndex].LabelEn; ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::BeginCombo("##ManualCloneWeapon", weaponPreview)) { for (int i = 0; i < static_cast<int>(kCloneWeapons.size()); ++i) { const char* label = Localization::IsPortuguese() ? kCloneWeapons[i].LabelPt : kCloneWeapons[i].LabelEn; if (ImGui::Selectable(label, m_WeaponIndex == i)) m_WeaponIndex = i; } ImGui::EndCombo(); }
				ImGui::Spacing(); ImGui::SeparatorText(Localization::IsPortuguese() ? "COMPORTAMENTO" : "BEHAVIOR"); const char* modePreview = Localization::IsPortuguese() ? ModeLabelPt(m_Mode) : ModeLabelEn(m_Mode); ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::BeginCombo("##CloneBehavior", modePreview)) { for (int i = 0; i < 3; ++i) { const auto mode = static_cast<CloneMode>(i); const char* label = Localization::IsPortuguese() ? ModeLabelPt(mode) : ModeLabelEn(mode); if (ImGui::Selectable(label, m_Mode == mode)) m_Mode = mode; } ImGui::EndCombo(); }
				ImGui::Checkbox(Localization::IsPortuguese() ? "Floreio de emote (10 s)" : "Emote flourish (10 s)", &m_EmoteFlourish);
				ImGui::TextWrapped("%s", Localization::IsPortuguese() ? "Guarda-costas entra no grupo do jogador e recebe seguimento persistente; combate/cobertura ficam com a IA padrão do RDR2. Frenético abandona cadáveres, procura outro NPC e continua vagando entre buscas." : "Bodyguards join the player group and keep persistent following; combat/cover use stock RDR2 AI. Frenzy abandons corpses, searches another NPC and keeps roaming between scans.");
				ImGui::TextDisabled("%s", Localization::IsPortuguese() ? "Emotes são full-body: armas são guardadas antes da animação e voltam depois. Arma realmente desarmada não é recriada." : "Emotes are full-body: weapons are stowed before the animation and return after it. A truly disarmed weapon is not recreated.");
				ImGui::Spacing(); ImGui::SeparatorText(Localization::IsPortuguese() ? "REDE" : "NETWORK"); ImGui::Checkbox(Localization::IsPortuguese() ? "Ativar Network (visível para outros)" : "Enable Network (visible to others)", &m_Networked);
				ImGui::TextDisabled("%s", Localization::IsPortuguese() ? "Não: clone só no seu cliente. Sim: entidade de rede, outros podem receber/ver; pode haver mais desync/ownership." : "Off: clone exists only on your client. On: network entity others may receive/see; more desync/ownership is possible.");

				ImGui::Spacing(); ImGui::SeparatorText(Localization::IsPortuguese() ? "EVENTO DA SESSÃO" : "SESSION EVENT");
				if (ImGui::Button(Localization::IsPortuguese() ? "CLONAR TODOS: BATALHA + EXPEDIÇÃO" : "CLONE ALL: BRAWL + EXPEDITION", ImVec2(-1.0f, 36.0f)))
				{
					const CloneOptions options = GetOptions();
					FiberPool::Push([options] { RunSessionCloneBrawl(options); });
				}
				ImGui::TextWrapped("%s", Localization::IsPortuguese() ? "Cria 1 clone por jogador ativo em duas fileiras, com spawn escalonado para reduzir travadas. Após 10 s, os dois grupos brigam usando a IA nativa. Sobreviventes pegam cavalo se houver, seguem para Armadillo ou Saint Denis e atacam NPCs humanos da cidade." : "Creates 1 clone per active player in two lines, with staggered spawning to reduce hitches. After 10 s, both groups fight with stock AI. Survivors use nearby horses, head to Armadillo or Saint Denis and attack human NPCs in the city.");
				ImGui::TextDisabled("%s", Localization::IsPortuguese() ? "O duelo inicial é desarmado para preservar os 800 HP e evitar mortes instantâneas. A arma escolhida acima é equipada na expedição. 40% dos sobreviventes dormem 15 s uma única vez no caminho." : "The opening brawl is unarmed to preserve the 800 HP fights and avoid instant kills. The weapon selected above is equipped for the expedition. 40% of survivors sleep once for 15 seconds on the way.");
				if (g_SessionBrawlRunning) ImGui::TextDisabled("%s", Localization::IsPortuguese() ? "Evento em andamento — APAGAR TODOS OS CLONES também cancela o controlador do evento." : "Event running — DELETE ALL CLONES also cancels the event controller.");

				ImGui::Spacing(); ImGui::SeparatorText(Localization::IsPortuguese() ? "POSIÇÃO / LIMPEZA" : "POSITION / CLEANUP");
				if (ImGui::Button(Localization::IsPortuguese() ? "CRIAR NA MINHA FRENTE" : "CREATE IN FRONT OF ME", ImVec2(215.0f, 34.0f))) { const CloneOptions options = GetOptions(); FiberPool::Push([options] { SpawnManualCloneNearPlayer(options); }); }
				ImGui::SameLine(); if (ImGui::Button("FREECAM MULTI-SPAWN", ImVec2(225.0f, 34.0f))) { const CloneOptions options = GetOptions(); FiberPool::Push([options] { RunCloneSpawnFreecam(options); }); }
				if (ImGui::Button(Localization::IsPortuguese() ? "APAGAR TODOS OS CLONES" : "DELETE ALL CLONES", ImVec2(-1.0f, 32.0f))) FiberPool::Push([] { DeleteAllManagedClones(); });
				ImGui::TextDisabled("%s", Localization::IsPortuguese() ? "Na Freecam, ENTER cria sem fechar a câmera; BACK encerra. O círculo branco marca o spawn." : "In Freecam, ENTER spawns without closing the camera; BACK exits. The white circle marks the spawn.");
			}
			std::string_view GetMenuLabel() const override { return Localization::IsPortuguese() ? "Criar meu clone" : "Create my clone"; }
			std::string GetMenuValue() const override { return Localization::IsPortuguese() ? ModeLabelPt(m_Mode) : ModeLabelEn(m_Mode); }
			std::string_view GetMenuDescription() const override { return Localization::IsPortuguese() ? "Clones seus ou da sessão com IA nativa, guarda-costas, batalha coletiva, expedição, multi-spawn, loot e emotes." : "Clones of you or session players with stock AI, bodyguards, session brawl, expedition, multi-spawn, loot and emotes."; }
			bool RequiresImGuiEditor() const override { return true; }
			float GetPreferredEditorHeight() const override { return 1010.0f; }
		};
	}
	std::shared_ptr<UIItem> CreateManualCloneItem() { return std::make_shared<ManualCloneItem>(); }
}
