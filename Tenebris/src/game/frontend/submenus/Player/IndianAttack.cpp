#include "core/commands/LoopedCommand.hpp"
#include "game/backend/ScriptMgr.hpp"
#include "game/backend/Self.hpp"
#include "game/rdr/Natives.hpp"

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

        constexpr float kAmbientRadius = 210.0f;
        constexpr int kMaxNearbyPeds = 128;
        constexpr std::size_t kMaxAmbientAttackers = 34;
        constexpr std::size_t kMaxSpecialActors = 34;
        constexpr int kDoppelgangersPerRegion = 2;
        constexpr auto kScanInterval = std::chrono::milliseconds(520);
        constexpr auto kRetaskInterval = std::chrono::milliseconds(1700);
        constexpr float kRadToDeg = 57.29577951308232f;

        enum class Region
        {
            Default,
            Lagras,
            Armadillo,
            SaintDenis,
            Valentine,
            Strawberry,
            Heartlands,
            Snow
        };

        enum class Role
        {
            Regional,
            Torch,
            NativePatrol,
            Alligator,
            Doppelganger
        };

        struct AmbientState
        {
            Vector3 LastPos{};
            Clock::time_point LastMove{};
            Clock::time_point NextTask{};
            Clock::time_point NextPoison{};
            Region Area{Region::Default};
        };

        struct Actor
        {
            int Ped{};
            Role Type{Role::Regional};
            Region Area{Region::Default};
            Vector3 LastPos{};
            Clock::time_point LastMove{};
            Clock::time_point NextTask{};
            Clock::time_point NextVoice{};
            Clock::time_point NextStrike{};
            bool LightningUsed{};
            bool Dormant{};
        };

        std::unordered_map<int, AmbientState> g_Ambient;
        std::vector<Actor> g_Actors;
        std::vector<int> g_LawAllies;
        Clock::time_point g_NextSpecial{};
        Clock::time_point g_NextDoppel{};
        Clock::time_point g_NextLawSpawn{};
        Clock::time_point g_NextActorTick{};
        Clock::time_point g_NextLawTick{};
        Clock::time_point g_NextLawClear{};
        Clock::time_point g_NextSwampVoice{};
        Region g_LastRegion{Region::Default};

        FORCEINLINE constexpr void ForceLightningAtCoords(float x, float y, float z, float intensity)
        {
            return YimMenu::NativeInvoker::Invoke<2412, void, false>(x, y, z, intensity);
        }

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

        Clock::time_point AfterMs(int min, int max)
        {
            return Clock::now() + std::chrono::milliseconds(RandomInt(min, max));
        }

        float Distance(const Vector3& a, const Vector3& b)
        {
            return MISC::GET_DISTANCE_BETWEEN_COORDS(a.x, a.y, a.z, b.x, b.y, b.z, true);
        }

        bool Near(const Vector3& p, float x, float y, float radius)
        {
            const float dx = p.x - x;
            const float dy = p.y - y;
            return dx * dx + dy * dy <= radius * radius;
        }

        Region GetRegion(const Vector3& p)
        {
            if (Near(p, 2138.0f, -638.0f, 760.0f))
                return Region::Lagras;
            if (Near(p, -3705.0f, -2600.0f, 560.0f))
                return Region::Armadillo;
            if (Near(p, 2620.0f, -1285.0f, 720.0f))
                return Region::SaintDenis;
            if (Near(p, -285.0f, 790.0f, 520.0f))
                return Region::Valentine;
            if (Near(p, -1780.0f, -390.0f, 520.0f))
                return Region::Strawberry;

            if (p.y > 1050.0f && p.x > -2300.0f && p.x < 1800.0f)
                return Region::Snow;

            if (p.x > -1100.0f && p.x < 2100.0f && p.y > -450.0f && p.y < 1500.0f)
                return Region::Heartlands;

            return Region::Default;
        }

        bool LoadModel(Hash model)
        {
            if (!STREAMING::IS_MODEL_IN_CDIMAGE(model) || !STREAMING::IS_MODEL_VALID(model))
                return false;

            STREAMING::REQUEST_MODEL(model, false);
            for (int i = 0; i < 80 && !STREAMING::HAS_MODEL_LOADED(model); ++i)
                ScriptMgr::Yield(std::chrono::milliseconds(10));

            return STREAMING::HAS_MODEL_LOADED(model);
        }

        bool FindSpawn(const Vector3& target, float minDistance, float maxDistance, bool hideFromCamera, Vector3& out)
        {
            for (int i = 0; i < 30; ++i)
            {
                const float angle = RandomFloat(0.0f, 6.283185307179586f);
                const float distance = RandomFloat(minDistance, maxDistance);
                Vector3 pos{
                    target.x + std::cos(angle) * distance,
                    target.y + std::sin(angle) * distance,
                    target.z + 90.0f};

                float groundZ{};
                if (!MISC::GET_GROUND_Z_FOR_3D_COORD(pos.x, pos.y, pos.z, &groundZ, 0))
                    continue;

                pos.z = groundZ;
                if (hideFromCamera && CAM::IS_SPHERE_VISIBLE(pos.x, pos.y, pos.z + 1.0f, 4.0f))
                    continue;

                out = pos;
                return true;
            }
            return false;
        }

        bool FindBehindSpawn(int selfHandle, const Vector3& selfPos, Vector3& out)
        {
            const float heading = ENTITY::GET_ENTITY_HEADING(selfHandle) / kRadToDeg;
            for (int i = 0; i < 10; ++i)
            {
                const float distance = RandomFloat(9.0f, 16.0f);
                const float side = RandomFloat(-3.5f, 3.5f);
                Vector3 pos{
                    selfPos.x - std::sin(heading) * distance + std::cos(heading) * side,
                    selfPos.y - std::cos(heading) * distance - std::sin(heading) * side,
                    selfPos.z + 30.0f};

                float groundZ{};
                if (!MISC::GET_GROUND_Z_FOR_3D_COORD(pos.x, pos.y, pos.z, &groundZ, 0))
                    continue;
                pos.z = groundZ;

                if (CAM::IS_SPHERE_VISIBLE(pos.x, pos.y, pos.z + 1.0f, 2.5f))
                    continue;

                out = pos;
                return true;
            }
            return FindSpawn(selfPos, 10.0f, 18.0f, true, out);
        }

        int CreateLocalPed(const char* modelName, const Vector3& pos, float heading)
        {
            const Hash model = Joaat(modelName);
            if (!LoadModel(model))
                return 0;

            const int ped = PED::CREATE_PED(model, pos.x, pos.y, pos.z, heading, false, 0, 0, 0);
            if (ped)
            {
                ENTITY::SET_ENTITY_AS_MISSION_ENTITY(ped, true, true);
                PED::_SET_RANDOM_OUTFIT_VARIATION(ped, true);
                ENTITY::PLACE_ENTITY_ON_GROUND_PROPERLY(ped, true);
            }
            STREAMING::SET_MODEL_AS_NO_LONGER_NEEDED(model);
            return ped;
        }

        bool IsLawPed(int ped)
        {
            const Hash group = PED::GET_PED_RELATIONSHIP_GROUP_HASH(ped);
            return group == Joaat("COP") ||
                   group == Joaat("REL_COP") ||
                   group == Joaat("REL_GUAMA_LAW") ||
                   group == Joaat("REL_PINKERTONS") ||
                   group == Joaat("REL_PLAYER_COP");
        }

        bool IsLawAlly(int ped)
        {
            return std::find(g_LawAllies.begin(), g_LawAllies.end(), ped) != g_LawAllies.end();
        }

        bool IsOurActor(int ped)
        {
            return std::any_of(g_Actors.begin(), g_Actors.end(), [ped](const Actor& actor) {
                return actor.Ped == ped;
            });
        }

        void ClearLaw()
        {
            LAW::CLEAR_BOUNTY(PLAYER::PLAYER_ID());
            LAW::CLEAR_WANTED_SCORE(PLAYER::PLAYER_ID());
            LAW::_SET_BOUNTY_HUNTER_PURSUIT_CLEARED();
        }

        void ConfigureCombat(int ped, int target, bool ranged, bool melee, bool elite = false)
        {
            PED::SET_PED_KEEP_TASK(ped, true);
            PED::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(ped, true);
            PED::SET_PED_COMBAT_ABILITY(ped, 3);
            PED::SET_PED_COMBAT_MOVEMENT(ped, elite ? 3 : 2);
            PED::SET_PED_COMBAT_RANGE(ped, ranged ? 3 : 1);
            PED::SET_PED_ACCURACY(ped, ranged ? (elite ? 92 : 62) : 58);
            PED::SET_PED_SEEING_RANGE(ped, 260.0f);
            PED::SET_PED_HEARING_RANGE(ped, 260.0f);
            PED::SET_PED_MOVE_RATE_OVERRIDE(ped, elite ? 1.75f : 1.35f);

            for (int attr : {4, 5, 8, 13, 21, 25, 28, 42, 46, 50, 58, 78, 81, 93, 115})
                PED::SET_PED_COMBAT_ATTRIBUTES(ped, attr, true);
            PED::SET_PED_COMBAT_ATTRIBUTES(ped, 17, false);
            PED::SET_PED_COMBAT_ATTRIBUTES(ped, 125, false);
            PED::SET_PED_COMBAT_ATTRIBUTES(ped, 93, melee);
            PED::SET_PED_TO_PLAYER_WEAPON_DAMAGE_MODIFIER(ped, elite ? 1.45f : 1.18f);

            TASK::TASK_COMBAT_PED(ped, target, 0, 16);
        }

        void ConfigureDoppelganger(int ped, int selfHandle)
        {
            ConfigureCombat(ped, selfHandle, true, true, true);
            PED::SET_PED_ACCURACY(ped, 96);
            PED::SET_PED_COMBAT_MOVEMENT(ped, 3);
            PED::SET_PED_COMBAT_RANGE(ped, 1);
            PED::SET_PED_TO_PLAYER_WEAPON_DAMAGE_MODIFIER(ped, 1.65f);
            ENTITY::SET_ENTITY_MAX_HEALTH(ped, 600);
            ENTITY::SET_ENTITY_HEALTH(ped, 600, 0);

            for (int attr : {20, 24, 31, 39, 41, 49, 56, 63, 68, 80, 81, 92})
                PED::SET_PED_COMBAT_ATTRIBUTES(ped, attr, true);
        }

        void RestoreAmbientPed(int ped)
        {
            if (!ENTITY::DOES_ENTITY_EXIST(ped) || ENTITY::IS_ENTITY_DEAD(ped))
                return;

            TASK::CLEAR_PED_TASKS(ped, true, false);
            PED::SET_PED_KEEP_TASK(ped, false);
            PED::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(ped, false);
            PED::SET_PED_MOVE_RATE_OVERRIDE(ped, 1.0f);
            PED::SET_PED_TO_PLAYER_WEAPON_DAMAGE_MODIFIER(ped, 1.0f);
            for (int attr : {4, 5, 8, 13, 21, 25, 28, 42, 46, 50, 58, 78, 81, 93, 115})
                PED::SET_PED_COMBAT_ATTRIBUTES(ped, attr, false);
        }

        void GiveWeapon(int ped, const char* weaponName, int ammo = 32)
        {
            const Hash weapon = Joaat(weaponName);
            WEAPON::GIVE_WEAPON_TO_PED(
                ped, weapon, ammo, true, true, 0, false,
                0.5f, 1.0f, 1.0f, false, 0, false);
            WEAPON::SET_CURRENT_PED_WEAPON(ped, weapon, true, 0, false, false);
        }

        void GiveFists(int ped)
        {
            WEAPON::REMOVE_ALL_PED_WEAPONS(ped, true, true);
            WEAPON::SET_CURRENT_PED_WEAPON(ped, Joaat("WEAPON_UNARMED"), true, 0, false, false);
        }

        void GiveRandomMelee(int ped)
        {
            static constexpr std::array<const char*, 26> weapons{
                "WEAPON_MELEE_ANCIENT_HATCHET",
                "WEAPON_MELEE_BROKEN_SWORD",
                "WEAPON_MELEE_CLEAVER",
                "WEAPON_MELEE_HATCHET",
                "WEAPON_MELEE_HATCHET_DOUBLE_BIT",
                "WEAPON_MELEE_HATCHET_DOUBLE_BIT_RUSTED",
                "WEAPON_MELEE_HATCHET_HEWING",
                "WEAPON_MELEE_HATCHET_HUNTER",
                "WEAPON_MELEE_HATCHET_HUNTER_RUSTED",
                "WEAPON_MELEE_HATCHET_MELEEONLY",
                "WEAPON_MELEE_HATCHET_VIKING",
                "WEAPON_MELEE_KNIFE",
                "WEAPON_MELEE_KNIFE_BEAR",
                "WEAPON_MELEE_KNIFE_CIVIL_WAR",
                "WEAPON_MELEE_KNIFE_HORROR",
                "WEAPON_MELEE_KNIFE_JAWBONE",
                "WEAPON_MELEE_KNIFE_JOHN",
                "WEAPON_MELEE_KNIFE_MINER",
                "WEAPON_MELEE_KNIFE_RUSTIC",
                "WEAPON_MELEE_KNIFE_TRADER",
                "WEAPON_MELEE_KNIFE_VAMPIRE",
                "WEAPON_MELEE_MACHETE",
                "WEAPON_MELEE_MACHETE_COLLECTOR",
                "WEAPON_MELEE_MACHETE_HORROR",
                "WEAPON_THROWN_TOMAHAWK_ANCIENT",
                "WEAPON_THROWN_TOMAHAWK_MELEEONLY"};
            GiveWeapon(ped, weapons[RandomInt(0, static_cast<int>(weapons.size()) - 1)], 8);
        }

        void ArmForRegion(int ped, Region region, bool ambient)
        {
            const int roll = RandomInt(1, 100);

            switch (region)
            {
            case Region::Armadillo:
                if (roll <= 72)
                    GiveWeapon(ped, "WEAPON_MELEE_KNIFE", 1);
                else if (roll <= 90)
                    GiveRandomMelee(ped);
                else
                    GiveFists(ped);
                break;

            case Region::SaintDenis:
            case Region::Valentine:
            case Region::Strawberry:
                if (roll <= 36)
                    GiveWeapon(ped, "WEAPON_MELEE_MACHETE", 1);
                else if (roll <= 68)
                    GiveFists(ped);
                else if (roll <= 86)
                    GiveWeapon(ped, "WEAPON_THROWN_MOLOTOV", 8);
                else
                    GiveRandomMelee(ped);
                break;

            case Region::Heartlands:
            case Region::Snow:
                if (roll <= 34)
                {
                    GiveWeapon(ped, "WEAPON_BOW", 40);
                    PED::SET_PED_ACCURACY(ped, 28);
                }
                else if (roll <= 60)
                    GiveWeapon(ped, "WEAPON_MELEE_ANCIENT_HATCHET", 1);
                else if (roll <= 74)
                    GiveWeapon(ped, "WEAPON_THROWN_MOLOTOV", 8);
                else
                    GiveFists(ped);
                break;

            case Region::Lagras:
                if (roll <= 18)
                    GiveWeapon(ped, "WEAPON_MELEE_TORCH", 1);
                else if (roll <= 72)
                    GiveRandomMelee(ped);
                else
                    GiveFists(ped);
                break;

            default:
                if (roll <= 65)
                    GiveRandomMelee(ped);
                else
                    GiveFists(ped);
                break;
            }

            if (ambient)
                PED::SET_PED_MOVE_RATE_OVERRIDE(ped, 1.35f);
        }

        float HeadingTo(const Vector3& from, const Vector3& to)
        {
            return std::atan2(to.x - from.x, to.y - from.y) * kRadToDeg;
        }

        int CountRole(Role role, Region area)
        {
            return static_cast<int>(std::count_if(g_Actors.begin(), g_Actors.end(), [role, area](const Actor& actor) {
                return actor.Type == role && actor.Area == area && ENTITY::DOES_ENTITY_EXIST(actor.Ped) && !ENTITY::IS_ENTITY_DEAD(actor.Ped);
            }));
        }

        void AddActor(int ped, Role role, Region area, bool dormant = false)
        {
            const auto now = Clock::now();
            g_Actors.push_back({
                ped,
                role,
                area,
                ENTITY::GET_ENTITY_COORDS(ped, true, false),
                now,
                dormant ? AfterMs(3500, 7000) : now,
                AfterMs(7000, 14000),
                AfterMs(8000, 15000),
                false,
                dormant});
        }

        void PlayWarCry(int ped)
        {
            const Hash emote = RandomInt(0, 1)
                ? Joaat("KIT_EMOTE_TAUNT_WAR_CRY_1")
                : Joaat("KIT_EMOTE_REACTION_FRIGHTENED_1");
            TASK::TASK_PLAY_EMOTE_WITH_HASH(ped, 2, 0, emote, true, true, false, false, false);
        }

        int NearestHostile(const Vector3& pos, float maxDistance)
        {
            int best{};
            float bestDistance = maxDistance;

            for (const auto& actor : g_Actors)
            {
                if (!actor.Ped || !ENTITY::DOES_ENTITY_EXIST(actor.Ped) || ENTITY::IS_ENTITY_DEAD(actor.Ped))
                    continue;
                const float distance = Distance(pos, ENTITY::GET_ENTITY_COORDS(actor.Ped, true, false));
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    best = actor.Ped;
                }
            }

            for (const auto& [ped, state] : g_Ambient)
            {
                (void)state;
                if (!ENTITY::DOES_ENTITY_EXIST(ped) || ENTITY::IS_ENTITY_DEAD(ped))
                    continue;
                const float distance = Distance(pos, ENTITY::GET_ENTITY_COORDS(ped, true, false));
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    best = ped;
                }
            }
            return best;
        }

        int NearestLaw(const Vector3& pos, float maxDistance)
        {
            int best{};
            float bestDistance = maxDistance;
            for (int ped : g_LawAllies)
            {
                if (!ped || !ENTITY::DOES_ENTITY_EXIST(ped) || ENTITY::IS_ENTITY_DEAD(ped))
                    continue;
                const float distance = Distance(pos, ENTITY::GET_ENTITY_COORDS(ped, true, false));
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    best = ped;
                }
            }
            return best;
        }

        void RecruitLaw(int ped)
        {
            if (!ped || IsLawAlly(ped))
                return;
            g_LawAllies.push_back(ped);
            PED::SET_PED_KEEP_TASK(ped, true);
            PED::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(ped, true);
            PED::SET_PED_COMBAT_ABILITY(ped, 3);
            PED::SET_PED_ACCURACY(ped, 76);
            PED::SET_PED_COMBAT_MOVEMENT(ped, 2);
        }

        void SpawnLawAllies(int selfHandle, const Vector3& selfPos, const Clock::time_point now)
        {
            g_LawAllies.erase(std::remove_if(g_LawAllies.begin(), g_LawAllies.end(), [](int ped) {
                return !ENTITY::DOES_ENTITY_EXIST(ped) || ENTITY::IS_ENTITY_DEAD(ped);
            }), g_LawAllies.end());

            if (g_LawAllies.size() >= 2 || now < g_NextLawSpawn)
                return;
            g_NextLawSpawn = AfterMs(5000, 8000);

            Vector3 pos{};
            if (!FindSpawn(selfPos, 42.0f, 64.0f, true, pos))
                return;

            const int law = CreateLocalPed("S_M_M_AmbientBlWPolice_01", pos, HeadingTo(pos, selfPos));
            if (!law)
                return;
            GiveWeapon(law, "WEAPON_REVOLVER_CATTLEMAN", 80);
            RecruitLaw(law);
            (void)selfHandle;
        }

        void TickLaw(int selfHandle, const Vector3& selfPos, const Clock::time_point now)
        {
            if (now < g_NextLawTick)
                return;
            g_NextLawTick = now + std::chrono::milliseconds(550);

            SpawnLawAllies(selfHandle, selfPos, now);
            for (int law : g_LawAllies)
            {
                if (!ENTITY::DOES_ENTITY_EXIST(law) || ENTITY::IS_ENTITY_DEAD(law))
                    continue;
                const Vector3 pos = ENTITY::GET_ENTITY_COORDS(law, true, false);
                if (const int hostile = NearestHostile(pos, 90.0f))
                    TASK::TASK_COMBAT_PED(law, hostile, 0, 16);
            }
        }

        void TickAmbient(int selfHandle, const Vector3& selfPos, Region region, const Clock::time_point now)
        {
            static Clock::time_point nextScan{};
            if (now < nextScan)
                return;
            nextScan = now + kScanInterval;

            for (auto it = g_Ambient.begin(); it != g_Ambient.end();)
            {
                if (!ENTITY::DOES_ENTITY_EXIST(it->first) || ENTITY::IS_ENTITY_DEAD(it->first))
                    it = g_Ambient.erase(it);
                else
                    ++it;
            }

            int nearby[kMaxNearbyPeds * 2 + 2]{};
            nearby[0] = kMaxNearbyPeds;
            const int count = std::min(PED::GET_PED_NEARBY_PEDS(selfHandle, nearby, -1, 0), kMaxNearbyPeds);

            for (int i = 0; i < count; ++i)
            {
                const int ped = nearby[i * 2 + 2];
                if (!ped || ped == selfHandle || IsOurActor(ped))
                    continue;
                if (!ENTITY::DOES_ENTITY_EXIST(ped) || ENTITY::IS_ENTITY_DEAD(ped))
                    continue;
                if (PED::IS_PED_A_PLAYER(ped))
                    continue;

                if (IsLawPed(ped))
                {
                    RecruitLaw(ped);
                    continue;
                }

                if (!PED::IS_PED_HUMAN(ped))
                    continue;

                const Vector3 pos = ENTITY::GET_ENTITY_COORDS(ped, true, false);
                if (Distance(pos, selfPos) > kAmbientRadius)
                    continue;

                auto [it, inserted] = g_Ambient.try_emplace(ped);
                if (inserted)
                {
                    if (g_Ambient.size() > kMaxAmbientAttackers)
                    {
                        g_Ambient.erase(it);
                        continue;
                    }
                    it->second.LastPos = pos;
                    it->second.LastMove = now;
                    it->second.NextTask = now;
                    it->second.NextPoison = AfterMs(1800, 3200);
                    it->second.Area = region;
                    ArmForRegion(ped, region, true);
                    ConfigureCombat(ped, selfHandle, false, true);
                }

                auto& state = it->second;
                if (Distance(state.LastPos, pos) > 0.75f)
                {
                    state.LastPos = pos;
                    state.LastMove = now;
                }

                int target = selfHandle;
                if (const int law = NearestLaw(pos, 16.0f))
                    target = law;

                const bool stuck = now - state.LastMove > std::chrono::milliseconds(2500) && Distance(pos, ENTITY::GET_ENTITY_COORDS(target, true, false)) > 4.0f;
                if (now >= state.NextTask || stuck || !PED::IS_PED_IN_COMBAT(ped, target))
                {
                    PED::SET_PED_COMBAT_MOVEMENT(ped, 3);
                    PED::SET_PED_MOVE_RATE_OVERRIDE(ped, 1.45f);
                    TASK::TASK_COMBAT_PED(ped, target, 0, 16);
                    state.NextTask = now + kRetaskInterval;
                    if (stuck)
                    {
                        state.LastMove = now;
                        state.LastPos = pos;
                    }
                }

                if (state.Area == Region::Armadillo && target == selfHandle && Distance(pos, selfPos) < 2.4f && now >= state.NextPoison)
                {
                    const int health = ENTITY::GET_ENTITY_HEALTH(selfHandle);
                    if (health > 12)
                        ENTITY::SET_ENTITY_HEALTH(selfHandle, std::max(1, health - RandomInt(2, 5)), ped);
                    state.NextPoison = AfterMs(2600, 4200);
                }
            }
        }

        void StrikeLightning(int selfHandle, const Vector3& selfPos)
        {
            ForceLightningAtCoords(selfPos.x, selfPos.y, selfPos.z, 1.0f);

            const int health = ENTITY::GET_ENTITY_HEALTH(selfHandle);
            if (health > 15)
                ENTITY::SET_ENTITY_HEALTH(selfHandle, std::max(1, health - RandomInt(5, 9)), 0);

            for (auto& actor : g_Actors)
            {
                if (!actor.Ped || !ENTITY::DOES_ENTITY_EXIST(actor.Ped) || ENTITY::IS_ENTITY_DEAD(actor.Ped))
                    continue;
                const Vector3 pos = ENTITY::GET_ENTITY_COORDS(actor.Ped, true, false);
                if (Distance(pos, selfPos) <= 8.0f)
                    ENTITY::SET_ENTITY_HEALTH(actor.Ped, 0, 0);
            }

            for (const auto& [ped, state] : g_Ambient)
            {
                (void)state;
                if (!ENTITY::DOES_ENTITY_EXIST(ped) || ENTITY::IS_ENTITY_DEAD(ped))
                    continue;
                const Vector3 pos = ENTITY::GET_ENTITY_COORDS(ped, true, false);
                if (Distance(pos, selfPos) <= 8.0f)
                    ENTITY::SET_ENTITY_HEALTH(ped, 0, 0);
            }
        }

        void TickActors(int selfHandle, const Vector3& selfPos, const Clock::time_point now)
        {
            if (now < g_NextActorTick)
                return;
            g_NextActorTick = now + std::chrono::milliseconds(260);

            for (auto it = g_Actors.begin(); it != g_Actors.end();)
            {
                if (!ENTITY::DOES_ENTITY_EXIST(it->Ped) || ENTITY::IS_ENTITY_DEAD(it->Ped))
                {
                    it = g_Actors.erase(it);
                    continue;
                }

                Actor& actor = *it;
                const Vector3 pos = ENTITY::GET_ENTITY_COORDS(actor.Ped, true, false);
                if (Distance(actor.LastPos, pos) > 0.75f)
                {
                    actor.LastPos = pos;
                    actor.LastMove = now;
                }

                if (actor.Dormant)
                {
                    if (now < actor.NextTask)
                    {
                        ++it;
                        continue;
                    }
                    actor.Dormant = false;
                    ConfigureCombat(actor.Ped, selfHandle, actor.Type == Role::NativePatrol, true, actor.Type == Role::Doppelganger);
                }

                int target = selfHandle;
                if (actor.Type != Role::Alligator)
                {
                    if (const int law = NearestLaw(pos, 18.0f))
                        target = law;
                }

                const bool stuck = now - actor.LastMove > std::chrono::milliseconds(2600) && Distance(pos, ENTITY::GET_ENTITY_COORDS(target, true, false)) > 4.0f;
                if (now >= actor.NextTask || stuck || !PED::IS_PED_IN_COMBAT(actor.Ped, target))
                {
                    PED::SET_PED_COMBAT_MOVEMENT(actor.Ped, 3);
                    PED::SET_PED_MOVE_RATE_OVERRIDE(actor.Ped, actor.Type == Role::Doppelganger ? 1.8f : 1.45f);
                    TASK::TASK_COMBAT_PED(actor.Ped, target, 0, 16);
                    actor.NextTask = now + kRetaskInterval;
                    if (stuck)
                    {
                        actor.LastMove = now;
                        actor.LastPos = pos;
                    }
                }

                if (actor.Type == Role::Torch)
                {
                    if (now >= actor.NextVoice)
                    {
                        PlayWarCry(actor.Ped);
                        actor.NextVoice = AfterMs(5000, 9000);
                    }

                    if (!actor.LightningUsed && target == selfHandle && Distance(pos, selfPos) < 24.0f && now >= actor.NextStrike)
                    {
                        StrikeLightning(selfHandle, selfPos);
                        actor.LightningUsed = true;
                    }
                }
                else if (actor.Type != Role::Alligator && now >= actor.NextVoice)
                {
                    if (RandomInt(1, 100) <= 20)
                        PlayWarCry(actor.Ped);
                    actor.NextVoice = AfterMs(9000, 18000);
                }

                ++it;
            }
        }

        const char* RegionalModel(Region region)
        {
            switch (region)
            {
            case Region::Lagras:
                return "G_M_M_UniInbred_01";
            case Region::Heartlands:
            case Region::Snow:
            {
                static constexpr std::array<const char*, 3> natives{
                    "A_M_M_WapWarriors_01",
                    "U_M_M_NbxIndianOwner_01",
                    "A_F_M_WapNative_01"};
                return natives[RandomInt(0, static_cast<int>(natives.size()) - 1)];
            }
            default:
            {
                static constexpr std::array<const char*, 4> civilians{
                    "A_M_M_ValTownfolk_01",
                    "A_M_M_SdTownfolk_02",
                    "A_M_M_StrTownfolk_01",
                    "A_M_M_ArmadilloTownfolk_01"};
                return civilians[RandomInt(0, static_cast<int>(civilians.size()) - 1)];
            }
            }
        }

        void SpawnRegional(int selfHandle, const Vector3& selfPos, Region region, const Clock::time_point now)
        {
            if (now < g_NextSpecial || g_Actors.size() >= kMaxSpecialActors)
                return;

            g_NextSpecial = AfterMs(region == Region::Heartlands || region == Region::Snow ? 650 : 850,
                                    region == Region::Heartlands || region == Region::Snow ? 1100 : 1450);

            Vector3 pos{};
            const bool patrol = region == Region::Heartlands || region == Region::Snow;
            if (!FindSpawn(selfPos, patrol ? 95.0f : 55.0f, patrol ? 155.0f : 96.0f, true, pos))
                return;

            if (region == Region::Lagras && CountRole(Role::Alligator, region) < 4 && RandomInt(1, 100) <= 16)
            {
                const int gator = CreateLocalPed("A_C_Alligator_01", pos, HeadingTo(pos, selfPos));
                if (!gator)
                    return;
                PED::SET_PED_MOVE_RATE_OVERRIDE(gator, 1.25f);
                TASK::TASK_COMBAT_PED(gator, selfHandle, 0, 16);
                AddActor(gator, Role::Alligator, region);
                return;
            }

            const bool torch = CountRole(Role::Torch, region) < 1 && RandomInt(1, 100) <= (region == Region::Lagras ? 8 : 3);
            const int ped = CreateLocalPed(RegionalModel(region), pos, HeadingTo(pos, selfPos));
            if (!ped)
                return;

            if (torch)
            {
                GiveWeapon(ped, "WEAPON_MELEE_TORCH", 1);
                ConfigureCombat(ped, selfHandle, false, true);
                AddActor(ped, Role::Torch, region);
                PlayWarCry(ped);
                return;
            }

            ArmForRegion(ped, region, false);
            if (patrol)
            {
                TASK::TASK_WANDER_STANDARD(ped, 10.0f, 10);
                AddActor(ped, Role::NativePatrol, region, true);
            }
            else
            {
                ConfigureCombat(ped, selfHandle, false, true);
                AddActor(ped, Role::Regional, region);
            }
        }

        void SpawnDoppelganger(int selfHandle, const Vector3& selfPos, Region region, const Clock::time_point now)
        {
            if (CountRole(Role::Doppelganger, region) >= kDoppelgangersPerRegion || now < g_NextDoppel)
                return;
            g_NextDoppel = AfterMs(900, 1700);

            Vector3 pos{};
            if (!FindBehindSpawn(selfHandle, selfPos, pos))
                return;

            int clone = PED::CLONE_PED(selfHandle, false, false, true);
            if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone))
                return;

            ENTITY::SET_ENTITY_AS_MISSION_ENTITY(clone, true, true);
            ENTITY::SET_ENTITY_COORDS_NO_OFFSET(clone, pos.x, pos.y, pos.z, true, true, true);
            ENTITY::SET_ENTITY_HEADING(clone, HeadingTo(pos, selfPos));
            ENTITY::PLACE_ENTITY_ON_GROUND_PROPERLY(clone, true);

            Hash currentWeapon = Joaat("WEAPON_UNARMED");
            if (WEAPON::GET_CURRENT_PED_WEAPON(selfHandle, &currentWeapon, true, 0, false) && currentWeapon != Joaat("WEAPON_UNARMED"))
            {
                WEAPON::REMOVE_ALL_PED_WEAPONS(clone, true, true);
                WEAPON::GIVE_WEAPON_TO_PED(
                    clone, currentWeapon, 250, true, true, 0, false,
                    0.5f, 1.0f, 1.0f, false, 0, false);
                WEAPON::SET_CURRENT_PED_WEAPON(clone, currentWeapon, true, 0, false, false);
            }
            else
            {
                GiveFists(clone);
            }

            ConfigureDoppelganger(clone, selfHandle);
            AddActor(clone, Role::Doppelganger, region);
        }

        void TickSwampAmbience(Region region, const Clock::time_point now)
        {
            if (region != Region::Lagras || now < g_NextSwampVoice)
                return;

            g_NextSwampVoice = AfterMs(9000, 17000);
            std::vector<int> murfree;
            for (const auto& actor : g_Actors)
            {
                if (actor.Area == Region::Lagras && actor.Ped && ENTITY::DOES_ENTITY_EXIST(actor.Ped) && !ENTITY::IS_ENTITY_DEAD(actor.Ped) && actor.Type != Role::Alligator)
                    murfree.push_back(actor.Ped);
            }

            if (!murfree.empty())
            {
                const int ped = murfree[RandomInt(0, static_cast<int>(murfree.size()) - 1)];
                TASK::TASK_PLAY_EMOTE_WITH_HASH(
                    ped, 0, 2,
                    RandomInt(0, 1) ? Joaat("KIT_EMOTE_REACTION_SCARED_1") : Joaat("KIT_EMOTE_REACTION_DISAGREE_1"),
                    false, false, false, false, false);
            }
        }

        void Cleanup()
        {
            for (const auto& [ped, state] : g_Ambient)
            {
                (void)state;
                RestoreAmbientPed(ped);
            }
            g_Ambient.clear();

            for (auto& actor : g_Actors)
            {
                if (actor.Ped && ENTITY::DOES_ENTITY_EXIST(actor.Ped) && !ENTITY::IS_ENTITY_DEAD(actor.Ped))
                {
                    int ped = actor.Ped;
                    PED::DELETE_PED(&ped);
                }
            }
            g_Actors.clear();

            for (int law : g_LawAllies)
            {
                if (law && ENTITY::DOES_ENTITY_EXIST(law) && !ENTITY::IS_ENTITY_DEAD(law))
                {
                    int ped = law;
                    PED::DELETE_PED(&ped);
                }
            }
            g_LawAllies.clear();

            g_NextSpecial = {};
            g_NextDoppel = {};
            g_NextLawSpawn = {};
            g_NextActorTick = {};
            g_NextLawTick = {};
            g_NextLawClear = {};
            g_NextSwampVoice = {};
            g_LastRegion = Region::Default;
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
            const Region region = GetRegion(selfPos);

            if (region != g_LastRegion)
            {
                g_LastRegion = region;
                g_NextDoppel = now;
                g_NextSpecial = now;
            }

            if (now >= g_NextLawClear)
            {
                ClearLaw();
                g_NextLawClear = now + std::chrono::milliseconds(750);
            }

            TickAmbient(selfHandle, selfPos, region, now);
            TickActors(selfHandle, selfPos, now);
            TickLaw(selfHandle, selfPos, now);
            TickSwampAmbience(region, now);
            SpawnDoppelganger(selfHandle, selfPos, region, now);
            SpawnRegional(selfHandle, selfPos, region, now);
        }

        void OnDisable() override
        {
            Cleanup();
            ClearLaw();
        }
    };

    static PlayerMustDie _PlayerMustDie{
        "nearbyhostilepeds",
        "Player Must Die",
        "Turns the local world against your character with regional attackers, law allies, torch lightning and elite doppelgangers."};
}
