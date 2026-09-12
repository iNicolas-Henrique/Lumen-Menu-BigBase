#include "Esp.hpp"
#include "common.hpp"
#include "core/commands/BoolCommand.hpp"
#include "core/commands/ColorCommand.hpp"
#include "game/backend/Players.hpp"
#include "game/backend/Self.hpp"
#include "game/pointers/Pointers.hpp"
#include "game/rdr/Pools.hpp"
#include "game/rdr/Scripts.hpp"
#include "util/Math.hpp"
#include "game/rdr/data/PedModels.hpp"
#include "game/rdr/invoker/Invoker.hpp"

namespace
{
    constexpr int headBone          = 21030;
    constexpr int neckBone          = 14283;
    constexpr int torsoBone         = 14410;
    constexpr int leftHandBone      = 34606;
    constexpr int rightHandBone     = 22798;
    constexpr int leftFootBone      = 45454;
    constexpr int rightFootBone     = 33646;
    constexpr int leftElbowBone     = 22711;
    constexpr int rightElbowBone    = 2992;
    constexpr int leftKneeBone      = 22173;
    constexpr int rightKneeBone     = 63133;
    constexpr int leftShoulderBone  = 44903;
    constexpr int rightShoulderBone = 4312;

    constexpr int horseHeadBone   = 21030;
    constexpr int horseNeckBone0  = 14283;
    constexpr int horseTorsoBone  = 14410;
    constexpr int horsePelvisBone = 56200;

    constexpr int horseFrontLeftUpperArmBone = 37873;
    constexpr int horseFrontLeftForearmBone  = 53675;
    constexpr int horseFrontLeftHandBone     = 34606;
    constexpr int horseFrontLeftFingerBone0  = 41403;
    constexpr int horseFrontLeftFingerBone1  = 41404;

    constexpr int horseFrontRightUpperArmBone = 46065;
    constexpr int horseFrontRightForearmBone  = 54187;
    constexpr int horseFrontRightHandBone     = 22798;
    constexpr int horseFrontRightFingerBone0  = 16827;
    constexpr int horseFrontRightFingerBone1  = 16828;

    constexpr int horseRearLeftThighBone = 65478;
    constexpr int horseRearLeftCalfBone  = 55120;
    constexpr int horseRearLeftFootBone  = 45454;
    constexpr int horseRearLeftToeBone0  = 53081;
    constexpr int horseRearLeftToeBone1  = 53082;

    constexpr int horseRearRightThighBone = 6884;
    constexpr int horseRearRightCalfBone  = 43312;
    constexpr int horseRearRightFootBone  = 33646;
    constexpr int horseRearRightToeBone0  = 41273;
    constexpr int horseRearRightToeBone1  = 41274;
}

namespace YimMenu::Features
{
    BoolCommand _ESPDrawPlayers("espdrawplayers", "Draw Players", "Should the ESP draw players?");
    BoolCommand _ESPDrawDeadPlayers("espdrawdeadplayers", "Draw Dead Players", "Should the ESP draw dead players?");
    BoolCommand _ESPName("espnameplayers", "Show Player Name", "Should the ESP draw player names?");
    BoolCommand _ESPDistance("espdistanceplayers", "Show Player Distance", "Should the ESP draw player distance?");
    BoolCommand _ESPSkeleton("espskeletonplayers", "Show Player Skeleton", "Should the ESP draw player skeletons?");

    ColorCommand _NameColorPlayers("namecolorplayers", "Player Name Color", "Changes the color of the name ESP for players", ImVec4{ 1.0f, 1.0f, 1.0f, 1.0f });
    ColorCommand _DistanceColorPlayers("distancecolorplayers", "Player Distance Color", "Changes the color of the distance ESP for players", ImVec4{ 1.0f, 1.0f, 1.0f, 1.0f });
    ColorCommand _SkeletonColorPlayers("skeletoncolorplayers", "Player Skeleton Color", "Changes the color of the skeleton ESP for players", ImVec4{ 1.0f, 1.0f, 1.0f, 1.0f });

    BoolCommand _ESPDrawPeds("espdrawpeds", "Draw Peds", "Should the ESP draw peds?");
    BoolCommand _ESPDrawDeadPeds("espdrawdeadpeds", "Draw Dead Peds", "Should the ESP draw dead peds?");
    BoolCommand _ESPModelPeds("espmodelspeds", "Show Ped Model", "Should the ESP draw ped models?");
    BoolCommand _ESPNetworkInfoPeds("espnetinfopeds", "Show Ped Network Info", "Should the ESP draw network info?");
    BoolCommand _ESPScriptInfoPeds("espscriptinfopeds", "Show Ped Script Info", "Should the ESP draw script info?");
    BoolCommand _ESPDistancePeds("espdistancepeds", "Show Ped Distance", "Should the ESP draw distance?");
    BoolCommand _ESPSkeletonPeds("espskeletonpeds", "Show Ped Skeleton", "Should the ESP draw the skeleton?");
    BoolCommand _ESPSkeletonHorse("espskeletonhorse", "Show Horse Skeleton", "Should the ESP draw horse skeletons?");

    ColorCommand _HashColorPeds("hashcolorpeds", "Ped Hash Color", "Changes the color of the hash ESP for peds", ImVec4{ 1.0f, 1.0f, 1.0f, 1.0f });
    ColorCommand _DistanceColorPeds("distancecolorpeds", "Ped Distance Color", "Changes the color of the distance ESP for peds", ImVec4{ 1.0f, 1.0f, 1.0f, 1.0f });
    ColorCommand _SkeletonColorPeds("skeletoncolorpeds", "Ped Skeleton Color", "Changes the color of the skeleton ESP for peds", ImVec4{ 1.0f, 1.0f, 1.0f, 1.0f });
    ColorCommand _SkeletonColorHorse("skeletoncolorhorse", "Horse Skeleton Color", "Changes the color of the skeleton ESP for horses", ImVec4{ 1.0f, 1.0f, 1.0f, 1.0f });
}

namespace YimMenu
{
    static ImVec4 Green  = ImVec4(0.29f, 0.69f, 0.34f, 1.f);
    static ImVec4 Orange = ImVec4(0.69f, 0.49f, 0.29f, 1.f);
    static ImVec4 Red    = ImVec4(0.69f, 0.29f, 0.29f, 1.f);
    static ImVec4 Blue   = ImVec4(0.36f, 0.71f, 0.89f, 1.f);

    ImColor ESP::GetColorBasedOnDistance(float distance)
    {
        if (distance < 100.f)
            return Green;
        if (distance < 300.f)
            return Orange;
        return Red;
    }

    void ESP::DrawTextInfo(ImDrawList* drawList, const ImVec2& position, const std::string& text, ImColor color)
    {
        drawList->AddText(position, color, text.c_str());
    }

    ImVec2 ESP::boneToScreen(rage::fvector3 bone)
    {
        float screenX{};
        float screenY{};
        float boneCoords[3] = { bone.x, bone.y, bone.z };
        Pointers.WorldToScreen(boneCoords, &screenX, &screenY);
        return ImVec2(screenX * (*Pointers.ScreenResX), screenY * (*Pointers.ScreenResY));
    }

    void ESP::DrawSkeleton(Ped ped, ImDrawList* drawList, ImColor color)
    {
        const auto headPos          = boneToScreen(ped.GetBonePosition(headBone));
        const auto neckPos          = boneToScreen(ped.GetBonePosition(neckBone));
        const auto torsoPos         = boneToScreen(ped.GetBonePosition(torsoBone));
        const auto leftShoulderPos  = boneToScreen(ped.GetBonePosition(leftShoulderBone));
        const auto rightShoulderPos = boneToScreen(ped.GetBonePosition(rightShoulderBone));
        const auto leftElbowPos     = boneToScreen(ped.GetBonePosition(leftElbowBone));
        const auto rightElbowPos    = boneToScreen(ped.GetBonePosition(rightElbowBone));
        const auto leftHandPos      = boneToScreen(ped.GetBonePosition(leftHandBone));
        const auto rightHandPos     = boneToScreen(ped.GetBonePosition(rightHandBone));
        const auto leftKneePos      = boneToScreen(ped.GetBonePosition(leftKneeBone));
        const auto rightKneePos     = boneToScreen(ped.GetBonePosition(rightKneeBone));
        const auto leftFootPos      = boneToScreen(ped.GetBonePosition(leftFootBone));
        const auto rightFootPos     = boneToScreen(ped.GetBonePosition(rightFootBone));

        drawList->AddLine(headPos, neckPos, color, 1.5f);
        drawList->AddLine(neckPos, leftShoulderPos, color, 1.5f);
        drawList->AddLine(leftShoulderPos, leftElbowPos, color, 1.5f);
        drawList->AddLine(leftElbowPos, leftHandPos, color, 1.5f);
        drawList->AddLine(neckPos, rightShoulderPos, color, 1.5f);
        drawList->AddLine(rightShoulderPos, rightElbowPos, color, 1.5f);
        drawList->AddLine(rightElbowPos, rightHandPos, color, 1.5f);
        drawList->AddLine(neckPos, torsoPos, color, 1.5f);
        drawList->AddLine(torsoPos, leftKneePos, color, 1.5f);
        drawList->AddLine(leftKneePos, leftFootPos, color, 1.5f);
        drawList->AddLine(torsoPos, rightKneePos, color, 1.5f);
        drawList->AddLine(rightKneePos, rightFootPos, color, 1.5f);
    }

    void ESP::DrawHorseSkeleton(Ped horse, ImDrawList* drawList, ImColor color)
    {
        const auto headPos   = boneToScreen(horse.GetBonePosition(horseHeadBone));
        const auto neckPos0  = boneToScreen(horse.GetBonePosition(horseNeckBone0));
        const auto torsoPos  = boneToScreen(horse.GetBonePosition(horseTorsoBone));
        const auto pelvisPos = boneToScreen(horse.GetBonePosition(horsePelvisBone));

        drawList->AddLine(headPos, neckPos0, color, 1.5f);
        drawList->AddLine(neckPos0, torsoPos, color, 1.5f);
        drawList->AddLine(torsoPos, pelvisPos, color, 1.5f);

        const auto frontLeftUpperArmPos = boneToScreen(horse.GetBonePosition(horseFrontLeftUpperArmBone));
        const auto frontLeftForearmPos  = boneToScreen(horse.GetBonePosition(horseFrontLeftForearmBone));
        const auto frontLeftHandPos     = boneToScreen(horse.GetBonePosition(horseFrontLeftHandBone));
        const auto frontLeftFinger0Pos  = boneToScreen(horse.GetBonePosition(horseFrontLeftFingerBone0));
        const auto frontLeftFinger1Pos  = boneToScreen(horse.GetBonePosition(horseFrontLeftFingerBone1));

        drawList->AddLine(neckPos0, frontLeftUpperArmPos, color, 1.5f);
        drawList->AddLine(frontLeftUpperArmPos, frontLeftForearmPos, color, 1.5f);
        drawList->AddLine(frontLeftForearmPos, frontLeftHandPos, color, 1.5f);
        drawList->AddLine(frontLeftHandPos, frontLeftFinger0Pos, color, 1.5f);
        drawList->AddLine(frontLeftFinger0Pos, frontLeftFinger1Pos, color, 1.5f);

        const auto frontRightUpperArmPos = boneToScreen(horse.GetBonePosition(horseFrontRightUpperArmBone));
        const auto frontRightForearmPos  = boneToScreen(horse.GetBonePosition(horseFrontRightForearmBone));
        const auto frontRightHandPos     = boneToScreen(horse.GetBonePosition(horseFrontRightHandBone));
        const auto frontRightFinger0Pos  = boneToScreen(horse.GetBonePosition(horseFrontRightFingerBone0));
        const auto frontRightFinger1Pos  = boneToScreen(horse.GetBonePosition(horseFrontRightFingerBone1));

        drawList->AddLine(neckPos0, frontRightUpperArmPos, color, 1.5f);
        drawList->AddLine(frontRightUpperArmPos, frontRightForearmPos, color, 1.5f);
        drawList->AddLine(frontRightForearmPos, frontRightHandPos, color, 1.5f);
        drawList->AddLine(frontRightHandPos, frontRightFinger0Pos, color, 1.5f);
        drawList->AddLine(frontRightFinger0Pos, frontRightFinger1Pos, color, 1.5f);

        const auto rearLeftThighPos = boneToScreen(horse.GetBonePosition(horseRearLeftThighBone));
        const auto rearLeftCalfPos  = boneToScreen(horse.GetBonePosition(horseRearLeftCalfBone));
        const auto rearLeftFootPos  = boneToScreen(horse.GetBonePosition(horseRearLeftFootBone));
        const auto rearLeftToe0Pos  = boneToScreen(horse.GetBonePosition(horseRearLeftToeBone0));
        const auto rearLeftToe1Pos  = boneToScreen(horse.GetBonePosition(horseRearLeftToeBone1));

        drawList->AddLine(pelvisPos, rearLeftThighPos, color, 1.5f);
        drawList->AddLine(rearLeftThighPos, rearLeftCalfPos, color, 1.5f);
        drawList->AddLine(rearLeftCalfPos, rearLeftFootPos, color, 1.5f);
        drawList->AddLine(rearLeftFootPos, rearLeftToe0Pos, color, 1.5f);
        drawList->AddLine(rearLeftToe0Pos, rearLeftToe1Pos, color, 1.5f);

        const auto rearRightThighPos = boneToScreen(horse.GetBonePosition(horseRearRightThighBone));
        const auto rearRightCalfPos  = boneToScreen(horse.GetBonePosition(horseRearRightCalfBone));
        const auto rearRightFootPos  = boneToScreen(horse.GetBonePosition(horseRearRightFootBone));
        const auto rearRightToe0Pos  = boneToScreen(horse.GetBonePosition(horseRearRightToeBone0));
        const auto rearRightToe1Pos  = boneToScreen(horse.GetBonePosition(horseRearRightToeBone1));

        drawList->AddLine(pelvisPos, rearRightThighPos, color, 1.5f);
        drawList->AddLine(rearRightThighPos, rearRightCalfPos, color, 1.5f);
        drawList->AddLine(rearRightCalfPos, rearRightFootPos, color, 1.5f);
        drawList->AddLine(rearRightFootPos, rearRightToe0Pos, color, 1.5f);
        drawList->AddLine(rearRightToe0Pos, rearRightToe1Pos, color, 1.5f);
    }

    void ESP::DrawPlayer(Player& plyr, ImDrawList* drawList, const rage::fvector3& selfPosition, Ped selfPed)
    {
        if (!plyr.IsValid())
            return;

        const Ped ped = plyr.GetPed();
        if (!ped.IsValid() || ped == selfPed || (ped.IsDead() && !Features::_ESPDrawDeadPlayers.GetState()))
            return;

        const auto torsoWorld = ped.GetBonePosition(torsoBone);
        const auto torsoScreen = boneToScreen(torsoWorld);
        if (torsoScreen.x == 0.0f)
            return;

        const float distanceToPlayer = selfPosition.GetDistance(torsoWorld);
        const bool showName = Features::_ESPName.GetState();
        const bool showDistance = Features::_ESPDistance.GetState();

        if (showName || showDistance)
        {
            const auto headPos = boneToScreen(ped.GetBonePosition(headBone));
            auto* currentFont = ImGui::GetFont();
            const auto originalFontSize = currentFont->Scale;
            currentFont->Scale *= 1.2f;
            ImGui::PushFont(currentFont);

            if (showName)
                DrawTextInfo(drawList, headPos, plyr.GetName(), plyr == Players::GetSelected() ? Blue : Features::_NameColorPlayers.GetState());

            if (showDistance)
            {
                const std::string distanceStr = std::to_string(static_cast<int>(distanceToPlayer)) + "m";
                DrawTextInfo(drawList, { headPos.x, headPos.y + 20.0f }, distanceStr, GetColorBasedOnDistance(distanceToPlayer));
            }

            currentFont->Scale = originalFontSize;
            ImGui::PopFont();
        }

        if (Features::_ESPSkeleton.GetState() && !ped.IsAnimal() && distanceToPlayer < 250.f)
            DrawSkeleton(ped, drawList, ImGui::ColorConvertFloat4ToU32(Features::_SkeletonColorPlayers.GetState()));
    }

    void ESP::DrawPeds(Ped ped, ImDrawList* drawList, const rage::fvector3& selfPosition, Ped selfPed)
    {
        if (!ped.IsValid() || ped.IsPlayer() || ped == selfPed || (ped.IsDead() && !Features::_ESPDrawDeadPeds.GetState()))
            return;

        const auto torsoWorld = ped.GetBonePosition(torsoBone);
        const auto torsoScreen = boneToScreen(torsoWorld);
        if (torsoScreen.x == 0.0f)
            return;

        const float distanceToPed = selfPosition.GetDistance(torsoWorld);
        const bool showModel = Features::_ESPModelPeds.GetState();
        const bool showNetwork = Features::_ESPNetworkInfoPeds.GetState();
        const bool showScript = Features::_ESPScriptInfoPeds.GetState();
        const bool showDistance = Features::_ESPDistancePeds.GetState();

        std::string info;
        if (showModel)
        {
            const auto model = ped.GetModel();
            if (auto it = Data::g_PedModels.find(model); it != Data::g_PedModels.end())
                info += std::format("{} (0x{:08X}) ", Data::GetPedDisplayName(it->second), static_cast<joaat_t>(model));
            else
                info += std::format("0x{:08X} ", static_cast<joaat_t>(model));
        }

        if (showNetwork && ped.IsNetworked())
        {
            const auto owner = Player(ped.GetOwner());
            info += std::format("{} {} ", ped.GetNetworkObjectId(), owner.GetName());
        }

        if (showScript)
        {
            if (auto script = ENTITY::_GET_ENTITY_SCRIPT(ped.GetHandle(), nullptr))
                if (auto name = Scripts::GetScriptName(script))
                    info += std::format("{} ", name);
        }

        if (!info.empty() || showDistance)
        {
            const auto headPos = boneToScreen(ped.GetBonePosition(headBone));
            auto* currentFont = ImGui::GetFont();
            const auto originalFontSize = currentFont->Scale;
            currentFont->Scale *= 1.2f;
            ImGui::PushFont(currentFont);

            if (!info.empty())
                DrawTextInfo(drawList, headPos, info, ImGui::ColorConvertFloat4ToU32(Features::_HashColorPeds.GetState()));

            if (showDistance)
            {
                const std::string distanceStr = std::to_string(static_cast<int>(distanceToPed)) + "m";
                DrawTextInfo(drawList, { headPos.x, headPos.y + 20.0f }, distanceStr, GetColorBasedOnDistance(distanceToPed));
            }

            currentFont->Scale = originalFontSize;
            ImGui::PopFont();
        }

        if (Features::_ESPSkeletonPeds.GetState() && !ped.IsAnimal() && distanceToPed < 250.f)
            DrawSkeleton(ped, drawList, ImGui::ColorConvertFloat4ToU32(Features::_SkeletonColorPeds.GetState()));

        if (Features::_ESPSkeletonHorse.GetState() && PED::_IS_THIS_MODEL_A_HORSE(ped.GetModel()) && distanceToPed < 250.f)
            DrawHorseSkeleton(ped, drawList, ImGui::ColorConvertFloat4ToU32(Features::_SkeletonColorHorse.GetState()));
    }

    void ESP::Draw()
    {
        const bool drawPlayers = Features::_ESPDrawPlayers.GetState();
        const bool drawPeds = Features::_ESPDrawPeds.GetState();
        if ((!drawPlayers && !drawPeds) || !NativeInvoker::AreHandlersCached() || CAM::IS_SCREEN_FADED_OUT())
            return;

        auto* drawList = ImGui::GetBackgroundDrawList();
        if (!drawList)
            return;

        const Ped selfPed = Self::GetPed();
        if (!selfPed.IsValid())
            return;
        const auto selfPosition = selfPed.GetPosition();

        if (drawPlayers)
        {
            for (auto& [id, player] : Players::GetPlayers())
                DrawPlayer(player, drawList, selfPosition, selfPed);
        }

        if (drawPeds && GetPedPool())
        {
            for (Ped ped : Pools::GetPeds())
            {
                if (ped.IsValid() && ped.GetPointer<void*>())
                    DrawPeds(ped, drawList, selfPosition, selfPed);
            }
        }
    }
}
