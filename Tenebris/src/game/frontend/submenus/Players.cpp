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
#include "BoomPlus.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

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

    static void DrawModderTooltip(YimMenu::Player player, bool hovered)
    {
        if (!player.IsModder() || !hovered)
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

    static std::string GetDisplayName(uint8_t id, YimMenu::Player player)
    {
        std::string name = player.GetName();
        if (name.empty())
            name = std::format("Jogador #{}", static_cast<unsigned int>(id));
        return name;
    }

    static std::string Lowercase(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return value;
    }

    static std::vector<std::pair<uint8_t, Player>> GetSortedPlayers()
    {
        std::vector<std::pair<uint8_t, Player>> sortedPlayers;
        sortedPlayers.reserve(YimMenu::Players::GetPlayers().size());

        for (const auto& [id, player] : YimMenu::Players::GetPlayers())
        {
            if (player.IsValid())
                sortedPlayers.emplace_back(id, player);
        }

        std::sort(sortedPlayers.begin(), sortedPlayers.end(), [](const auto& left, const auto& right) {
            const auto leftName = Lowercase(GetDisplayName(left.first, left.second));
            const auto rightName = Lowercase(GetDisplayName(right.first, right.second));
            if (leftName == rightName)
                return left.first < right.first;
            return leftName < rightName;
        });

        return sortedPlayers;
    }

    static bool DrawPlayerNameRow(const std::string& displayName, bool selected)
    {
        // O Selectable antigo deixava apenas o primeiro caractere visivel em certas
        // combinacoes de tabela/clip do ImGui. Agora a area clicavel nao carrega o
        // texto; o nome e desenhado explicitamente dentro do retangulo da linha.
        const float rowHeight = ImGui::GetTextLineHeightWithSpacing() + 4.0f;
        const bool clicked = ImGui::Selectable("##player_row", selected, ImGuiSelectableFlags_None, ImVec2(0.0f, rowHeight));
        const bool hovered = ImGui::IsItemHovered();
        const ImVec2 rowMin = ImGui::GetItemRectMin();
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(rowMin.x + 5.0f, rowMin.y + 3.0f),
            ImGui::GetColorU32(ImGuiCol_Text),
            displayName.c_str());
        return clicked || (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left));
    }

    static void DrawPlayerList(bool external = true, float offset = 15.0f)
    {
        auto sortedPlayers = GetSortedPlayers();

        if (external)
        {
            ImGui::SetNextWindowPos(
                ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowSize().x + offset, ImGui::GetWindowPos().y));
            ImGui::SetNextWindowSize(ImVec2(300, ImGui::GetWindowSize().y));
            ImGui::Begin("Player List", nullptr, ImGuiWindowFlags_NoDecoration);

            ImGui::Checkbox("Spectate", &YimMenu::g_Spectating);
            for (auto& [id, player] : sortedPlayers)
            {
                const std::string displayName = GetDisplayName(id, player);
                ImGui::PushID(static_cast<int>(id));
                if (ImGui::Selectable(displayName.c_str(), YimMenu::Players::GetSelected() == player))
                    YimMenu::Players::SetSelected(id);
                DrawModderTooltip(player, ImGui::IsItemHovered());
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
            YimMenu::Players::SetSelected(sortedPlayers.front().first);
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
            ImGui::TableSetupColumn("Jogador", ImGuiTableColumnFlags_WidthStretch, 0.70f);
            ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthStretch, 0.30f);
            ImGui::TableHeadersRow();

            for (auto& [id, player] : sortedPlayers)
            {
                const std::string displayName = GetDisplayName(id, player);
                const bool isSelected = YimMenu::Players::GetSelected() == player;

                ImGui::PushID(static_cast<int>(id));
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                if (DrawPlayerNameRow(displayName, isSelected))
                    YimMenu::Players::SetSelected(id);
                DrawModderTooltip(player, ImGui::IsItemHovered());

                ImGui::TableSetColumnIndex(1);
                DrawTags(player);
                ImGui::PopID();
            }

            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::Separator();
        auto currentSelected = YimMenu::Players::GetSelected();
        if (currentSelected.IsValid())
            ImGui::Text("Selecionado: %s", currentSelected.GetName());
    }

    Players::Players() : Submenu::Submenu("Jogadores")
    {
        AddCategory(BuildInfoMenu());
        AddCategory(BuildHelpfulMenu());
        AddCategory(BuildTrollingMenu());
        AddCategory(BuildToxicMenu());
        AddCategory(BuildBoomPlusMenu());
        AddCategory(BuildKickMenu());

        for (auto& category : m_Categories)
            category->PrependItem(std::make_shared<ImGuiItem>([] {
                DrawPlayerList(false);
            }, "Selecionar jogador", "Mostra os jogadores em lista organizada e escolhe quem sera usado pelas opcoes desta categoria.", 520.0f));
    }
}
