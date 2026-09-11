#include "core/commands/LoopedCommand.hpp"
#include "game/backend/Self.hpp"
#include "game/rdr/Natives.hpp"
#include "game/rdr/Pools.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <random>
#include <unordered_map>
#include <vector>

namespace YimMenu::Features
{
    namespace
    {
        using Clock = std::chrono::steady_clock;

        constexpr float kRiotRadius = 115.0f;
        constexpr float kRiotRadiusSq = kRiotRadius * kRiotRadius;
        constexpr float kMeleeFinishDistance = 2.15f;
        constexpr float kMeleeFinishDistanceSq = kMeleeFinishDistance * kMeleeFinishDistance;
        constexpr float kCloneSpawnMin = 5.0f;
        constexpr float kCloneSpawnMax = 10.0f;
        constexpr int kCloneHealth = 800;
        constexpr auto kScanInterval = std::chrono::milliseconds(500);
        constexpr auto kRetaskInterval = std::chrono::milliseconds(1000);

        struct Fighter
        {
            int Ped{};
            int Target{};
            bool Clone{};
            Clock::time_point NextTask{};
            Clock::time_point NextVoice{};
            Clock::time_point NextHitCheck{};
        };

        std::vector<Fighter> g_Fighters;
        std::vector<int> g_Clones;
        std::vector<int> g_LocalHumans;
        Clock::time_point g_NextScan{};
        Clock::time_point g_NextClone{};
        Clock::time_point g_NextLawClear{};

        std::mt19937& Rng()
        {
            static std::mt19937 rng{std::random_device{}()};
            return rng;
        }

        int RandomInt(int min, int max)
        {
            return std::uniform_int_distribution<int>(min, max)(Rng());
        }

        float RandomFloat(float min, float max)
        {
            return std::uniform_real_distribution<float>(min, max)(Rng());
        }

        float DistanceSquared(const Vector3& a, const Vector3& b)
        {
            const float dx = a.x - b.x;
            const float dy = a.y - b.y;
            const float dz = a.z - b.z;
            return dx * dx + dy * dy + dz * dz;
        }

        bool IsLawPed(int ped)
        {
            if (!ped || !ENTITY::DOES_ENTITY_EXIST(ped))
                return false;
            const Hash group = PED::GET_PED_RELATIONSHIP_GROUP_HASH(ped);
            return group == Joaat("COP") || group == Joaat("REL_COP") ||
                   group == Joaat("REL_GUAMA_LAW") || group == Joaat("REL_PINKERTONS") ||
                   group == Joaat("REL_PLAYER_COP");
        }

        void SuppressLaw()
        {
            LAW::CLEAR_BOUNTY(PLAYER::PLAYER_ID());
            LAW::CLEAR_WANTED_SCORE(PLAYER::PLAYER_ID());
            LAW::_SET_BOUNTY_HUNTER_PURSUIT_CLEARED();
        }

        void GiveWeapon(int ped, const char* weaponName, int ammo = 60)
        {
            const Hash weapon = Joaat(weaponName);
            WEAPON::GIVE_WEAPON_TO_PED(ped, weapon, ammo, true, true, 0, false, 0.5f, 1.0f, 1.0f, false, 0, false);
            WEAPON::SET_CURRENT_PED_WEAPON(ped, weapon, true, 0, false, false);
        }

        void ArmRandomly(int ped)
        {
            static constexpr std::array<const char*, 12> weapons{
                "WEAPON_UNARMED",
                "WEAPON_UNARMED",
                "WEAPON_UNARMED",
                "WEAPON_MELEE_KNIFE",
                "WEAPON_MELEE_MACHETE",
                "WEAPON_MELEE_HATCHET",
                "WEAPON_MELEE_CLEAVER",
                "WEAPON_REVOLVER_CATTLEMAN",
                "WEAPON_REVOLVER_DOUBLEACTION",
                "WEAPON_PISTOL_M1899",
                "WEAPON_REPEATER_CARBINE",
                "WEAPON_SHOTGUN_SAWEDOFF"};

            WEAPON::REMOVE_ALL_PED_WEAPONS(ped, true, true);
            const char* selected = weapons[RandomInt(0, static_cast<int>(weapons.size()) - 1)];
            if (std::string_view(selected) == "WEAPON_UNARMED")
                WEAPON::SET_CURRENT_PED_WEAPON(ped, Joaat("WEAPON_UNARMED"), true, 0, false, false);
            else
                GiveWeapon(ped, selected);
        }

        void ConfigureFighter(int ped, bool clone)
        {
            PED::SET_PED_KEEP_TASK(ped, true);
            PED::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(ped, true);
            PED::SET_PED_COMBAT_ABILITY(ped, 3);
            PED::SET_PED_COMBAT_MOVEMENT(ped, 3);
            PED::SET_PED_COMBAT_RANGE(ped, 1);
            PED::SET_PED_ACCURACY(ped, clone ? 86 : 58);
            PED::SET_PED_SEEING_RANGE(ped, 150.0f);
            PED::SET_PED_HEARING_RANGE(ped, 150.0f);
            PED::SET_PED_MOVE_RATE_OVERRIDE(ped, clone ? 1.70f : 1.42f);

            for (int attr : {4, 5, 8, 13, 21, 25, 28, 31, 39, 41, 42, 46, 49, 50, 56, 58, 63, 68, 78, 80, 81, 92, 93, 115})
                PED::SET_PED_COMBAT_ATTRIBUTES(ped, attr, true);
            PED::SET_PED_COMBAT_ATTRIBUTES(ped, 17, false);
            PED::SET_PED_COMBAT_ATTRIBUTES(ped, 125, false);

            PED::SET_PED_TO_PLAYER_WEAPON_DAMAGE_MODIFIER(ped, clone ? 1.38f : 1.15f);
        }

        void PlayRiotVoice(int ped)
        {
            static const std::array<Hash, 6> emotes{
                Joaat("KIT_EMOTE_TAUNT_WAR_CRY_1"),
                Joaat("KIT_EMOTE_REACTION_FRIGHTENED_1"),
                Joaat("KIT_EMOTE_REACTION_ANGRY_1"),
                Joaat("KIT_EMOTE_REACTION_DISAGREE_1"),
                Joaat("KIT_EMOTE_TAUNT_INSULT_1"),
                Joaat("KIT_EMOTE_TAUNT_DANCE_1")};
            TASK::TASK_PLAY_EMOTE_WITH_HASH(ped, 2, 0, emotes[RandomInt(0, static_cast<int>(emotes.size()) - 1)], true, true, false, false, false);
        }

        std::vector<int> CollectLocalHumans(int selfHandle, const Vector3& selfPos)
        {
            std::vector<int> result;
            result.reserve(64);
            for (Ped ped : Pools::GetPeds())
            {
                if (!ped.IsValid())
                    continue;
                const int handle = ped.GetHandle();
                if (!handle || handle == selfHandle || PED::IS_PED_A_PLAYER(handle) || !PED::IS_PED_HUMAN(handle) || ENTITY::IS_ENTITY_DEAD(handle))
                    continue;

                if (IsLawPed(handle))
                {
                    int law = handle;
                    PED::DELETE_PED(&law);
                    continue;
                }

                const Vector3 pedPos = ENTITY::GET_ENTITY_COORDS(handle, true, false);
                if (DistanceSquared(pedPos, selfPos) <= kRiotRadiusSq)
                    result.push_back(handle);
            }
            return result;
        }

        int ChooseTarget(int source, int selfHandle, const std::vector<int>& humans)
        {
            if (RandomInt(1, 100) <= 18)
                return selfHandle;

            std::vector<int> candidates;
            candidates.reserve(humans.size());
            for (int ped : humans)
                if (ped != source && ENTITY::DOES_ENTITY_EXIST(ped) && !ENTITY::IS_ENTITY_DEAD(ped))
                    candidates.push_back(ped);
            if (candidates.empty())
                return selfHandle;
            return candidates[RandomInt(0, static_cast<int>(candidates.size()) - 1)];
        }

        Fighter* FindFighter(int ped)
        {
            auto it = std::find_if(g_Fighters.begin(), g_Fighters.end(), [ped](const Fighter& fighter) { return fighter.Ped == ped; });
            return it == g_Fighters.end() ? nullptr : &*it;
        }

        void Retask(Fighter& fighter, int selfHandle, const std::vector<int>& humans, const Clock::time_point now)
        {
            if (!fighter.Ped || !ENTITY::DOES_ENTITY_EXIST(fighter.Ped) || ENTITY::IS_ENTITY_DEAD(fighter.Ped))
                return;

            const bool targetInvalid = !fighter.Target || !ENTITY::DOES_ENTITY_EXIST(fighter.Target) ||
                ENTITY::IS_ENTITY_DEAD(fighter.Target) || fighter.Target == fighter.Ped;
            if (targetInvalid)
            {
                fighter.Target = ChooseTarget(fighter.Ped, selfHandle, humans);
                fighter.NextTask = now;
            }

            if (fighter.Target && now >= fighter.NextTask && !PED::IS_PED_IN_COMBAT(fighter.Ped, fighter.Target))
            {
                PED::SET_PED_COMBAT_MOVEMENT(fighter.Ped, 3);
                PED::SET_PED_MOVE_RATE_OVERRIDE(fighter.Ped, fighter.Clone ? 1.72f : 1.45f);
                TASK::TASK_COMBAT_PED(fighter.Ped, fighter.Target, 0, 16);
                fighter.NextTask = now + kRetaskInterval;
            }

            if (now >= fighter.NextVoice)
            {
                if (RandomInt(1, 100) <= 55)
                    PlayRiotVoice(fighter.Ped);
                fighter.NextVoice = now + std::chrono::milliseconds(RandomInt(2600, 5600));
            }

            if (fighter.Target != selfHandle && fighter.Target && now >= fighter.NextHitCheck)
            {
                const Vector3 a = ENTITY::GET_ENTITY_COORDS(fighter.Ped, true, false);
                const Vector3 b = ENTITY::GET_ENTITY_COORDS(fighter.Target, true, false);
                if (DistanceSquared(a, b) <= kMeleeFinishDistanceSq && PED::IS_PED_IN_COMBAT(fighter.Ped, fighter.Target))
                    ENTITY::SET_ENTITY_HEALTH(fighter.Target, 0, fighter.Ped);
                fighter.NextHitCheck = now + std::chrono::milliseconds(650);
            }
        }

        bool FindCloneSpawn(int selfHandle, const Vector3& selfPos, Vector3& out)
        {
            const float heading = ENTITY::GET_ENTITY_HEADING(selfHandle) * 0.017453292519943295f;
            for (int i = 0; i < 16; ++i)
            {
                const float distance = RandomFloat(kCloneSpawnMin, kCloneSpawnMax);
                const float side = RandomFloat(-2.5f, 2.5f);
                Vector3 pos{
                    selfPos.x - std::sin(heading) * distance + std::cos(heading) * side,
                    selfPos.y - std::cos(heading) * distance - std::sin(heading) * side,
                    selfPos.z + 20.0f};
                float groundZ{};
                if (!MISC::GET_GROUND_Z_FOR_3D_COORD(pos.x, pos.y, pos.z, &groundZ, 0))
                    continue;
                pos.z = groundZ;
                out = pos;
                return true;
            }
            return false;
        }

        int CreateDoppelgangerShell(int selfHandle, const Vector3& pos)
        {
            if (!selfHandle || !ENTITY::DOES_ENTITY_EXIST(selfHandle))
                return 0;

            const Hash model = ENTITY::GET_ENTITY_MODEL(selfHandle);
            if (!model || !STREAMING::IS_MODEL_IN_CDIMAGE(model) || !STREAMING::HAS_MODEL_LOADED(model))
                return 0;

            const float heading = ENTITY::GET_ENTITY_HEADING(selfHandle);
            LOG(INFO) << "[PlayerMustDie] creating local doppelganger shell; model=" << model;
            int clone = PED::CREATE_PED(model, pos.x, pos.y, pos.z, heading, false, 0, 0, 0);
            if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone) || PED::IS_PED_A_PLAYER(clone))
            {
                LOG(WARNING) << "[PlayerMustDie] local doppelganger shell was invalid";
                if (clone && ENTITY::DOES_ENTITY_EXIST(clone) && !PED::IS_PED_A_PLAYER(clone))
                    PED::DELETE_PED(&clone);
                return 0;
            }

            ENTITY::SET_ENTITY_AS_MISSION_ENTITY(clone, true, true);
            if (!ENTITY::DOES_ENTITY_EXIST(selfHandle) || !ENTITY::DOES_ENTITY_EXIST(clone))
            {
                if (clone && ENTITY::DOES_ENTITY_EXIST(clone))
                    PED::DELETE_PED(&clone);
                return 0;
            }

            PED::CLONE_PED_TO_TARGET(selfHandle, clone);
            if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone) || PED::IS_PED_A_PLAYER(clone))
            {
                LOG(WARNING) << "[PlayerMustDie] doppelganger became invalid after appearance copy";
                if (clone && ENTITY::DOES_ENTITY_EXIST(clone) && !PED::IS_PED_A_PLAYER(clone))
                    PED::DELETE_PED(&clone);
                return 0;
            }

            PED::_UPDATE_PED_VARIATION(clone, 0, 1, 1, 1, 0);
            if (!ENTITY::DOES_ENTITY_EXIST(clone))
                return 0;

            LOG(INFO) << "[PlayerMustDie] doppelganger appearance copied and refreshed; handle=" << clone;
            return clone;
        }

        void SpawnClone(int selfHandle, const Vector3& selfPos, const std::vector<int>& humans, const Clock::time_point now)
        {
            g_Clones.erase(std::remove_if(g_Clones.begin(), g_Clones.end(), [](int ped) {
                return !ped || !ENTITY::DOES_ENTITY_EXIST(ped) || ENTITY::IS_ENTITY_DEAD(ped);
            }), g_Clones.end());

            if (g_Clones.size() >= 2 || now < g_NextClone)
                return;
            g_NextClone = now + std::chrono::milliseconds(RandomInt(5000, 8500));

            Vector3 pos{};
            if (!FindCloneSpawn(selfHandle, selfPos, pos))
                return;

            int clone = CreateDoppelgangerShell(selfHandle, pos);
            if (!clone)
                return;

            ENTITY::PLACE_ENTITY_ON_GROUND_PROPERLY(clone, true);
            ENTITY::SET_ENTITY_MAX_HEALTH(clone, kCloneHealth);
            ENTITY::SET_ENTITY_HEALTH(clone, kCloneHealth, 0);

            ArmRandomly(clone);
            ConfigureFighter(clone, true);

            Fighter fighter{};
            fighter.Ped = clone;
            fighter.Clone = true;
            fighter.Target = ChooseTarget(clone, selfHandle, humans);
            fighter.NextTask = now;
            fighter.NextVoice = now + std::chrono::milliseconds(RandomInt(1200, 2600));
            fighter.NextHitCheck = now;
            Retask(fighter, selfHandle, humans, now);
            g_Fighters.push_back(fighter);
            g_Clones.push_back(clone);
            LOG(INFO) << "[PlayerMustDie] doppelganger ready; handle=" << clone;
        }

        void Cleanup()
        {
            for (auto& fighter : g_Fighters)
            {
                if (!fighter.Ped || !ENTITY::DOES_ENTITY_EXIST(fighter.Ped))
                    continue;
                if (fighter.Clone)
                {
                    int ped = fighter.Ped;
                    PED::DELETE_PED(&ped);
                }
                else
                {
                    TASK::CLEAR_PED_TASKS(fighter.Ped, true, false);
                    PED::SET_PED_KEEP_TASK(fighter.Ped, false);
                    PED::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(fighter.Ped, false);
                    PED::SET_PED_MOVE_RATE_OVERRIDE(fighter.Ped, 1.0f);
                    PED::SET_PED_TO_PLAYER_WEAPON_DAMAGE_MODIFIER(fighter.Ped, 1.0f);
                }
            }
            g_Fighters.clear();
            g_Clones.clear();
            g_LocalHumans.clear();
            g_NextScan = {};
            g_NextClone = {};
            g_NextLawClear = {};
        }
    }

    class PlayerMustDie final : public LoopedCommand
    {
        using LoopedCommand::LoopedCommand;

        void OnTick() override
        {
            Ped self = Self::GetPed();
            if (!self.IsValid() || self.GetHealth() <= 0)
                return;

            const int selfHandle = self.GetHandle();
            const Vector3 selfPos = self.GetPosition();
            const auto now = Clock::now();

            if (now >= g_NextLawClear)
            {
                SuppressLaw();
                g_NextLawClear = now + std::chrono::milliseconds(750);
            }

            if (now >= g_NextScan)
            {
                g_NextScan = now + kScanInterval;
                g_LocalHumans = CollectLocalHumans(selfHandle, selfPos);
                for (int ped : g_LocalHumans)
                {
                    if (FindFighter(ped))
                        continue;
                    ArmRandomly(ped);
                    ConfigureFighter(ped, false);
                    Fighter fighter{};
                    fighter.Ped = ped;
                    fighter.Target = ChooseTarget(ped, selfHandle, g_LocalHumans);
                    fighter.NextTask = now;
                    fighter.NextVoice = now + std::chrono::milliseconds(RandomInt(0, 1400));
                    fighter.NextHitCheck = now;
                    g_Fighters.push_back(fighter);
                }
            }

            g_Fighters.erase(std::remove_if(g_Fighters.begin(), g_Fighters.end(), [](const Fighter& fighter) {
                return !fighter.Ped || !ENTITY::DOES_ENTITY_EXIST(fighter.Ped) || ENTITY::IS_ENTITY_DEAD(fighter.Ped);
            }), g_Fighters.end());

            for (auto& fighter : g_Fighters)
                Retask(fighter, selfHandle, g_LocalHumans, now);

            SpawnClone(selfHandle, selfPos, g_LocalHumans, now);
        }

        void OnDisable() override
        {
            Cleanup();
            SuppressLaw();
        }
    };

    static PlayerMustDie _PlayerMustDie{
        "nearbyhostilepeds",
        "Player Must Die",
        "Starts a frantic local NPC riot. Most civilians fight each other, some attack you, law is suppressed, and hostile 800-health doppelgangers join the chaos."};
}
