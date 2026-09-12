#include "ManualClone.hpp"

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
        return std::make_shared<ManualCloneItem>();
    }
}
