#include "core/commands/Command.hpp"
#include "game/backend/Self.hpp"
#include "game/rdr/Enums.hpp"
#include "game/rdr/Natives.hpp"

namespace YimMenu::Features
{
	namespace
	{
		class RestorePlayerCommand final : public Command
		{
		public:
			RestorePlayerCommand() :
			    Command("restoreplayer", "Restaurar personagem", "Revive o personagem, recupera a vida e restaura o vigor.")
			{
			}

		private:
			void OnCall() override
			{
				auto ped = Self::GetPed();
				if (!ped)
					return;
				const int handle = ped.GetHandle();
				if (ENTITY::IS_ENTITY_DEAD(handle))
					PED::RESURRECT_PED(handle);
				PED::REVIVE_INJURED_PED(handle);
				ENTITY::SET_ENTITY_HEALTH(handle, ENTITY::GET_ENTITY_MAX_HEALTH(handle, false), 0);
				PLAYER::RESTORE_PLAYER_STAMINA(PLAYER::PLAYER_ID(), 1.0f);
			}
		};

		class CleanPlayerCommand final : public Command
		{
		public:
			CleanPlayerCommand() :
			    Command("cleanplayernow", "Limpar personagem", "Remove sangue e sujeira visíveis imediatamente.")
			{
			}

		private:
			void OnCall() override
			{
				auto ped = Self::GetPed();
				if (ped)
				{
					PED::CLEAR_PED_BLOOD_DAMAGE(ped.GetHandle());
					PED::_CLEAR_PED_BLOOD_DAMAGE_FACIAL(ped.GetHandle(), 0);
				}
			}
		};

		class RefillCoresCommand final : public Command
		{
		public:
			RefillCoresCommand() :
			    Command("refillcoresnow", "Preencher núcleos", "Preenche imediatamente os núcleos de vida, vigor e Olho da Morte.")
			{
			}

		private:
			void OnCall() override
			{
				auto ped = Self::GetPed();
				if (!ped)
					return;

				for (const auto core : {AttributeCore::ATTRIBUTE_CORE_HEALTH, AttributeCore::ATTRIBUTE_CORE_STAMINA, AttributeCore::ATTRIBUTE_CORE_DEADEYE})
					ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(ped.GetHandle(), static_cast<int>(core), 100);
			}
		};

		class RefillDeadEyeCommand final : public Command
		{
		public:
			RefillDeadEyeCommand() :
			    Command("refilldeadeyenow", "Preencher Olho da Morte", "Restaura imediatamente a barra e o anel externo do Olho da Morte.")
			{
			}

		private:
			void OnCall() override
			{
				const auto player = Self::GetPlayer().GetId();
				const float maximum = PLAYER::_GET_PLAYER_MAX_DEAD_EYE(player, false);
				PLAYER::_SPECIAL_ABILITY_RESTORE_BY_AMOUNT(player, maximum, 0, 0, 1);
				PLAYER::_SPECIAL_ABILITY_RESTORE_OUTER_RING(player, maximum);
			}
		};

		class ClearTasksCommand final : public Command
		{
		public:
			ClearTasksCommand() :
			    Command("cleartasks", "Cancelar tarefas", "Interrompe a animação, cenário ou tarefa atual do personagem.")
			{
			}

		private:
			void OnCall() override
			{
				auto ped = Self::GetPed();
				if (ped)
					TASK::CLEAR_PED_TASKS_IMMEDIATELY(ped.GetHandle(), true, true);
			}
		};

		class RagdollPlayerCommand final : public Command
		{
		public:
			RagdollPlayerCommand() :
			    Command("ragdollplayer", "Cair no chão", "Coloca o personagem em ragdoll por dois segundos.")
			{
			}

		private:
			void OnCall() override
			{
				auto ped = Self::GetPed();
				if (ped)
					PED::SET_PED_TO_RAGDOLL(ped.GetHandle(), 2000, 2000, 0, false, false, nullptr);
			}
		};

		class RemoveWeaponsCommand final : public Command
		{
		public:
			RemoveWeaponsCommand() :
			    Command("removeallweapons", "Remover todas as armas", "Remove as armas equipadas do personagem. Use com cuidado.")
			{
			}

		private:
			void OnCall() override
			{
				auto ped = Self::GetPed();
				if (ped)
					WEAPON::REMOVE_ALL_PED_WEAPONS(ped.GetHandle(), true, true);
			}
		};

		class MaximumWantedScoreCommand final : public Command
		{
		public:
			MaximumWantedScoreCommand() :
			    Command("maximumhostility", "Nível máximo de procurado", "Coloca a procura da lei diretamente no nível 5 e faz a polícia perseguir você imediatamente.")
			{
			}

		private:
			void OnCall() override
			{
				LAW::SET_WANTED_SCORE(PLAYER::PLAYER_ID(), 5);
				LAW::_FORCE_LAW_ON_LOCAL_PLAYER_IMMEDIATELY();
			}
		};

		class ClearLawStateCommand final : public Command
		{
		public:
			ClearLawStateCommand() :
			    Command("clearlawstate", "Sem nível de procurado", "Remove a recompensa, zera o nível de procurado e encerra a perseguição atual da lei.")
			{
			}

		private:
			void OnCall() override
			{
				LAW::CLEAR_BOUNTY(PLAYER::PLAYER_ID());
				LAW::CLEAR_WANTED_SCORE(PLAYER::PLAYER_ID());
				LAW::_SET_BOUNTY_HUNTER_PURSUIT_CLEARED();
			}
		};

		RestorePlayerCommand g_RestorePlayer;
		CleanPlayerCommand g_CleanPlayer;
		RefillCoresCommand g_RefillCores;
		RefillDeadEyeCommand g_RefillDeadEye;
		ClearTasksCommand g_ClearTasks;
		RagdollPlayerCommand g_RagdollPlayer;
		RemoveWeaponsCommand g_RemoveWeapons;
		MaximumWantedScoreCommand g_MaximumWantedScore;
		ClearLawStateCommand g_ClearLawState;
	}
}
