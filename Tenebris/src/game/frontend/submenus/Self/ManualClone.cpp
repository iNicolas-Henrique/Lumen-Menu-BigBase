#include "ManualClone.hpp"
#include "core/frontend/manager/AdvancedEditor.hpp"
#include "util/Rewards.hpp"

#define ManualCloneItem ManualCloneItemLegacy
#define CreateManualCloneItem CreateManualCloneItemLegacy
#include "ManualCloneCore.inc"
#undef CreateManualCloneItem
#undef ManualCloneItem

#ifdef FindNearestNpcTarget
#undef FindNearestNpcTarget
#endif
#ifdef IsValidNpcTarget
#undef IsValidNpcTarget
#endif
#ifdef TaskVanillaCombat
#undef TaskVanillaCombat
#endif
#ifdef EnsureCloneWeaponEquipped
#undef EnsureCloneWeaponEquipped
#endif
#ifdef FindBodyguardThreat
#undef FindBodyguardThreat
#endif
#ifdef MaintainBodyguardFollow
#undef MaintainBodyguardFollow
#endif
#ifdef StartCloneGunTwirl
#undef StartCloneGunTwirl
#endif
#ifdef PlayTimedEmote
#undef PlayTimedEmote
#endif
#ifdef PlayRareSocialReaction
#undef PlayRareSocialReaction
#endif
#ifdef MaybeStartMourning
#undef MaybeStartMourning
#endif
#ifdef DeleteAllManagedClones
#undef DeleteAllManagedClones
#endif

namespace YimMenu::Submenus
{
	namespace
	{
		struct BodyguardMountMirrorState
		{
			int Clone{};
			bool OwnerWasMounted{};
			bool GuardWasMounted{};
			Clock::time_point LastMountAttempt{};
			Clock::time_point LastDismountAttempt{};
		};

		struct CloneRadarBlipState
		{
			int Clone{};
			int Blip{};
			CloneMode Mode{CloneMode::Bodyguard};
		};

		std::array<BodyguardMountMirrorState, kMaxActiveClones> g_BodyguardMountMirror{};
		std::array<CloneRadarBlipState, kMaxActiveClones> g_CloneRadarBlips{};

#include "ManualCloneLifeLoot.inc"
#include "ManualCloneAdvanced.inc"

		void RemoveCloneRadarBlip(CloneRadarBlipState& state)
		{
			if (state.Blip)
			{
				int blip = state.Blip;
				MAP::REMOVE_BLIP(&blip);
			}
			state = {};
		}

		int CreateCloneRadarBlip(int clone, CloneMode mode)
		{
			if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone) || ENTITY::IS_ENTITY_DEAD(clone))
				return 0;
			const bool bodyguard = mode == CloneMode::Bodyguard;
			const Hash style = bodyguard ? "BLIP_STYLE_COMPANION"_J : "BLIP_STYLE_CREATOR_DEFAULT"_J;
			const int blip = MAP::BLIP_ADD_FOR_ENTITY(style, clone);
			if (!blip)
				return 0;
			MAP::BLIP_ADD_MODIFIER(blip, bodyguard ? "BLIP_MODIFIER_MP_COLOR_1"_J : "BLIP_MODIFIER_MP_COLOR_10"_J);
			MAP::SET_BLIP_SPRITE(blip, "BLIP_AMBIENT_PED_SMALL"_J, true);
			MAP::_SET_BLIP_NAME(blip, bodyguard ? "Guarda-costas Tenebris" : "Clone hostil Tenebris");
			return blip;
		}

		void SyncCloneRadarBlips()
		{
			for (auto& state : g_CloneRadarBlips)
			{
				if (!state.Clone)
					continue;
				auto* managed = FindManagedClone(state.Clone);
				if (!managed || !managed->Ped || !ENTITY::DOES_ENTITY_EXIST(managed->Ped) || ENTITY::IS_ENTITY_DEAD(managed->Ped) || managed->Mode != state.Mode)
					RemoveCloneRadarBlip(state);
			}

			for (const auto& managed : g_ManagedClones)
			{
				if (!managed.Ped || !ENTITY::DOES_ENTITY_EXIST(managed.Ped) || ENTITY::IS_ENTITY_DEAD(managed.Ped))
					continue;
				bool tracked{};
				for (const auto& state : g_CloneRadarBlips)
					if (state.Clone == managed.Ped) { tracked = true; break; }
				if (tracked)
					continue;
				for (auto& state : g_CloneRadarBlips)
				{
					if (state.Clone)
						continue;
					state.Clone = managed.Ped;
					state.Mode = managed.Mode;
					state.Blip = CreateCloneRadarBlip(managed.Ped, managed.Mode);
					if (!state.Blip)
						state = {};
					break;
				}
			}
		}

		BodyguardMountMirrorState& GetBodyguardMountMirrorState(int clone)
		{
			for (auto& state : g_BodyguardMountMirror)
				if (state.Clone == clone)
					return state;
			for (auto& state : g_BodyguardMountMirror)
			{
				if (!state.Clone || !ENTITY::DOES_ENTITY_EXIST(state.Clone) || ENTITY::IS_ENTITY_DEAD(state.Clone))
				{
					state = {};
					state.Clone = clone;
					return state;
				}
			}
			g_BodyguardMountMirror[0] = {};
			g_BodyguardMountMirror[0].Clone = clone;
			return g_BodyguardMountMirror[0];
		}

		void SyncBodyguardMountState(int clone, int owner, bool ownerMounted, Clock::time_point now)
		{
			if (g_BodyguardsDismissed || !clone || !owner || !ENTITY::DOES_ENTITY_EXIST(clone) || ENTITY::IS_ENTITY_DEAD(clone) || !ENTITY::DOES_ENTITY_EXIST(owner))
				return;
			auto& mirror = GetBodyguardMountMirrorState(clone);
			auto& support = GetCloneSupportState(clone);
			const bool guardMounted = PED::IS_PED_ON_MOUNT(clone);
			const bool inCombat = PED::IS_PED_IN_COMBAT(clone, 0);

			if (ownerMounted)
			{
				if (guardMounted)
				{
					support.MountTaskUntil = {};
					mirror.LastMountAttempt = {};
				}
				else if (!inCombat)
				{
					const bool urgent = !mirror.OwnerWasMounted || mirror.GuardWasMounted;
					const bool due = mirror.LastMountAttempt == Clock::time_point{} || now - mirror.LastMountAttempt >= 900ms;
					if ((urgent || due) && due)
					{
						support.MountTaskUntil = {};
						ForceGuardMount(clone, owner, true);
						if (support.Horse) EnsureSupportHorseVisible(support.Horse);
						mirror.LastMountAttempt = now;
					}
				}
			}
			else
			{
				if (guardMounted)
				{
					const bool due = mirror.LastDismountAttempt == Clock::time_point{} || now - mirror.LastDismountAttempt >= 1200ms;
					if (due)
					{
						TASK::TASK_DISMOUNT_ANIMAL(clone, 0, 0, 0, 0.0f, 0);
						mirror.LastDismountAttempt = now;
					}
				}
				else if (IsMountPlanActive(clone) && !inCombat)
				{
					TASK::CLEAR_PED_TASKS(clone, true, false);
					support.MountTaskUntil = {};
				}
			}
			mirror.OwnerWasMounted = ownerMounted;
			mirror.GuardWasMounted = PED::IS_PED_ON_MOUNT(clone);
		}

		void RunCloneSupportController()
		{
			while (true)
			{
				// Nothing in this controller is needed until at least one managed
				// clone exists. Keeping the editor-open path free of game-native work
				// also avoids the old crash surface when simply entering CLONE MANUAL.
				if (g_ManagedClones.empty())
				{
					ScriptMgr::Yield(700ms);
					continue;
				}

				auto self = Self::GetPed();
				if (!self.IsValid() || self.GetHealth() <= 0)
				{
					ScriptMgr::Yield(500ms);
					continue;
				}
				const int owner = self.GetHandle();
				const auto now = Clock::now();
				const bool ownerMounted = GetPlayerMountHandle() != 0;
				bool hasGuards{};
				SyncCloneRadarBlips();
				SyncCloneLifeAndLoot(owner, now);
				for (const auto& managed : g_ManagedClones)
				{
					if (managed.Mode != CloneMode::Bodyguard || !managed.Ped || !ENTITY::DOES_ENTITY_EXIST(managed.Ped) || ENTITY::IS_ENTITY_DEAD(managed.Ped))
						continue;
					hasGuards = true;
					SyncBodyguardMountState(managed.Ped, owner, ownerMounted, now);
				}
				ScriptMgr::Yield(hasGuards ? 250ms : 350ms);
			}
		}

		void EnsureCloneControllersStarted()
		{
			static bool supportStarted{};
			if (!supportStarted)
			{
				supportStarted = true;
				FiberPool::Push([] { RunCloneSupportController(); });
			}
			StartCloneEnhancementSupervisor();
		}

		void DeleteAllClonesRobust()
		{
			DeleteAllManagedClonesAndMounts();
			for (auto& state : g_CloneRadarBlips)
				RemoveCloneRadarBlip(state);
			for (auto& state : g_BodyguardMountMirror)
				state = {};
			for (auto& state : g_CloneLifeLoot)
				state = {};
			ClearActiveCloneExtensionState();
		}

		const char* ActiveModeLabel(CloneMode mode)
		{
			if (Localization::IsPortuguese())
				return mode == CloneMode::Bodyguard ? "Guarda-costas" : "NPC hostil em equipe";
			return mode == CloneMode::Bodyguard ? "Bodyguard" : "Hostile squad NPC";
		}

		class ManualCloneItem final : public UIItem
		{
			int m_WeaponIndex{};
			CloneMode m_Mode{CloneMode::Bodyguard};
			int m_SourcePlayerId{-1};
			bool m_Networked{};
			bool m_EmoteFlourish{true};
			int m_Health{800};
			float m_SeeingRange{220.0f};
			int m_WalkStyleIndex{};
			bool m_IdleGunTwirl{true};

			CloneOptions GetOptions() const
			{
				CloneOptions out{};
				out.WeaponIndex = std::clamp(m_WeaponIndex, 0, static_cast<int>(kCloneWeapons.size()) - 1);
				out.Mode = m_Mode == CloneMode::Bodyguard ? CloneMode::Bodyguard : CloneMode::FrenzyNpcs;
				out.SourcePlayerId = m_SourcePlayerId < 0 ? PLAYER::PLAYER_ID() : m_SourcePlayerId;
				out.Networked = m_Networked;
				out.EmoteFlourish = out.Mode == CloneMode::Bodyguard && m_EmoteFlourish;
				out.Health = std::clamp(m_Health, 50, 5000);
				out.SeeingRange = std::clamp(m_SeeingRange, 20.0f, 500.0f);
				out.WalkStyleIndex = std::clamp(m_WalkStyleIndex, 0, static_cast<int>(kCloneWalkStyles.size()) - 1);
				out.IdleGunTwirl = out.Mode == CloneMode::Bodyguard && m_IdleGunTwirl;
				return out;
			}

			std::string SourcePreview() const
			{
				const int id = m_SourcePlayerId < 0 ? PLAYER::PLAYER_ID() : m_SourcePlayerId;
				const char* name = PLAYER::GET_PLAYER_NAME(id);
				if (!name || !*name)
					return Localization::IsPortuguese() ? "Eu" : "Me";
				return id == PLAYER::PLAYER_ID() ? std::string(Localization::IsPortuguese() ? "Eu - " : "Me - ") + name : std::string(name);
			}

			bool HasLivingBodyguard() const
			{
				for (const auto& managed : g_ManagedClones)
					if (managed.Mode == CloneMode::Bodyguard && managed.Ped && ENTITY::DOES_ENTITY_EXIST(managed.Ped) && !ENTITY::IS_ENTITY_DEAD(managed.Ped))
						return true;
				return false;
			}

		public:
			void Draw() override
			{
				if (AdvancedEditor::IsDetachedMode())
				{
					g_FreecamLiveOptions = GetOptions();
					g_FreecamLiveOptionsValid = true;
				}

				ImGui::TextUnformatted(Localization::IsPortuguese() ? "Criar clone inteligente" : "Create smart clone");
				ImGui::Spacing();
				ImGui::TextUnformatted(Localization::IsPortuguese() ? "Jogador / aparência" : "Player / appearance");
				const std::string sourcePreview = SourcePreview();
				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::BeginCombo("##CloneSourcePlayer", sourcePreview.c_str()))
				{
					const int localId = PLAYER::PLAYER_ID();
					const char* localName = PLAYER::GET_PLAYER_NAME(localId);
					std::string localLabel = std::string(Localization::IsPortuguese() ? "Eu" : "Me") + (localName && *localName ? std::string(" - ") + localName : "");
					if (ImGui::Selectable(localLabel.c_str(), m_SourcePlayerId < 0 || m_SourcePlayerId == localId)) m_SourcePlayerId = localId;
					for (int id = 0; id < 32; ++id)
					{
						if (id == localId || !NETWORK::NETWORK_IS_PLAYER_ACTIVE(id)) continue;
						const int ped = PLAYER::GET_PLAYER_PED_SCRIPT_INDEX(id);
						const char* name = PLAYER::GET_PLAYER_NAME(id);
						if (ped && ENTITY::DOES_ENTITY_EXIST(ped) && name && *name && ImGui::Selectable(name, m_SourcePlayerId == id)) m_SourcePlayerId = id;
					}
					ImGui::EndCombo();
				}

				ImGui::SeparatorText(Localization::IsPortuguese() ? "ATRIBUTOS" : "ATTRIBUTES");
				ImGui::TextUnformatted(Localization::IsPortuguese() ? "Vida máxima" : "Maximum health");
				ImGui::SetNextItemWidth(-1.0f);
				ImGui::SliderInt("##CloneHealth", &m_Health, 50, 5000, "%d HP");
				ImGui::TextUnformatted(Localization::IsPortuguese() ? "Alcance de percepção" : "Perception range");
				ImGui::SetNextItemWidth(-1.0f);
				ImGui::SliderFloat("##CloneSeeingRange", &m_SeeingRange, 20.0f, 500.0f, "%.0f m");

				ImGui::TextUnformatted(Localization::IsPortuguese() ? "Dano causado pelos clones" : "Clone outgoing damage");
				ImGui::SetNextItemWidth(-1.0f);
				ImGui::SliderInt("##CloneDamagePercent", &g_CloneDamagePercent, 100, 500, "%d%%");
				ImGui::TextDisabled(Localization::IsPortuguese() ? "100%% = dano normal; acima disso adiciona dano somente aos acertos do clone." : "100%% = normal damage; higher values add damage only to clone hits.");

				ImGui::TextUnformatted(Localization::IsPortuguese() ? "Estilo de andar" : "Walk style");
				const char* walkPreview = Localization::IsPortuguese() ? kCloneWalkStyles[m_WalkStyleIndex].LabelPt : kCloneWalkStyles[m_WalkStyleIndex].LabelEn;
				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::BeginCombo("##CloneWalkStyle", walkPreview))
				{
					for (int i = 0; i < static_cast<int>(kCloneWalkStyles.size()); ++i)
					{
						const char* label = Localization::IsPortuguese() ? kCloneWalkStyles[i].LabelPt : kCloneWalkStyles[i].LabelEn;
						if (ImGui::Selectable(label, m_WalkStyleIndex == i)) m_WalkStyleIndex = i;
					}
					ImGui::EndCombo();
				}

				ImGui::TextUnformatted(Localization::IsPortuguese() ? "Arma" : "Weapon");
				const char* weaponPreview = Localization::IsPortuguese() ? kCloneWeapons[m_WeaponIndex].LabelPt : kCloneWeapons[m_WeaponIndex].LabelEn;
				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::BeginCombo("##CloneWeapon", weaponPreview))
				{
					for (int i = 0; i < static_cast<int>(kCloneWeapons.size()); ++i)
					{
						const char* label = Localization::IsPortuguese() ? kCloneWeapons[i].LabelPt : kCloneWeapons[i].LabelEn;
						if (ImGui::Selectable(label, m_WeaponIndex == i)) m_WeaponIndex = i;
					}
					ImGui::EndCombo();
				}

				ImGui::TextUnformatted(Localization::IsPortuguese() ? "Comportamento" : "Behavior");
				const char* modePreview = ActiveModeLabel(m_Mode);
				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::BeginCombo("##CloneMode", modePreview))
				{
					for (CloneMode mode : {CloneMode::Bodyguard, CloneMode::FrenzyNpcs})
						if (ImGui::Selectable(ActiveModeLabel(mode), m_Mode == mode)) m_Mode = mode;
					ImGui::EndCombo();
				}

				if (m_Mode == CloneMode::Bodyguard)
				{
					ImGui::Checkbox(Localization::IsPortuguese() ? "Emotes quando ocioso" : "Idle emotes", &m_EmoteFlourish);
					ImGui::Checkbox(Localization::IsPortuguese() ? "Gun twirl quando ocioso" : "Idle gun twirl", &m_IdleGunTwirl);
				}
				ImGui::Checkbox(Localization::IsPortuguese() ? "Ativar Network (visível para outros)" : "Enable Network (visible to others)", &m_Networked);

				if (HasLivingBodyguard())
				{
					ImGui::SeparatorText(Localization::IsPortuguese() ? "GUARDA-COSTAS" : "BODYGUARDS");
					if (!g_BodyguardsDismissed)
					{
						if (ImGui::Button(Localization::IsPortuguese() ? "DISPENSAR GUARDA-COSTAS" : "DISMISS BODYGUARDS", ImVec2(-1.0f, 0.0f))) SetBodyguardsDismissed(true);
					}
					else if (ImGui::Button(Localization::IsPortuguese() ? "CONVOCAR GUARDA-COSTAS" : "RECALL BODYGUARDS", ImVec2(-1.0f, 0.0f))) SetBodyguardsDismissed(false);
					ImGui::TextWrapped(Localization::IsPortuguese() ? "Dispensados usam seus cavalos e retornam ao jogador original que foi clonado; convocados voltam até você usando navegação normal." : "Dismissed guards use their horses and return to the source player; recalled guards navigate back to you normally.");
				}

				ImGui::SeparatorText(Localization::IsPortuguese() ? "AÇÕES" : "ACTIONS");
				if (ImGui::Button(Localization::IsPortuguese() ? "CRIAR CLONE" : "CREATE CLONE", ImVec2(-1.0f, 0.0f)))
				{
					const CloneOptions options = GetOptions();
					EnsureCloneControllersStarted();
					FiberPool::Push([options] { PruneManagedClones(true); SpawnManualCloneNearPlayer(options); });
				}
				if (ImGui::Button(Localization::IsPortuguese() ? "FREECAM MULTI-SPAWN" : "FREECAM MULTI-SPAWN", ImVec2(-1.0f, 0.0f)))
				{
					const CloneOptions options = GetOptions();
					EnsureCloneControllersStarted();
					FiberPool::Push([options] { RunCloneSpawnFreecamInteractive(options); });
				}
				ImGui::TextWrapped(Localization::IsPortuguese() ? "Na Freecam o editor fica preso à direita. Mouse sobre o painel edita; fora dele a câmera continua livre. ENTER cria clone e BACK sai." : "In Freecam the editor stays docked right. Mouse over it edits; outside it the camera stays free. ENTER spawns and BACK exits.");
				if (ImGui::Button(Localization::IsPortuguese() ? "APAGAR TODOS OS CLONES" : "DELETE ALL CLONES", ImVec2(-1.0f, 0.0f)))
					FiberPool::Push([] { DeleteAllClonesRobust(); });
				ImGui::Text("%s: %zu / %zu", Localization::IsPortuguese() ? "Clones ativos" : "Active clones", ActiveCloneCount(), kMaxActiveClones);
			}

			std::string_view GetMenuLabel() const override { return Localization::IsPortuguese() ? "CLONE MANUAL" : "MANUAL CLONE"; }
			float GetPreferredEditorHeight() const override { return 720.0f; }
			bool RequiresImGuiEditor() const override { return true; }
			bool IsSelectable() const override { return true; }
			bool HandleEditorKey(int) override { return false; }
		};
	}

	std::shared_ptr<UIItem> CreateManualCloneItem()
	{
		return std::make_shared<ManualCloneItem>();
	}
}
