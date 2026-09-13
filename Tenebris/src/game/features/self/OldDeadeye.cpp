#include "core/commands/LoopedCommand.hpp"
#include "game/backend/Self.hpp"
#include "game/rdr/Enums.hpp"
#include "game/rdr/Natives.hpp"

namespace YimMenu::Features
{
	class OldDeadeye : public LoopedCommand
	{
		using LoopedCommand::LoopedCommand;

		bool m_WasDeadeyeActive = false;

		void OnTick() override
		{
			auto ped = Self::GetPed();
			if (!ped.IsValid())
				return;

			const auto playerId = Self::GetPlayer().GetId();
			const bool active = PLAYER::_IS_SPECIAL_ABILITY_ACTIVE(playerId);

			if (m_WasDeadeyeActive && !active)
			{
				MISC::SET_TIME_SCALE(1.0f);
				PLAYER::_MODIFY_INFINITE_TRAIL_VISION(playerId, false);
			}
			m_WasDeadeyeActive = active;

			if (!active)
				return;

			// Old Dead Eye always uses the game's automatic tagging mode. There is no
			// target-filter editor anymore: enemies, animals and other valid targets
			// are handled by the default "all" tagging configuration.
			PLAYER::_SET_DEADEYE_TAGGING_CONFIG(playerId, 7);
			PLAYER::_SET_DEADEYE_TAGGING_ENABLED(playerId, true);
			PLAYER::_SET_LOCAL_PLAYER_PERSONA_ABILITY_FLAG(PERSONA_DISABLE_DEADEYE_PERFECT_ACCURACY, false);

			if (_SlipperyBastardAccuracy.GetState())
				ped.SetAccuracy(100);

			if (_MomentToRecuperate.GetState())
				PLAYER::_SET_LOCAL_PLAYER_PERSONA_ABILITY_FLAG(ePersonaAbilityFlag::PERSONA_EXIT_DEADEYE_ON_TAKING_DAMAGE, false);

			if (_EnhancedDeadeye.GetState())
			{
				MISC::SET_TIME_SCALE(0.3f);
				if (_SlowDeadeyeDrain.GetState())
					PLAYER::_MODIFY_INFINITE_TRAIL_VISION(playerId, true);
			}
		}

		void OnDisable() override
		{
			const auto playerId = Self::GetPlayer().GetId();
			PLAYER::_SET_LOCAL_PLAYER_PERSONA_ABILITY_FLAG(PERSONA_DISABLE_DEADEYE_PERFECT_ACCURACY, true);
			PLAYER::_SET_LOCAL_PLAYER_PERSONA_ABILITY_FLAG(ePersonaAbilityFlag::PERSONA_EXIT_DEADEYE_ON_TAKING_DAMAGE, true);
			PLAYER::_SET_DEADEYE_TAGGING_ENABLED(playerId, false);
			MISC::SET_TIME_SCALE(1.0f);
			PLAYER::_MODIFY_INFINITE_TRAIL_VISION(playerId, false);
			m_WasDeadeyeActive = false;
		}

		BoolCommand _EnhancedDeadeye{"enhanceddeadeye", "Enhanced Deadeye", "Adds cinematic time dilation and visual effects", false};
		BoolCommand _SlowDeadeyeDrain{"slowdeadeyedrain", "Slow Deadeye Drain", "Significantly reduces Deadeye drain rate", false};
		BoolCommand _SlipperyBastardAccuracy{"slipperybastardaccuracy", "Slippery Bastard Accuracy", "Ensures perfect accuracy with Slippery Bastard", false};
		BoolCommand _MomentToRecuperate{"momenttorecuperate", "A Moment to Recuperate", "Prevents exiting Deadeye when taking damage", false};
	};

	static OldDeadeye _OldDeadeye{"olddeadeye", "Old Deadeye", "Classic Dead Eye with automatic tagging enabled by default."};
}
