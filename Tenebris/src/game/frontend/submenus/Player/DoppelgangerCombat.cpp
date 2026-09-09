#include "core/commands/LoopedCommand.hpp"
#include "game/backend/Self.hpp"
#include "game/rdr/Natives.hpp"
#include "game/rdr/Pools.hpp"

#include <array>
#include <chrono>

namespace YimMenu::Features
{
    class DoppelgangerCombatEngine final : public LoopedCommand
    {
        using LoopedCommand::LoopedCommand;
        using Clock = std::chrono::steady_clock;

        Clock::time_point m_NextTick{};

        static bool IsDoppelganger(int ped, int selfHandle, Hash selfModel)
        {
            if (!ped || ped == selfHandle || !ENTITY::DOES_ENTITY_EXIST(ped) || ENTITY::IS_ENTITY_DEAD(ped))
                return false;
            if (PED::IS_PED_A_PLAYER(ped) || !PED::IS_PED_HUMAN(ped))
                return false;
            if (ENTITY::GET_ENTITY_MODEL(ped) != selfModel)
                return false;

            return ENTITY::GET_ENTITY_MAX_HEALTH(ped, false) == 600;
        }

        static void MakeImplacable(int ped, int selfHandle)
        {
            static constexpr std::array<int, 29> actions{
                0,  // melee
                1,  // grapple
                2,  // attack
                3,  // knockout
                4,  // kick
                5,  // shove
                6,  // choke
                7,  // blocking
                8,  // counter
                10, // dodge
                11, // parry
                12, // lead-in
                13, // disarm
                15, // takedown
                16, // execution
                18, // environmental flourish
                20, // struggle
                21, // escape
                22, // reversal
                23, // breakout
                26, // arm grab
                27, // leg grab
                28, // knockdown
                30, // defensive-area auto grapple
                31, // grapple transition
                32, // auto shove
                33, // tackle
                34, // paired turn attack
                24  // release
            };

            for (int action : actions)
                PED::_CLEAR_PED_ACTION_DISABLE_FLAG(ped, action);

            PED::SET_PED_COMBAT_ABILITY(ped, 3);
            PED::SET_PED_COMBAT_MOVEMENT(ped, 3);
            PED::SET_PED_COMBAT_RANGE(ped, 1);
            PED::SET_PED_MOVE_RATE_OVERRIDE(ped, 1.85f);
            PED::SET_PED_KEEP_TASK(ped, true);
            PED::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(ped, true);

            for (int attr : {4, 5, 8, 13, 21, 25, 28, 31, 39, 41, 42, 46, 49, 50, 56, 58, 63, 68, 78, 80, 81, 92, 93, 115})
                PED::SET_PED_COMBAT_ATTRIBUTES(ped, attr, true);

            PED::SET_PED_COMBAT_ATTRIBUTES(ped, 17, false);
            PED::SET_PED_COMBAT_ATTRIBUTES(ped, 125, false);

            if (!PED::IS_PED_IN_COMBAT(ped, selfHandle))
                TASK::TASK_COMBAT_PED(ped, selfHandle, 0, 16);
        }

        void OnTick() override
        {
            const auto now = Clock::now();
            if (now < m_NextTick)
                return;
            m_NextTick = now + std::chrono::milliseconds(350);

            Ped self = Self::GetPed();
            if (!self.IsValid() || self.GetHealth() <= 0)
                return;

            const int selfHandle = self.GetHandle();
            const Hash selfModel = ENTITY::GET_ENTITY_MODEL(selfHandle);

            for (Ped ped : Pools::GetPeds())
            {
                if (!ped.IsValid())
                    continue;

                const int handle = ped.GetHandle();
                if (IsDoppelganger(handle, selfHandle, selfModel))
                    MakeImplacable(handle, selfHandle);
            }
        }

    public:
        DoppelgangerCombatEngine() :
            LoopedCommand("doppelgangercombatengine", "Doppelganger Combat Engine", "Keeps Player Must Die doppelgangers on the full close-combat action set.")
        {
            m_State = true;
        }
    };

    static DoppelgangerCombatEngine _DoppelgangerCombatEngine;
}
