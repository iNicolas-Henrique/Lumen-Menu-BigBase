#include "core/commands/LoopedCommand.hpp"
#include "game/backend/ScriptMgr.hpp"
#include "game/backend/Self.hpp"
#include "game/rdr/Natives.hpp"
#include "game/rdr/Ped.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <random>
#include <unordered_set>
#include <vector>

namespace YimMenu::Features
{
    namespace
    {
        constexpr int kAttackersPerWave = 20;
        constexpr float kMinSpawnDistance = 160.0f;
        constexpr float kMaxSpawnDistance = 220.0f;
        constexpr float kNearbyNpcRadius = 80.0f;
        constexpr auto kNearbyScanInterval = std::chrono::milliseconds(600);
        constexpr float kRadToDeg = 57.29577951308232f;

        constexpr std::array<const char*, 3> kWarriorModels = {
            "a_m_m_wapwarriors_01",
            "u_m_m_nbxindianowner_01",
            "a_f_m_wapnative_01",
        };

        constexpr std::array<const char*, 4> kHorseModels = {
            "A_C_Horse_AmericanPaint_Greyovero",
            "A_C_Horse_AmericanPaint_Overo",
            "A_C_Horse_AmericanPaint_SplashedWhite",
            "A_C_Horse_AmericanPaint_Tobiano",
        };

        struct MountedAttacker
        {
            int Rider{};
            int Horse{};
        };

        std::vector<MountedAttacker> g_Attackers;
        std::unordered_set<int> g_ForcedNearbyPeds;

        std::mt19937& Rng()
        {
            static std::mt19937 rng{std::random_device{}()};
            return rng;
        }

        bool LoadModel(Hash model)
        {
            if (!STREAMING::IS_MODEL_VALID(model))
                return false;

            STREAMING::REQUEST_MODEL(model, false);
            for (int tries = 0; tries < 120 && !STREAMING::HAS_MODEL_LOADED(model); ++tries)
                ScriptMgr::Yield();

            return STREAMING::HAS_MODEL_LOADED(model);
        }

        bool FindHiddenSpawn(const Vector3& playerPos, Vector3& outPos)
        {
            std::uniform_real_distribution<float> angleDist(0.0f, 6.283185307179586f);
            std::uniform_real_distribution<float> distanceDist(kMinSpawnDistance, kMaxSpawnDistance);

            for (int attempt = 0; attempt < 32; ++attempt)
            {
                const float angle = angleDist(Rng());
                const float distance = distanceDist(Rng());

                Vector3 candidate{
                    playerPos.x + std::cos(angle) * distance,
                    playerPos.y + std::sin(angle) * distance,
                    playerPos.z + 100.0f,
                };

                float groundZ = 0.0f;
                if (!MISC::GET_GROUND_Z_FOR_3D_COORD(candidate.x, candidate.y, candidate.z, &groundZ, 0))
                    continue;

                candidate.z = groundZ;
                if (CAM::IS_SPHERE_VISIBLE(candidate.x, candidate.y, candidate.z + 1.0f, 8.0f))
                    continue;

                outPos = candidate;
                return true;
            }

            return false;
        }

        void ConfigureAggressiveCombat(int pedHandle, int targetPed, bool ranged)
        {
            PED::SET_PED_KEEP_TASK(pedHandle, true);
            PED::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(pedHandle, true);
            PED::SET_PED_COMBAT_ABILITY(pedHandle, 2);
            PED::SET_PED_COMBAT_MOVEMENT(pedHandle, 2);
            PED::SET_PED_COMBAT_RANGE(pedHandle, ranged ? 3 : 2);
            PED::SET_PED_ACCURACY(pedHandle, ranged ? 78 : 65);

            PED::SET_PED_COMBAT_ATTRIBUTES(pedHandle, 5, true);   // Always fight
            PED::SET_PED_COMBAT_ATTRIBUTES(pedHandle, 13, true);  // Aggressive
            PED::SET_PED_COMBAT_ATTRIBUTES(pedHandle, 17, false); // Never force flee
            PED::SET_PED_COMBAT_ATTRIBUTES(pedHandle, 46, true);  // Fight armed targets
            PED::SET_PED_COMBAT_ATTRIBUTES(pedHandle, 58, true);  // Disable flee from combat

            TASK::TASK_COMBAT_PED(pedHandle, targetPed, 0, 16);
        }

        void GiveFireBow(int pedHandle)
        {
            const Hash bow = Joaat("WEAPON_BOW");
            const Hash fireArrow = Joaat("AMMO_ARROW_FIRE");

            WEAPON::GIVE_WEAPON_TO_PED(
                pedHandle,
                bow,
                80,
                true,
                true,
                0,
                false,
                0.5f,
                1.0f,
                1.0f,
                false,
                0,
                false);
            WEAPON::SET_PED_AMMO_BY_TYPE(pedHandle, fireArrow, 80);
            WEAPON::_SET_AMMO_TYPE_FOR_PED_WEAPON(pedHandle, bow, fireArrow);
            WEAPON::SET_CURRENT_PED_WEAPON(pedHandle, bow, true, 0, false, false);
        }

        void ForgetDeadAttackers()
        {
            auto it = g_Attackers.begin();
            while (it != g_Attackers.end())
            {
                if (!ENTITY::DOES_ENTITY_EXIST(it->Rider) || ENTITY::IS_ENTITY_DEAD(it->Rider))
                    it = g_Attackers.erase(it);
                else
                    ++it;
            }
        }

        void ReleaseLivingAttackers()
        {
            for (const auto& attacker : g_Attackers)
            {
                if (!ENTITY::DOES_ENTITY_EXIST(attacker.Rider) || ENTITY::IS_ENTITY_DEAD(attacker.Rider))
                    continue;

                TASK::CLEAR_PED_TASKS(attacker.Rider, true, false);
                PED::SET_PED_KEEP_TASK(attacker.Rider, false);
                PED::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(attacker.Rider, false);
            }
            g_Attackers.clear();
        }

        void SpawnMountedWave(int targetPed)
        {
            const Vector3 playerPos = ENTITY::GET_ENTITY_COORDS(targetPed, true, false);
            std::uniform_int_distribution<size_t> warriorDist(0, kWarriorModels.size() - 1);
            std::uniform_int_distribution<size_t> horseDist(0, kHorseModels.size() - 1);

            for (int i = 0; i < kAttackersPerWave; ++i)
            {
                Vector3 spawnPos{};
                if (!FindHiddenSpawn(playerPos, spawnPos))
                    continue;

                const Hash warriorModel = Joaat(kWarriorModels[warriorDist(Rng())]);
                const Hash horseModel = Joaat(kHorseModels[horseDist(Rng())]);
                if (!LoadModel(warriorModel) || !LoadModel(horseModel))
                    continue;

                const float dx = playerPos.x - spawnPos.x;
                const float dy = playerPos.y - spawnPos.y;
                const float heading = std::atan2(dx, dy) * kRadToDeg;

                Ped horse = Ped::Create(horseModel, spawnPos, heading);
                if (!horse.IsValid())
                    continue;

                Ped rider = Ped::Create(warriorModel, spawnPos, heading);
                if (!rider.IsValid())
                {
                    int horseHandle = horse.GetHandle();
                    PED::DELETE_PED(&horseHandle);
                    continue;
                }

                rider.SetInMount(horse, -1);
                GiveFireBow(rider.GetHandle());
                ConfigureAggressiveCombat(rider.GetHandle(), targetPed, true);
                g_Attackers.push_back({rider.GetHandle(), horse.GetHandle()});
            }
        }

        void RestoreNearbyPeds()
        {
            for (const int pedHandle : g_ForcedNearbyPeds)
            {
                if (!ENTITY::DOES_ENTITY_EXIST(pedHandle) || ENTITY::IS_ENTITY_DEAD(pedHandle))
                    continue;

                TASK::CLEAR_PED_TASKS(pedHandle, true, false);
                PED::SET_PED_KEEP_TASK(pedHandle, false);
            }
            g_ForcedNearbyPeds.clear();
        }
    }

    class IndianAttack : public LoopedCommand
    {
        using LoopedCommand::LoopedCommand;

        void OnTick() override
        {
            Ped self = Self::GetPed();
            if (!self.IsValid() || self.GetHealth() <= 0)
                return;

            ForgetDeadAttackers();
            if (g_Attackers.empty())
                SpawnMountedWave(self.GetHandle());
        }

        void OnDisable() override
        {
            ReleaseLivingAttackers();
        }
    };

    class NearbyHostilePeds : public LoopedCommand
    {
        using LoopedCommand::LoopedCommand;

        void OnTick() override
        {
            static auto lastScan = std::chrono::steady_clock::time_point{};
            const auto now = std::chrono::steady_clock::now();
            if (now - lastScan < kNearbyScanInterval)
                return;
            lastScan = now;

            Ped self = Self::GetPed();
            if (!self.IsValid() || self.GetHealth() <= 0)
                return;

            const int selfHandle = self.GetHandle();
            const Vector3 selfPos = self.GetPosition();
            int nearbyPeds[64]{};
            const int count = PED::GET_PED_NEARBY_PEDS(selfHandle, nearbyPeds, 0, 64);

            for (int i = 0; i < count; ++i)
            {
                const int candidate = nearbyPeds[i];
                if (!candidate || candidate == selfHandle || !ENTITY::DOES_ENTITY_EXIST(candidate) || ENTITY::IS_ENTITY_DEAD(candidate))
                    continue;
                if (!PED::IS_PED_HUMAN(candidate) || PED::IS_PED_A_PLAYER(candidate))
                    continue;

                const Vector3 pedPos = ENTITY::GET_ENTITY_COORDS(candidate, true, false);
                if (MISC::GET_DISTANCE_BETWEEN_COORDS(
                        selfPos.x, selfPos.y, selfPos.z,
                        pedPos.x, pedPos.y, pedPos.z,
                        true) > kNearbyNpcRadius)
                    continue;

                if (g_ForcedNearbyPeds.insert(candidate).second)
                {
                    PED::SET_PED_COMBAT_ATTRIBUTES(candidate, 5, true);
                    PED::SET_PED_COMBAT_ATTRIBUTES(candidate, 13, true);
                    PED::SET_PED_COMBAT_ATTRIBUTES(candidate, 17, false);
                    PED::SET_PED_COMBAT_ATTRIBUTES(candidate, 46, true);
                    PED::SET_PED_COMBAT_ATTRIBUTES(candidate, 58, true);
                    PED::SET_PED_KEEP_TASK(candidate, true);
                }

                if (!PED::IS_PED_IN_COMBAT(candidate, selfHandle))
                    TASK::TASK_COMBAT_PED(candidate, selfHandle, 0, 16);
            }
        }

        void OnDisable() override
        {
            RestoreNearbyPeds();
        }
    };

    static IndianAttack _IndianAttack{
        "indianattack",
        "Indian Attack",
        "Spawns a large mounted ambush far from view. Riders use fire arrows and target only your local character."};

    static NearbyHostilePeds _NearbyHostilePeds{
        "nearbyhostilepeds",
        "NPCs Proximos Hostis",
        "Makes nearby human NPCs aggressively attack only your local character and resist fleeing."};
}
