// Lightweight lifecycle/loot support for manually spawned clones.
// Included by ManualClone.cpp inside its anonymous namespace so it can reuse the
// existing managed-clone registry without adding another PED-pool scan.

struct CloneLifeLootState
{
    int Clone{};
    CloneMode Mode{CloneMode::Bodyguard};
    bool ProfileApplied{};
    bool WasIncapacitated{};
    bool LootGranted{};
    bool CorpsePromptsRestored{};
    int AmmoVariant{};
    int TonicCount{10};
    int CloseLootTicks{};
    int LastHorseAppearance{};
    Clock::time_point IncapacitatedAt{};
};

constexpr int kBodyguardIncapacitationHealth = 50;
constexpr int kHostileIncapacitationHealth = 20;
constexpr int kCloneBleedoutMilliseconds = 15000;
constexpr float kCloneBleedoutSeconds = 15.0f;
constexpr int kRareLootAmmoAmount = 100;
constexpr int kDisableAmbientPromptsFlag = 129;
constexpr float kCloneLootInteractionRadius = 3.25f;
constexpr float kCloneLootFallbackRadius = 1.65f;

std::array<CloneLifeLootState, kMaxActiveClones> g_CloneLifeLoot{};

CloneLifeLootState& GetCloneLifeLootState(int clone, CloneMode mode)
{
    for (auto& state : g_CloneLifeLoot)
    {
        if (state.Clone == clone)
        {
            state.Mode = mode;
            return state;
        }
    }

    for (auto& state : g_CloneLifeLoot)
    {
        if (!state.Clone || !FindManagedClone(state.Clone) || !ENTITY::DOES_ENTITY_EXIST(state.Clone))
        {
            state = {};
            state.Clone = clone;
            state.Mode = mode;
            state.AmmoVariant = RandomInt(0, 4);
            state.TonicCount = RandomInt(0, 1) == 0 ? 10 : 30;
            return state;
        }
    }

    g_CloneLifeLoot[0] = {};
    g_CloneLifeLoot[0].Clone = clone;
    g_CloneLifeLoot[0].Mode = mode;
    g_CloneLifeLoot[0].AmmoVariant = RandomInt(0, 4);
    g_CloneLifeLoot[0].TonicCount = RandomInt(0, 1) == 0 ? 10 : 30;
    return g_CloneLifeLoot[0];
}

Hash RareLootAmmoHash(int variant)
{
    switch (variant)
    {
    case 0: return "AMMO_RIFLE_EXPRESS"_J;
    case 1: return "AMMO_SHOTGUN_SLUG"_J;
    case 2: return "AMMO_PISTOL_EXPRESS"_J;
    case 3: return "AMMO_REVOLVER_EXPRESS"_J;
    default: return "AMMO_REPEATER_EXPRESS"_J;
    }
}

const char* RareLootAmmoNamePt(int variant)
{
    switch (variant)
    {
    case 0: return "munição expressa de rifle/sniper";
    case 1: return "munição slug de escopeta";
    case 2: return "munição expressa de pistola";
    case 3: return "munição expressa de revólver";
    default: return "munição expressa de repetidora";
    }
}

void ApplyCloneLifeProfile(int clone, CloneMode mode)
{
    if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone) || ENTITY::IS_ENTITY_DEAD(clone))
        return;

    const int threshold = mode == CloneMode::Bodyguard ? kBodyguardIncapacitationHealth : kHostileIncapacitationHealth;
    PED::SET_PED_CAN_BE_INCAPACITATED(clone, true);
    PED::_SET_PED_INCAPACITATION_MODIFIERS(clone, true, threshold, kCloneBleedoutMilliseconds, 0);
    PED::_SET_PED_INCAPACITATION_TOTAL_BLEED_OUT_DURATION(clone, kCloneBleedoutSeconds);
    PED::_SET_PED_WRITHING_DURATION(clone, 10.0f, kCloneBleedoutSeconds, 0);
    PED::SET_PAUSE_PED_WRITHE_BLEEDOUT(clone, false);
    PED::SET_PED_CONFIG_FLAG(clone, kDisableAmbientPromptsFlag, false);

    PED::SET_LOOTING_FLAG(clone, 0, true);
    PED::SET_LOOTING_FLAG(clone, 1, true);
    PED::SET_LOOTING_FLAG(clone, 15, true);
}

bool IsOwnerInteractingWithCloneLoot(int owner, int clone)
{
    if (!owner || !clone || !ENTITY::DOES_ENTITY_EXIST(owner) || !ENTITY::DOES_ENTITY_EXIST(clone) ||
        ENTITY::IS_ENTITY_DEAD(owner) || !ENTITY::IS_ENTITY_DEAD(clone))
        return false;

    const Vector3 ownerPos = ENTITY::GET_ENTITY_COORDS(owner, true, false);
    const Vector3 corpsePos = ENTITY::GET_ENTITY_COORDS(clone, true, false);
    if (DistanceSquared(ownerPos, corpsePos) > kCloneLootInteractionRadius * kCloneLootInteractionRadius)
        return false;
    if (!ENTITY::HAS_ENTITY_CLEAR_LOS_TO_ENTITY(owner, clone, 17))
        return false;

    const auto pressed = [](NativeInputs input) {
        const int control = static_cast<int>(input);
        return PAD::IS_CONTROL_PRESSED(0, control) || PAD::IS_DISABLED_CONTROL_PRESSED(0, control);
    };
    return pressed(NativeInputs::INPUT_LOOT) || pressed(NativeInputs::INPUT_LOOT2) ||
           pressed(NativeInputs::INPUT_LOOT3) || pressed(NativeInputs::INPUT_LOOT_AMMO);
}

void GrantCloneRareLoot(int owner, int ammoVariant, int tonicCount)
{
    if (!owner || !ENTITY::DOES_ENTITY_EXIST(owner) || ENTITY::IS_ENTITY_DEAD(owner))
        return;

    const Hash ammoHash = RareLootAmmoHash(ammoVariant);
    // Normal corpse loot can collapse to tiny pickups (for example six rounds).
    // This uses the same proven fill path as the project's GiveAllAmmo feature;
    // RDR2 clamps the result to the player's capacity for the selected ammo type.
    WEAPON::_ADD_AMMO_TO_PED_BY_TYPE(owner, ammoHash, 9999, 0x2CD419DC);

    const Rewards::RewardInfo miracleTonic{0, "CONSUMABLE_TONIC"_J, "Miracle Tonic"};
    for (int i = 0; i < tonicCount; ++i)
    {
        Rewards::GiveScriptReward(miracleTonic, false);
        if ((i + 1) % 5 == 0)
            ScriptMgr::Yield(1ms);
    }

    std::string message = "Saque raro: pacote 100x ";
    message += RareLootAmmoNamePt(ammoVariant);
    message += " + ";
    message += std::to_string(tonicCount);
    message += "x Tônico Milagroso";
    Notifications::Show("Tenebris", message, NotificationType::Success, 2800);
}

void SyncCloneLifeAndLoot(int owner, Clock::time_point now)
{
    for (auto& state : g_CloneLifeLoot)
    {
        if (state.Clone && (!FindManagedClone(state.Clone) || !ENTITY::DOES_ENTITY_EXIST(state.Clone)))
            state = {};
    }

    for (const auto& managed : g_ManagedClones)
    {
        const int clone = managed.Ped;
        if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone))
            continue;

        auto& state = GetCloneLifeLootState(clone, managed.Mode);

        if (!state.ProfileApplied && !ENTITY::IS_ENTITY_DEAD(clone))
        {
            ApplyCloneLifeProfile(clone, managed.Mode);
            state.ProfileApplied = true;
        }

        if (managed.Mode == CloneMode::Bodyguard && !ENTITY::IS_ENTITY_DEAD(clone))
        {
            auto& support = GetCloneSupportState(clone);
            if (support.Horse && ENTITY::DOES_ENTITY_EXIST(support.Horse))
            {
                // Newly created RDR2 horses may not render until a variation is
                // initialized. Do this once per assigned horse, then keep the cheap
                // visibility repair handled by the existing 900ms supervisor.
                if (state.LastHorseAppearance != support.Horse)
                {
                    PED::_SET_RANDOM_OUTFIT_VARIATION(support.Horse, true);
                    ENTITY::PLACE_ENTITY_ON_GROUND_PROPERLY(support.Horse, true);
                    state.LastHorseAppearance = support.Horse;
                }
                EnsureSupportHorseVisible(support.Horse);
            }
            else
            {
                state.LastHorseAppearance = 0;
            }
        }

        if (ENTITY::IS_ENTITY_DEAD(clone))
        {
            if (!state.CorpsePromptsRestored)
            {
                PED::SET_PED_CONFIG_FLAG(clone, kDisableAmbientPromptsFlag, false);
                state.CorpsePromptsRestored = true;
            }

            bool shouldGrant = !state.LootGranted && IsOwnerInteractingWithCloneLoot(owner, clone);
            if (!shouldGrant && !state.LootGranted && owner && ENTITY::DOES_ENTITY_EXIST(owner) && !ENTITY::IS_ENTITY_DEAD(owner))
            {
                const float distanceSq = DistanceSquared(
                    ENTITY::GET_ENTITY_COORDS(owner, true, false),
                    ENTITY::GET_ENTITY_COORDS(clone, true, false));
                if (distanceSq <= kCloneLootFallbackRadius * kCloneLootFallbackRadius &&
                    ENTITY::HAS_ENTITY_CLEAR_LOS_TO_ENTITY(owner, clone, 17))
                    ++state.CloseLootTicks;
                else
                    state.CloseLootTicks = 0;
                shouldGrant = state.CloseLootTicks >= 2;
            }

            if (shouldGrant)
            {
                state.LootGranted = true;
                state.CloseLootTicks = 0;
                const int ammoVariant = state.AmmoVariant;
                const int tonicCount = state.TonicCount;
                FiberPool::Push([owner, ammoVariant, tonicCount] {
                    GrantCloneRareLoot(owner, ammoVariant, tonicCount);
                });
            }
            continue;
        }

        state.CloseLootTicks = 0;
        const bool incapacitated = PED::IS_PED_INCAPACITATED(clone);
        if (incapacitated)
        {
            if (!state.WasIncapacitated)
            {
                state.WasIncapacitated = true;
                state.IncapacitatedAt = now;
                PED::SET_PED_CONFIG_FLAG(clone, kDisableAmbientPromptsFlag, managed.Mode != CloneMode::Bodyguard);
            }

            PED::SET_PAUSE_PED_WRITHE_BLEEDOUT(clone, false);
            if (state.IncapacitatedAt != Clock::time_point{} && now - state.IncapacitatedAt >= 15s)
                ENTITY::SET_ENTITY_HEALTH(clone, 0, 0);
            continue;
        }

        if (!state.WasIncapacitated)
            continue;

        if (managed.Mode == CloneMode::Bodyguard)
        {
            state.WasIncapacitated = false;
            state.IncapacitatedAt = {};
            PED::SET_PED_CONFIG_FLAG(clone, kDisableAmbientPromptsFlag, false);
            const int maxHealth = ENTITY::GET_ENTITY_MAX_HEALTH(clone, false);
            const int safeHealth = std::min(maxHealth, std::max(150, kBodyguardIncapacitationHealth + 100));
            if (ENTITY::GET_ENTITY_HEALTH(clone) < safeHealth)
                ENTITY::SET_ENTITY_HEALTH(clone, safeHealth, owner);
        }
        else
        {
            ENTITY::SET_ENTITY_HEALTH(clone, 0, 0);
        }
    }
}
