#include "core/commands/Command.hpp"
#include "core/commands/Commands.hpp"
#include "core/commands/IntCommand.hpp"
#include "core/logger/LogHelper.hpp"
#include "game/backend/Self.hpp"
#include "game/rdr/Enums.hpp"
#include "game/rdr/Natives.hpp"

#include <algorithm>

namespace YimMenu::Features
{
	namespace
	{
		IntCommand g_BountyAmount("bountyamount", "Valor da recompensa", "Digite o valor em centavos do jogo (100 = $1,00) e selecione Aplicar recompensa. O jogo ainda pode impor um limite interno.", 0, INT_MAX, 0);
		IntCommand g_WantedScore("wantedscore", "Nivel de procura da lei", "Intensidade local de procura da lei entre 0 e 5; não é a hostilidade online do jogador.", 0, 5, 0);

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
			    Command("refillcoresnow", "Preencher nucleos", "Preenche imediatamente os nucleos de vida, vigor e Olho da Morte.")
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

		class RandomOutfitCommand final : public Command
		{
		public:
			RandomOutfitCommand() :
			    Command("randomoutfit", "Traje aleatorio", "Aplica uma variação aleatória compatível com o modelo atual.")
			{
			}

		private:
			void OnCall() override
			{
				auto ped = Self::GetPed();
				if (ped)
					PED::_SET_RANDOM_OUTFIT_VARIATION(ped.GetHandle(), true);
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
			    Command("ragdollplayer", "Cair no chao", "Coloca o personagem em ragdoll por dois segundos.")
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

		class GroundPlayerCommand final : public Command
		{
		public:
			GroundPlayerCommand() :
			    Command("groundplayer", "Colocar no solo", "Reposiciona o personagem sobre a superfície abaixo dele.")
			{
			}

		private:
			void OnCall() override
			{
				auto ped = Self::GetPed();
				if (ped)
					ENTITY::PLACE_ENTITY_ON_GROUND_PROPERLY(ped.GetHandle(), true);
			}
		};

		class ApplyBountyCommand final : public Command
		{
		public:
			ApplyBountyCommand() :
			    Command("applybounty", "Aplicar recompensa", "Aplica ao personagem o valor configurado acima.")
			{
			}

		private:
			void OnCall() override
			{
				const int amount = std::max(0, g_BountyAmount.GetState());
				LOG(INFO) << "Applying local bounty value: " << amount;
				LAW::SET_BOUNTY(PLAYER::PLAYER_ID(), amount);
			}
		};

		class ApplyWantedScoreCommand final : public Command
		{
		public:
			ApplyWantedScoreCommand() :
			    Command("applywantedscore", "Aplicar procura da lei", "Aplica o nível de procura da lei configurado acima.")
			{
			}

		private:
			void OnCall() override
			{
				LAW::SET_WANTED_SCORE(PLAYER::PLAYER_ID(), g_WantedScore.GetState());
				if (g_WantedScore.GetState() > 0)
					LAW::_FORCE_LAW_ON_LOCAL_PLAYER_IMMEDIATELY();
			}
		};

		class MaximumWantedScoreCommand final : public Command
		{
		public:
			MaximumWantedScoreCommand() :
			    Command("maximumhostility", "Procura maxima da lei", "Define a intensidade de procura como 5 e chama a lei imediatamente.")
			{
			}

		private:
			void OnCall() override
			{
				g_WantedScore.SetState(5);
				LAW::SET_WANTED_SCORE(PLAYER::PLAYER_ID(), 5);
				LAW::_FORCE_LAW_ON_LOCAL_PLAYER_IMMEDIATELY();
			}
		};

		class ReadLawStateCommand final : public Command
		{
		public:
			ReadLawStateCommand() :
			    Command("readlawstate", "Ler valores atuais", "Atualiza os campos com a recompensa e procura da lei atuais.")
			{
			}

		private:
			void OnCall() override
			{
				g_BountyAmount.SetState(std::max(0, LAW::GET_BOUNTY(PLAYER::PLAYER_ID())));
				g_WantedScore.SetState(std::clamp(LAW::GET_WANTED_SCORE(PLAYER::PLAYER_ID()), 0, 5));
			}
		};

		class ClearLawStateCommand final : public Command
		{
		public:
			ClearLawStateCommand() :
			    Command("clearlawstate", "Limpar recompensa e procura", "Remove a recompensa, o wanted score e a perseguição atual.")
			{
			}

		private:
			void OnCall() override
			{
				LAW::CLEAR_BOUNTY(PLAYER::PLAYER_ID());
				LAW::CLEAR_WANTED_SCORE(PLAYER::PLAYER_ID());
				LAW::_SET_BOUNTY_HUNTER_PURSUIT_CLEARED();
				g_BountyAmount.SetState(0);
				g_WantedScore.SetState(0);
			}
		};

		RestorePlayerCommand g_RestorePlayer;
		CleanPlayerCommand g_CleanPlayer;
		RefillCoresCommand g_RefillCores;
		RefillDeadEyeCommand g_RefillDeadEye;
		RandomOutfitCommand g_RandomOutfit;
		ClearTasksCommand g_ClearTasks;
		RagdollPlayerCommand g_RagdollPlayer;
		RemoveWeaponsCommand g_RemoveWeapons;
		GroundPlayerCommand g_GroundPlayer;
		ApplyBountyCommand g_ApplyBounty;
		ApplyWantedScoreCommand g_ApplyWantedScore;
		MaximumWantedScoreCommand g_MaximumWantedScore;
		ReadLawStateCommand g_ReadLawState;
		ClearLawStateCommand g_ClearLawState;
	}
}
