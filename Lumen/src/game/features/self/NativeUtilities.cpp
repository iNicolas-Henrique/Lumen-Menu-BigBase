#include "core/commands/Command.hpp"
#include "game/backend/Self.hpp"
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

		RestorePlayerCommand g_RestorePlayer;
		CleanPlayerCommand g_CleanPlayer;
		RandomOutfitCommand g_RandomOutfit;
		ClearTasksCommand g_ClearTasks;
		RagdollPlayerCommand g_RagdollPlayer;
		RemoveWeaponsCommand g_RemoveWeapons;
		GroundPlayerCommand g_GroundPlayer;
	}
}
