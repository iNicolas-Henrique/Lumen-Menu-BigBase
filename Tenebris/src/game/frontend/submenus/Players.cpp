#include "Players.hpp"

#include "core/frontend/widgets/imgui_colors.h"
#include "game/backend/PlayerData.hpp"
#include "game/backend/Players.hpp"
#include "game/backend/PlayerDatabase.hpp"
#include "game/features/Features.hpp"
#include "game/frontend/items/Items.hpp"

#include "Player/Helpful.hpp"
#include "Player/Info.hpp"
#include "Player/Kick.hpp"
#include "Player/Toxic.hpp"
#include "Player/Trolling.hpp"
#include "BoomPlus.hpp" // This includes the UI elements for Zombies/Indian Attack
// REMOVED #include "Player/Zombies.hpp"

namespace YimMenu::Submenus
{
    struct Tag
    {
        std::string Name;
        ImVec4 Color;
    };

    static std::vector<Tag> GetPlayerTags(YimMenu::Player player)
    {
        std::vector<Tag> tags;

        if (player.IsHost())
            tags.push_back({"HOST", ImGui::Colors::DeepSkyBlue});

        if (player.IsModder())
            tags.push_back({"HACKER", ImGui::Colors::DeepPink});

        if (player.GetPed() && player.GetPed().IsInvincible())
            tags.push_back({"GOD", ImGui::Colors::Crimson});

        if (player.GetPed() && !player.GetPed().IsVisible())
            tags.push_back({"INVIS", ImGui::Colors::MediumPurple});

        return tags;
    }

    static void DrawModderTooltip(YimMenu::Player player)
    {
        if (!player.IsModder() || !ImGui::IsItemHovered())
            return;

        ImGui::BeginTooltip();
        ImGui::TextUnformatted("Detectado pelo Tenebris:");
        for (auto detection : player.GetData().m_Detections)
            ImGui::BulletText("%s", g_PlayerDatabase->ConvertDetectionToDescription(detection).c_str());
        ImGui::EndTooltip();
    }

    static void DrawTags(YimMenu::Player player)
    {
        const auto tags = GetPlayerTags(player);
        if (tags.empty())
        {
            ImGui::TextDisabled("-");
            return;
        }

        for (std::size_t i = 0; i < tags.size(); ++i)
        {
            if (i > 0)
                ImGui::SameLine(0.0f, 4.0f);

            ImGui::TextColored(tags[i].Color, "[%s]", tags[i].Name.c_str());
        }
    }

    static void DrawPlayerList(bool external = true, float offset = 15.0f)
    {
        struct ComparePlayerNames
        {
            bool operator()(YimMenu::Player a, YimMenu::Player b) const
            {
                std::string nameA = a.GetName();
                std::string nameB = b.GetName();
                return nameA < nameB;
            }
        };

        std::multimap<uint8_t, Player, ComparePlayerNames> sortedPlayers(YimMenu::Players::GetPlayers().begin(),
            YimMenu::Players::GetPlayers().end());

        if (external)
        {
            ImGui::SetNextWindowPos(
                ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowSize().x + offset, ImGui::GetWindowPos().y));
            ImGui::SetNextWindowSize(ImVec2(215, ImGui::GetWindowSize().y));
            ImGui::Begin("Player List", nullptr, ImGuiWindowFlags_NoDecoration);

            ImGui::Checkbox("Spectate", &YimMenu::g_Spectating);
            for (auto& [id, player] : sortedPlayers)
            {
                std::string display_name = player.GetName();

                ImGui::PushID(id);
                if (ImGui::Selectable(display_name.c_str(), (YimMenu::Players::GetSelected() == player)))
                    YimMenu::Players::SetSelected(id);
                DrawModderTooltip(player);
                DrawTags(player);
                ImGui::PopID();
            }
            ImGui::End();
            return;
        }

        if (sortedPlayers.empty())
        {
            ImGui::TextDisabled("Nenhum jogador disponivel nesta sessao.");
            return;
        }

        auto selected = YimMenu::Players::GetSelected();
        if (!selected.IsValid())
        {
            YimMenu::Players::SetSelected(sortedPlayers.begin()->first);
            selected = YimMenu::Players::GetSelected();
        }

        ImGui::TextDisabled("%zu jogador(es) na sessao", sortedPlayers.size());
        ImGui::Spacing();

        constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_RowBg
            | ImGuiTableFlags_BordersInnerH
            | ImGuiTableFlags_BordersOuter
            | ImGuiTableFlags_SizingStretchProp;

        if (ImGui::BeginTable("##TenebrisPlayerList", 2, tableFlags))
        {
            ImGui::TableSetupColumn("Jogador", ImGuiTableColumnFlags_WidthStretch, 0.62f);
            ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthStretch, 0.38f);
            ImGui::TableHeadersRow();

            for (auto& [id, player] : sortedPlayers)
            {
                if (!player.IsValid())
                    continue;

                ImGui::PushID(id);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                const bool isSelected = YimMenu::Players::GetSelected() == player;
                const std::string displayName = player.GetName();
                if (ImGui::Selectable(displayName.c_str(), isSelected, ImGuiSelectableFlags_None, ImVec2(-FLT_MIN, 0.0f)))
                    YimMenu::Players::SetSelected(id);
                DrawModderTooltip(player);

                ImGui::TableSetColumnIndex(1);
                DrawTags(player);
                ImGui::PopID();
            }

            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextDisabled("Selecionado: %s", YimMenu::Players::GetSelected().GetName());
    }

    Players::Players() : Submenu::Submenu("Jogadores")
    {
        AddCategory(BuildInfoMenu());
        AddCategory(BuildHelpfulMenu());
        AddCategory(BuildTrollingMenu());
        AddCategory(BuildToxicMenu());
        AddCategory(BuildBoomPlusMenu()); // BoomPlus contains the Zombie/Indian Attack UI items
        AddCategory(BuildKickMenu());

        for (auto& category : m_Categories)
            category->PrependItem(std::make_shared<ImGuiItem>([] {
                DrawPlayerList(false);
            }, "Selecionar jogador", "Mostra os jogadores em lista organizada e escolhe quem sera usado pelas opcoes desta categoria.", 520.0f));
    }
}
