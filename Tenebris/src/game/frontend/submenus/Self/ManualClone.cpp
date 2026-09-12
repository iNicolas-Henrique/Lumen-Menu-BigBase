#include "ManualClone.hpp"
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

namespace YimMenu::Submenus
{
    namespace
    {
        struct BodyguardMountMirrorState
        {
            int Clone{};
            bool OwnerWasMounted{};
            bool GuardWasMounted{};
            bool SuppressingLegacyDistanceMount{};
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

            // MP_COLOR_10 is the game's red network-player color. Bodyguards use
            // light blue so friend/enemy affiliation is readable at a glance.
            MAP::BLIP_ADD_MODIFIER(blip, bodyguard ? "BLIP_MODIFIER_MP_COLOR_1"_J : "BLIP_MODIFIER_MP_COLOR_10"_J);
            MAP::SET_BLIP_SPRITE(blip, "BLIP_AMBIENT_PED_SMALL"_J, true);
            MAP::_SET_BLIP_NAME(blip, bodyguard ? "Guarda-costas Tenebris" : "Clone hostil Tenebris");
            return blip;
        }

        void SyncCloneRadarBlips()
        {
            // Remove stale/dead/reclassified entries first. Attached blips follow
            // their entity automatically, so there is no per-frame position work.
            for (auto& state : g_CloneRadarBlips)
            {
                if (!state.Clone)
                    continue;

                auto* managed = FindManagedClone(state.Clone);
                if (!managed || !managed->Ped || !ENTITY::DOES_ENTITY_EXIST(managed->Ped) ||
                    ENTITY::IS_ENTITY_DEAD(managed->Ped) || managed->Mode != state.Mode)
                {
                    RemoveCloneRadarBlip(state);
                }
            }

            for (const auto& managed : g_ManagedClones)
            {
                if (!managed.Ped || !ENTITY::DOES_ENTITY_EXIST(managed.Ped) || ENTITY::IS_ENTITY_DEAD(managed.Ped))
                    continue;

                bool alreadyTracked = false;
                for (const auto& state : g_CloneRadarBlips)
                {
                    if (state.Clone == managed.Ped)
                    {
                        alreadyTracked = true;
                        break;
                    }
                }
                if (alreadyTracked)
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
            {
                if (state.Clone == clone)
                    return state;
            }

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
            if (!clone || !owner || !ENTITY::DOES_ENTITY_EXIST(clone) || ENTITY::IS_ENTITY_DEAD(clone) ||
                !ENTITY::DOES_ENTITY_EXIST(owner) || ENTITY::IS_ENTITY_DEAD(owner))
                return;

            auto& mirror = GetBodyguardMountMirrorState(clone);
            auto& support = GetCloneSupportState(clone);
            const bool guardMounted = PED::IS_PED_ON_MOUNT(clone);
            const bool inCombat = PED::IS_PED_IN_COMBAT(clone, 0);

            if (ownerMounted)
            {
                // The old follow path also mounted guards merely for being far away.
                // Clear the temporary blocker as soon as the owner actually mounts.
                if (mirror.SuppressingLegacyDistanceMount)
                {
                    support.MountTaskIssuedAt = {};
                    mirror.SuppressingLegacyDistanceMount = false;
                }

                if (guardMounted)
                {
                    // Successful mount: clear stale task timing so a later fall can
                    // trigger an immediate remount instead of waiting for timeout.
                    support.MountTaskIssuedAt = {};
                    mirror.LastMountAttempt = {};
                }
                else if (!inCombat)
                {
                    const bool ownerJustMounted = !mirror.OwnerWasMounted;
                    const bool guardJustFell = mirror.GuardWasMounted;
                    const bool retryDue = mirror.LastMountAttempt == Clock::time_point{} || now - mirror.LastMountAttempt >= 900ms;
                    if ((ownerJustMounted || guardJustFell || retryDue) && retryDue)
                    {
                        // Reuse the assigned horse first. AcquireGuardHorse only
                        // searches/spawns a replacement if that horse is no longer usable.
                        support.MountTaskIssuedAt = {};
                        ForceGuardMount(clone, owner, true);
                        mirror.LastMountAttempt = now;
                    }
                }
            }
            else
            {
                if (guardMounted)
                {
                    const bool retryDue = mirror.LastDismountAttempt == Clock::time_point{} || now - mirror.LastDismountAttempt >= 1200ms;
                    if (retryDue)
                    {
                        // Mirror the owner's state: normal dismount, while keeping
                        // the assigned horse cached so it can be reused next time.
                        TASK::TASK_DISMOUNT_ANIMAL(clone, 0, 0, 0, 0.0f, 0);
                        mirror.LastDismountAttempt = now;
                    }
                }
                else
                {
                    if (!mirror.SuppressingLegacyDistanceMount && IsMountPlanActive(clone) && !inCombat)
                    {
                        // A mount task may have been issued between mirror ticks.
                        // Cancel it once whenever the owner is on foot, then block
                        // only the legacy distance-based remount path below.
                        TASK::CLEAR_PED_TASKS(clone, true, false);
                        support.MountTaskIssuedAt = {};
                    }

                    const float dSq = DistanceSquared(
                        ENTITY::GET_ENTITY_COORDS(clone, true, false),
                        ENTITY::GET_ENTITY_COORDS(owner, true, false));

                    if (!inCombat && dSq > kGuardHorseTriggerDistance * kGuardHorseTriggerDistance)
                    {
                        // MaintainProfessionalBodyguardFollow historically mounted a
                        // distant guard even when the owner was on foot. Refreshing
                        // this existing cooldown suppresses only that legacy path.
                        support.MountTaskIssuedAt = now;
                        mirror.SuppressingLegacyDistanceMount = true;
                    }
                    else if (mirror.SuppressingLegacyDistanceMount)
                    {
                        support.MountTaskIssuedAt = {};
                        mirror.SuppressingLegacyDistanceMount = false;
                    }
                }
            }

            mirror.OwnerWasMounted = ownerMounted;
            mirror.GuardWasMounted = PED::IS_PED_ON_MOUNT(clone);
        }

        void RunBodyguardMountMirror()
        {
            while (true)
            {
                auto self = Self::GetPed();
                if (!self.IsValid() || self.GetHealth() <= 0)
                {
                    ScriptMgr::Yield(500ms);
                    continue;
                }

                const int owner = self.GetHandle();
                const bool ownerMounted = GetPlayerMountHandle() != 0;
                const auto now = Clock::now();
                const bool hasClones = !g_ManagedClones.empty();
                bool hasBodyguards = false;

                SyncCloneRadarBlips();
                SyncCloneLifeAndLoot(owner, now);

                for (const auto& managed : g_ManagedClones)
                {
                    if (managed.Mode != CloneMode::Bodyguard || !managed.Ped || !ENTITY::DOES_ENTITY_EXIST(managed.Ped) || ENTITY::IS_ENTITY_DEAD(managed.Ped))
                        continue;
                    hasBodyguards = true;
                    SyncBodyguardMountState(managed.Ped, owner, ownerMounted, now);
                }

                ScriptMgr::Yield(hasBodyguards ? 250ms : (hasClones ? 350ms : 700ms));
            }
        }

        void EnsureBodyguardMountMirrorStarted()
        {
            static bool started{};
            if (started)
                return;
            started = true;
            FiberPool::Push([] { RunBodyguardMountMirror(); });
        }

        const char* OptimizedCloneModeLabelPt(CloneMode mode)
        {
            return mode == CloneMode::Bodyguard ? "Guarda-costas" : "NPC padrão hostil (campo de visão)";
        }

        const char* OptimizedCloneModeLabelEn(CloneMode mode)
        {
            return mode == CloneMode::Bodyguard ? "Bodyguard" : "Standard hostile NPC (field of view)";
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
                out.WeaponIndex = m_WeaponIndex;
                out.Mode = m_Mode == CloneMode::Bodyguard ? CloneMode::Bodyguard : CloneMode::FrenzyNpcs;
                out.SourcePlayerId = m_SourcePlayerId < 0 ? PLAYER::PLAYER_ID() : m_SourcePlayerId;
                out.Networked = m_Networked;
                out.EmoteFlourish = out.Mode == CloneMode::Bodyguard && m_EmoteFlourish;
                out.Health = m_Health;
                out.SeeingRange = m_SeeingRange;
                out.WalkStyleIndex = m_WalkStyleIndex;
                out.IdleGunTwirl = out.Mode == CloneMode::Bodyguard && m_IdleGunTwirl;
                return out;
            }

            std::string SourcePreview() const
            {
                const int id = m_SourcePlayerId < 0 ? PLAYER::PLAYER_ID() : m_SourcePlayerId;
                const char* name = PLAYER::GET_PLAYER_NAME(id);
                if (!name || !*name)
                    return Localization::IsPortuguese() ? "Eu" : "Me";
                if (id == PLAYER::PLAYER_ID())
                    return std::string(Localization::IsPortuguese() ? "Eu - " : "Me - ") + name;
                return name;
            }

        public:
            void Draw() override
            {
                ImGui::TextUnformatted(Localization::IsPortuguese() ? "Criar clone inteligente" : "Create smart clone");
                ImGui::Spacing();

                ImGui::TextUnformatted(Localization::IsPortuguese() ? "Aparência / jogador clonado" : "Appearance / cloned player");
                const std::string sourcePreview = SourcePreview();
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::BeginCombo("##CloneSourcePlayerOptimized", sourcePreview.c_str()))
                {
                    const int localId = PLAYER::PLAYER_ID();
                    const char* localName = PLAYER::GET_PLAYER_NAME(localId);
                    std::string localLabel = std::string(Localization::IsPortuguese() ? "Eu" : "Me") +
                        (localName && *localName ? std::string(" - ") + localName : "");
                    if (ImGui::Selectable(localLabel.c_str(), m_SourcePlayerId < 0 || m_SourcePlayerId == localId))
                        m_SourcePlayerId = localId;
                    for (int id = 0; id < 32; ++id)
                    {
                        if (id == localId || !NETWORK::NETWORK_IS_PLAYER_ACTIVE(id))
                            continue;
                        const int ped = PLAYER::GET_PLAYER_PED_SCRIPT_INDEX(id);
                        const char* name = PLAYER::GET_PLAYER_NAME(id);
                        if (ped && ENTITY::DOES_ENTITY_EXIST(ped) && name && *name && ImGui::Selectable(name, m_SourcePlayerId == id))
                            m_SourcePlayerId = id;
                    }
                    ImGui::EndCombo();
                }

                ImGui::Spacing();
                ImGui::SeparatorText(Localization::IsPortuguese() ? "ATRIBUTOS DO CLONE" : "CLONE ATTRIBUTES");
                ImGui::TextUnformatted(Localization::IsPortuguese() ? "Vida máxima" : "Maximum health");
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::SliderInt("##CloneHealthOptimized", &m_Health, 50, 5000, "%d HP");

                ImGui::TextUnformatted(Localization::IsPortuguese() ? "Campo de visão / alcance" : "Field of view / range");
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::SliderFloat("##CloneSeeingRangeOptimized", &m_SeeingRange, 20.0f, 500.0f, "%.0f m");

                ImGui::TextUnformatted(Localization::IsPortuguese() ? "Estilo de andar" : "Walk style");
                const char* walkPreview = Localization::IsPortuguese() ? kCloneWalkStyles[m_WalkStyleIndex].LabelPt : kCloneWalkStyles[m_WalkStyleIndex].LabelEn;
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::BeginCombo("##CloneWalkStyleOptimized", walkPreview))
                {
                    for (int i = 0; i < static_cast<int>(kCloneWalkStyles.size()); ++i)
                    {
                        const char* label = Localization::IsPortuguese() ? kCloneWalkStyles[i].LabelPt : kCloneWalkStyles[i].LabelEn;
                        if (ImGui::Selectable(label, m_WalkStyleIndex == i))
                            m_WalkStyleIndex = i;
                    }
                    ImGui::EndCombo();
                }

                ImGui::TextUnformatted(Localization::IsPortuguese() ? "Arma (Red Dead Online)" : "Weapon (Red Dead Online)");
                const char* weaponPreview = Localization::IsPortuguese() ? kCloneWeapons[m_WeaponIndex].LabelPt : kCloneWeapons[m_WeaponIndex].LabelEn;
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::BeginCombo("##ManualCloneWeaponOptimized", weaponPreview))
                {
                    for (int i = 0; i < static_cast<int>(kCloneWeapons.size()); ++i)
                    {
                        const char* label = Localization::IsPortuguese() ? kCloneWeapons[i].LabelPt : kCloneWeapons[i].LabelEn;
                        if (ImGui::Selectable(label, m_WeaponIndex == i))
                            m_WeaponIndex = i;
                    }
                    ImGui::EndCombo();
                }

                ImGui::Spacing();
                ImGui::SeparatorText(Localization::IsPortuguese() ? "COMPORTAMENTO" : "BEHAVIOR");
                const char* behaviorPreview = Localization::IsPortuguese() ? OptimizedCloneModeLabelPt(m_Mode) : OptimizedCloneModeLabelEn(m_Mode);
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::BeginCombo("##CloneBehaviorOptimized", behaviorPreview))
                {
                    const bool guardSelected = m_Mode == CloneMode::Bodyguard;
                    if (ImGui::Selectable(Localization::IsPortuguese() ? "Guarda-costas" : "Bodyguard", guardSelected))
                        m_Mode = CloneMode::Bodyguard;
                    if (ImGui::Selectable(Localization::IsPortuguese() ? "NPC padrão hostil (campo de visão)" : "Standard hostile NPC (field of view)", !guardSelected))
                        m_Mode = CloneMode::FrenzyNpcs;
                    ImGui::EndCombo();
                }

                if (m_Mode == CloneMode::Bodyguard)
                {
                    ImGui::Checkbox(Localization::IsPortuguese() ? "Floreio de emote (10 s)" : "Emote flourish (10 s)", &m_EmoteFlourish);
                    ImGui::Checkbox(Localization::IsPortuguese() ? "Girar pistola/revólver quando ocioso (~6 s)" : "Idle pistol/revolver gun tricks (~6 s)", &m_IdleGunTwirl);
                }

                ImGui::Spacing();
                ImGui::SeparatorText(Localization::IsPortuguese() ? "REDE" : "NETWORK");
                ImGui::Checkbox(Localization::IsPortuguese() ? "Ativar Network (visível para outros)" : "Enable Network (visible to others)", &m_Networked);

                ImGui::Spacing();
                ImGui::SeparatorText(Localization::IsPortuguese() ? "CONTROLE REMOTO" : "REMOTE CONTROL");
                if (g_RemoteControlledClone)
                {
                    ImGui::TextDisabled("%s", Localization::IsPortuguese() ? "Controle remoto ativo. BACK sai da câmera do clone." : "Remote control active. BACK exits the clone camera.");
                }
                else if (ImGui::Button(Localization::IsPortuguese() ? "CONTROLAR ÚLTIMO CLONE" : "CONTROL LAST CLONE", ImVec2(-1.0f, 34.0f)))
                {
                    if (auto* managed = GetLatestRemoteControllableClone())
                    {
                        const int clone = managed->Ped;
                        FiberPool::Push([clone] { RunCloneRemoteControl(clone); });
                    }
                    else
                    {
                        Notifications::Show("Tenebris",
                            Localization::IsPortuguese() ? "Crie primeiro um clone manual ou pela freecam." : "Create a manual/freecam clone first.",
                            NotificationType::Warning,
                            2400);
                    }
                }

                ImGui::Spacing();
                ImGui::SeparatorText(Localization::IsPortuguese() ? "POSIÇÃO / LIMPEZA" : "POSITION / CLEANUP");
                if (ImGui::Button(Localization::IsPortuguese() ? "CRIAR NA MINHA FRENTE" : "CREATE IN FRONT OF ME", ImVec2(215.0f, 34.0f)))
                {
                    const CloneOptions options = GetOptions();
                    FiberPool::Push([options] { SpawnManualCloneNearPlayer(options); });
                }
                ImGui::SameLine();
                if (ImGui::Button("FREECAM MULTI-SPAWN", ImVec2(225.0f, 34.0f)))
                {
                    const CloneOptions options = GetOptions();
                    FiberPool::Push([options] { RunCloneSpawnFreecam(options); });
                }
                if (ImGui::Button(Localization::IsPortuguese() ? "APAGAR TODOS OS CLONES" : "DELETE ALL CLONES", ImVec2(-1.0f, 32.0f)))
                    FiberPool::Push([] { DeleteAllManagedClones(); });
            }

            std::string_view GetMenuLabel() const override
            {
                return Localization::IsPortuguese() ? "Criar meu clone" : "Create my clone";
            }

            std::string GetMenuValue() const override
            {
                return Localization::IsPortuguese() ? OptimizedCloneModeLabelPt(m_Mode) : OptimizedCloneModeLabelEn(m_Mode);
            }

            std::string_view GetMenuDescription() const override
            {
                return Localization::IsPortuguese() ? "Clone configurável com guarda-costas ou IA hostil por campo de visão." : "Configurable clone with bodyguard or field-of-view hostile AI.";
            }

            bool RequiresImGuiEditor() const override { return true; }
            float GetPreferredEditorHeight() const override { return 980.0f; }
        };
    }

    std::shared_ptr<UIItem> CreateManualCloneItem()
    {
        EnsureBodyguardMountMirrorStarted();
        return std::make_shared<ManualCloneItem>();
    }
}
