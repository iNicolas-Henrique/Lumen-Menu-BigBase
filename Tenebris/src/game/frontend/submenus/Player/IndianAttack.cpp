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

        constexpr float kAmbientRadius = 180.0f;
        constexpr int kMaxNearbyPeds = 96;
        constexpr std::size_t kMaxAmbientAttackers = 26;
        constexpr std::size_t kMaxSpecialActors = 12;
        constexpr int kMaxLasso = 3;
        constexpr int kMaxMurfree = 5;
        constexpr int kMaxNightFolk = 4;
        constexpr auto kScanInterval = std::chrono::milliseconds(650);
        constexpr auto kRetaskInterval = std::chrono::milliseconds(2200);
        constexpr float kRadToDeg = 57.29577951308232f;

        enum class Role
        {
            Murfree,
            Lasso,
            Torch,
            PoisonArcher,
            Invoker,
            NightFolk
        };

        struct AmbientState
        {
            Vector3 LastPos{};
            Clock::time_point LastMove{};
            Clock::time_point NextTask{};
        };

        struct Actor
        {
            int Ped{};
            int Mount{};
            Role Type{Role::Murfree};
            Vector3 LastPos{};
            Clock::time_point LastMove{};
            Clock::time_point NextTask{};
            Clock::time_point NextVoice{};
        };

        std::unordered_map<int, AmbientState> g_Ambient;
        std::vector<Actor> g_Actors;
        int g_Observer{};
        Clock::time_point g_NextSpecial{};
        Clock::time_point g_NextSummon{};
        Clock::time_point g_NextObserverLaugh{};
        Clock::time_point g_NextActorTick{};
        Clock::time_point g_NextObserverTick{};
        Clock::time_point g_NextLawClear{};
        Vector3 g_StartPos{};
        bool g_HasStartPos{};

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
            for (int i = 0; i < 24; ++i)
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
                if (hideFromCamera && CAM::IS_SPHERE_VISIBLE(pos.x, pos.y, pos.z + 1.0f, 5.0f))
                    continue;

                out = pos;
                return true;
            }
            return false;
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

        bool IsOurActor(int ped)
        {
            if (ped == g_Observer)
                return true;
            return std::any_of(g_Actors.begin(), g_Actors.end(), [ped](const Actor& actor) {
                return actor.Ped == ped || actor.Mount == ped;
            });
        }

        void ClearLaw()
        {
            LAW::CLEAR_BOUNTY(PLAYER::PLAYER_ID());
            LAW::CLEAR_WANTED_SCORE(PLAYER::PLAYER_ID());
            LAW::_SET_BOUNTY_HUNTER_PURSUIT_CLEARED();
        }

        void ConfigureCombat(int ped, int target, bool ranged, bool melee)
        {
            PED::SET_PED_KEEP_TASK(ped, true);
            PED::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(ped, true);
            PED::SET_PED_COMBAT_ABILITY(ped, 3);
            PED::SET_PED_COMBAT_MOVEMENT(ped, 2);
            PED::SET_PED_COMBAT_RANGE(ped, ranged ? 3 : 1);
            PED::SET_PED_ACCURACY(ped, ranged ? 70 : 58);
            PED::SET_PED_SEEING_RANGE(ped, 220.0f);
            PED::SET_PED_HEARING_RANGE(ped, 220.0f);

            for (int attr : {4, 5, 8, 21, 25, 28, 42, 46, 50, 58, 78, 81, 115})
                PED::SET_PED_COMBAT_ATTRIBUTES(ped, attr, true);
            PED::SET_PED_COMBAT_ATTRIBUTES(ped, 17, false);
            PED::SET_PED_COMBAT_ATTRIBUTES(ped, 125, false);
            PED::SET_PED_COMBAT_ATTRIBUTES(ped, 93, melee);
            PED::SET_PED_TO_PLAYER_WEAPON_DAMAGE_MODIFIER(ped, 1.15f);

            TASK::TASK_COMBAT_PED(ped, target, 0, 16);
        }

        void RestoreAmbientPed(int ped)
        {
            if (!ENTITY::DOES_ENTITY_EXIST(ped))
                return;

            TASK::CLEAR_PED_TASKS(ped, true, false);
            PED::SET_PED_KEEP_TASK(ped, false);
            PED::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(ped, false);
            PED::SET_PED_TO_PLAYER_WEAPON_DAMAGE_MODIFIER(ped, 1.0f);
            for (int attr : {4, 5, 8, 21, 25, 28, 42, 46, 50, 58, 78, 81, 93, 115})
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

        void GivePoisonBow(int ped)
        {
            const Hash bow = Joaat("WEAPON_BOW");
            const Hash poison = Joaat("AMMO_ARROW_POISON");
            WEAPON::GIVE_WEAPON_TO_PED(
                ped, bow, 24, true, true, 0, false,
                0.5f, 1.0f, 1.0f, false, 0, false);
            WEAPON::SET_PED_AMMO_BY_TYPE(ped, poison, 24);
            WEAPON::_SET_AMMO_TYPE_FOR_PED_WEAPON(ped, bow, poison);
            WEAPON::SET_CURRENT_PED_WEAPON(ped, bow, true, 0, false, false);
            PED::SET_PED_ACCURACY(ped, 70);
        }

        float HeadingTo(const Vector3& from, const Vector3& to)
        {
            return std::atan2(to.x - from.x, to.y - from.y) * kRadToDeg;
        }

        int CountRole(Role role)
        {
            return static_cast<int>(std::count_if(g_Actors.begin(), g_Actors.end(), [role](const Actor& actor) {
                return actor.Type == role && ENTITY::DOES_ENTITY_EXIST(actor.Ped) && !ENTITY::IS_ENTITY_DEAD(actor.Ped);
            }));
        }

        void AddActor(int ped, int mount, Role role)
        {
            const auto now = Clock::now();
            g_Actors.push_back({
                ped,
                mount,
                role,
                ENTITY::GET_ENTITY_COORDS(ped, true, false),
                now,
                now,
                AfterMs(7000, 14000)});
        }

        int SpawnMounted(const char* riderModel, const char* mountModel, const Vector3& pos, int target, Role role)
        {
            const Vector3 targetPos = ENTITY::GET_ENTITY_COORDS(target, true, false);
            const float heading = HeadingTo(pos, targetPos);
            const int mount = CreateLocalPed(mountModel, pos, heading);
            if (!mount)
                return 0;

            const int rider = CreateLocalPed(riderModel, pos, heading);
            if (!rider)
            {
                int handle = mount;
                PED::DELETE_PED(&handle);
                return 0;
            }

            PED::SET_PED_ONTO_MOUNT(rider, mount, -1, true);
            AddActor(rider, mount, role);
            return rider;
        }

        void PlayWarCry(int ped)
        {
            TASK::TASK_PLAY_EMOTE_WITH_HASH(
                ped, 2, 0, Joaat("KIT_EMOTE_TAUNT_WAR_CRY_1"),
                true, true, false, false, false);
        }

        void PlayObserverLaugh(int ped)
        {
            const Hash emote = RandomInt(0, 1)
                ? Joaat("KIT_EMOTE_REACTION_POINTLAUGH_1")
                : Joaat("KIT_EMOTE_REACTION_JOVIAL_LAUGH_1");
            TASK::TASK_PLAY_EMOTE_WITH_HASH(
                ped, 0, 2, emote,
                false, false, false, false, false);
        }

        void TickAmbient(int selfHandle, const Vector3& selfPos, const Clock::time_point now)
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
                if (!PED::IS_PED_HUMAN(ped) || PED::IS_PED_A_PLAYER(ped) || IsLawPed(ped))
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
                    ConfigureCombat(ped, selfHandle, false, true);
                }

                auto& state = it->second;
                if (Distance(state.LastPos, pos) > 0.8f)
                {
                    state.LastPos = pos;
                    state.LastMove = now;
                }

                const bool stuck = now - state.LastMove > std::chrono::milliseconds(2800) &&
                                   Distance(pos, selfPos) > 4.5f;
                if (now >= state.NextTask || stuck || !PED::IS_PED_IN_COMBAT(ped, selfHandle))
                {
                    PED::SET_PED_COMBAT_MOVEMENT(ped, stuck ? 3 : 2);
                    TASK::TASK_COMBAT_PED(ped, selfHandle, 0, 16);
                    state.NextTask = now + kRetaskInterval;
                    if (stuck)
                    {
                        state.LastMove = now;
                        state.LastPos = pos;
                    }
                }
            }
        }

        void TickActors(int selfHandle, const Vector3& selfPos, const Clock::time_point now)
        {
            if (now < g_NextActorTick)
                return;
            g_NextActorTick = now + std::chrono::milliseconds(300);

            for (auto it = g_Actors.begin(); it != g_Actors.end();)
            {
                if (!ENTITY::DOES_ENTITY_EXIST(it->Ped) || ENTITY::IS_ENTITY_DEAD(it->Ped))
                {
                    if (it->Ped && ENTITY::DOES_ENTITY_EXIST(it->Ped))
                    {
                        int ped = it->Ped;
                        PED::DELETE_PED(&ped);
                    }
                    if (it->Mount && ENTITY::DOES_ENTITY_EXIST(it->Mount))
                    {
                        int mount = it->Mount;
                        PED::DELETE_PED(&mount);
                    }
                    it = g_Actors.erase(it);
                    continue;
                }

                Actor& actor = *it;
                const Vector3 pos = ENTITY::GET_ENTITY_COORDS(actor.Ped, true, false);
                if (Distance(actor.LastPos, pos) > 0.8f)
                {
                    actor.LastPos = pos;
                    actor.LastMove = now;
                }

                if (actor.Type == Role::Invoker)
                {
                    if (now >= actor.NextTask)
                    {
                        TASK::TASK_LOOK_AT_ENTITY(actor.Ped, selfHandle, 5000, 0, 51, 0);
                        TASK::TASK_PLAY_EMOTE_WITH_HASH(
                            actor.Ped, 1, 0, Joaat(RandomInt(0, 1) ? "KIT_EMOTE_ACTION_SPOOKY_1" : "KIT_EMOTE_ACTION_PRAYER_1"),
                            true, true, false, false, false);
                        actor.NextTask = AfterMs(7000, 11000);
                    }
                    ++it;
                    continue;
                }

                const bool stuck = now - actor.LastMove > std::chrono::milliseconds(3000) &&
                                   Distance(pos, selfPos) > 5.0f;
                if (now >= actor.NextTask || stuck || !PED::IS_PED_IN_COMBAT(actor.Ped, selfHandle))
                {
                    PED::SET_PED_COMBAT_MOVEMENT(actor.Ped, stuck ? 3 : 2);
                    TASK::TASK_COMBAT_PED(actor.Ped, selfHandle, 0, 16);
                    actor.NextTask = now + kRetaskInterval;
                    if (stuck)
                    {
                        actor.LastMove = now;
                        actor.LastPos = pos;
                    }
                }

                if (now >= actor.NextVoice)
                {
                    if (RandomInt(1, 100) <= 28)
                        PlayWarCry(actor.Ped);
                    actor.NextVoice = AfterMs(8000, 16000);
                }
                ++it;
            }
        }

        void SpawnNightFolk(int selfHandle)
        {
            if (CountRole(Role::NightFolk) >= kMaxNightFolk)
                return;

            const Vector3 selfPos = ENTITY::GET_ENTITY_COORDS(selfHandle, true, false);
            Vector3 pos{};
            if (!FindSpawn(selfPos, 55.0f, 80.0f, true, pos))
                return;

            const int ped = CreateLocalPed("G_M_M_UNISWAMP_01", pos, HeadingTo(pos, selfPos));
            if (!ped)
                return;

            if (RandomInt(1, 100) <= 30)
                GiveWeapon(ped, "WEAPON_MELEE_TORCH", 1);
            else if (RandomInt(0, 1))
                GiveWeapon(ped, "WEAPON_MELEE_KNIFE", 1);

            ConfigureCombat(ped, selfHandle, false, true);
            AddActor(ped, 0, Role::NightFolk);
        }

        void TickInvoker(int selfHandle, const Clock::time_point now)
        {
            if (now < g_NextSummon || CountRole(Role::Invoker) == 0)
                return;

            SpawnNightFolk(selfHandle);
            g_NextSummon = AfterMs(9000, 13000);
        }

        void SpawnObserver(int selfHandle)
        {
            if (g_Observer && ENTITY::DOES_ENTITY_EXIST(g_Observer))
                return;

            const Vector3 selfPos = ENTITY::GET_ENTITY_COORDS(selfHandle, true, false);
            Vector3 pos{};
            if (!FindSpawn(selfPos, 42.0f, 58.0f, true, pos))
                return;

            g_Observer = CreateLocalPed("S_M_M_AmbientBlWPolice_01", pos, HeadingTo(pos, selfPos));
            if (!g_Observer)
                return;

            PED::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(g_Observer, true);
            PED::SET_PED_KEEP_TASK(g_Observer, true);
            TASK::TASK_STAND_STILL(g_Observer, -1);
            TASK::TASK_LOOK_AT_ENTITY(g_Observer, selfHandle, -1, 0, 51, 0);
            g_NextObserverLaugh = AfterMs(6500, 10000);
        }

        void TickObserver(int selfHandle, const Clock::time_point now)
        {
            if (now < g_NextObserverTick)
                return;
            g_NextObserverTick = now + std::chrono::milliseconds(500);

            if (!g_Observer || !ENTITY::DOES_ENTITY_EXIST(g_Observer) || ENTITY::IS_ENTITY_DEAD(g_Observer))
            {
                g_Observer = 0;
                SpawnObserver(selfHandle);
                return;
            }

            if (now >= g_NextObserverLaugh)
            {
                PlayObserverLaugh(g_Observer);
                g_NextObserverLaugh = AfterMs(12000, 20000);
            }
        }

        void SpawnSpecial(int selfHandle, const Vector3& selfPos, const Clock::time_point now)
        {
            if (now < g_NextSpecial || g_Actors.size() >= kMaxSpecialActors)
                return;

            g_NextSpecial = AfterMs(3800, 5800);

            Vector3 pos{};
            if (!FindSpawn(selfPos, 52.0f, 88.0f, true, pos))
                return;

            const bool fled = g_HasStartPos && Distance(selfPos, g_StartPos) > 45.0f;
            const int roll = RandomInt(1, 100);

            if (CountRole(Role::Lasso) < kMaxLasso && roll <= 35)
            {
                const int ped = CreateLocalPed("G_M_M_UniInbred_01", pos, HeadingTo(pos, selfPos));
                if (!ped)
                    return;
                GiveWeapon(ped, "WEAPON_LASSO", 1);
                ConfigureCombat(ped, selfHandle, false, false);
                AddActor(ped, 0, Role::Lasso);
                return;
            }

            if (CountRole(Role::PoisonArcher) == 0 && roll <= 46)
            {
                const int rider = SpawnMounted("G_M_M_UniInbred_01", "A_C_Donkey_01", pos, selfHandle, Role::PoisonArcher);
                if (!rider)
                    return;
                GivePoisonBow(rider);
                ConfigureCombat(rider, selfHandle, true, false);
                return;
            }

            if (CountRole(Role::Invoker) == 0 && roll <= 54)
            {
                const int ped = CreateLocalPed("MP_G_M_M_Cultmembers_01", pos, HeadingTo(pos, selfPos));
                if (!ped)
                    return;
                PED::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(ped, true);
                PED::SET_PED_KEEP_TASK(ped, true);
                TASK::TASK_LOOK_AT_ENTITY(ped, selfHandle, -1, 0, 51, 0);
                AddActor(ped, 0, Role::Invoker);
                g_NextSummon = AfterMs(2500, 4500);
                return;
            }

            if (CountRole(Role::Torch) < 2 && roll <= 62)
            {
                const int ped = CreateLocalPed("G_M_M_UniInbred_01", pos, HeadingTo(pos, selfPos));
                if (!ped)
                    return;
                GiveWeapon(ped, "WEAPON_MELEE_TORCH", 1);
                ConfigureCombat(ped, selfHandle, false, true);
                AddActor(ped, 0, Role::Torch);
                return;
            }

            if (CountRole(Role::Murfree) < kMaxMurfree || fled)
            {
                int ped{};
                if (fled && RandomInt(1, 100) <= 55)
                {
                    const char* horse = RandomInt(0, 1)
                        ? "A_C_Horse_MurfreeBrood_Mange_01"
                        : "A_C_Horse_MurfreeBrood_Mange_02";
                    ped = SpawnMounted("G_M_M_UniInbred_01", horse, pos, selfHandle, Role::Murfree);
                }
                else
                {
                    ped = CreateLocalPed("G_M_M_UniInbred_01", pos, HeadingTo(pos, selfPos));
                    if (ped)
                        AddActor(ped, 0, Role::Murfree);
                }

                if (!ped)
                    return;

                const int weaponRoll = RandomInt(1, 100);
                if (weaponRoll <= 18)
                    GiveWeapon(ped, "WEAPON_MELEE_TORCH", 1);
                else if (weaponRoll <= 55)
                    GiveWeapon(ped, "WEAPON_MELEE_MACHETE", 1);
                else if (weaponRoll <= 78)
                    GiveWeapon(ped, "WEAPON_MELEE_KNIFE", 1);
                else
                    WEAPON::REMOVE_ALL_PED_WEAPONS(ped, true, true);

                ConfigureCombat(ped, selfHandle, false, true);
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
                if (actor.Ped && ENTITY::DOES_ENTITY_EXIST(actor.Ped))
                {
                    int ped = actor.Ped;
                    PED::DELETE_PED(&ped);
                }
                if (actor.Mount && ENTITY::DOES_ENTITY_EXIST(actor.Mount))
                {
                    int mount = actor.Mount;
                    PED::DELETE_PED(&mount);
                }
            }
            g_Actors.clear();

            if (g_Observer && ENTITY::DOES_ENTITY_EXIST(g_Observer))
            {
                int observer = g_Observer;
                PED::DELETE_PED(&observer);
            }
            g_Observer = 0;
            g_HasStartPos = false;
            g_NextSpecial = {};
            g_NextSummon = {};
            g_NextObserverLaugh = {};
            g_NextActorTick = {};
            g_NextObserverTick = {};
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

            if (!g_HasStartPos)
            {
                g_StartPos = selfPos;
                g_HasStartPos = true;
                g_NextSpecial = AfterMs(1800, 3000);
            }

            if (now >= g_NextLawClear)
            {
                ClearLaw();
                g_NextLawClear = now + std::chrono::milliseconds(750);
            }

            TickAmbient(selfHandle, selfPos, now);
            TickActors(selfHandle, selfPos, now);
            TickInvoker(selfHandle, now);
            TickObserver(selfHandle, now);
            SpawnSpecial(selfHandle, selfPos, now);
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
        "Turns nearby human NPCs and staggered local reinforcements against your character while keeping law out of the fight."};
}
