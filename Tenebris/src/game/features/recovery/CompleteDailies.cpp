#include "core/commands/Command.hpp"
#include "core/frontend/Localization.hpp"
#include "core/frontend/Notifications.hpp"
#include "game/backend/FiberPool.hpp"
#include "game/rdr/Natives.hpp"
#include "game/rdr/Stats.hpp"
#include "game/rdr/data/Stats.hpp"

namespace YimMenu::Features
{
	class CompleteDailies : public Command
	{
		using Command::Command;

		void OnCall() override
		{
			FiberPool::Push([] {
				for (const auto& stat : Data::int_stats)
				{
					Stats::StatId statId{};
					statId.BaseId = stat.BaseId;
					statId.PermutationId = stat.PermutationId;
					STATS::_STAT_ID_INCREMENT_INT(&statId, stat.desiredValue);
				}
				Notifications::Show("Tenebris",
				    Localization::IsPortuguese() ? "Progresso dos desafios diários aplicado." : "Daily challenge progress applied.",
				    NotificationType::Success,
				    2600);
			});
		}
	};

	static CompleteDailies _CompleteDailies{
	    "completedailies",
	    "Complete Daily Challenges",
	    "Applies the configured progress to all daily challenge statistics."};
}
