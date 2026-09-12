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
    int AmmoVariant{};
    int TonicCount{10};
    Clock::time_point IncapacitatedAt{};
};

constexpr int kBodyguardIncapacitationHealth = 50;
constexpr int kHostileIncapacitationHealth = 20;
constexpr int kCloneBleedoutMilliseconds = 15000;
constexpr float kCloneBleedoutSeconds = 15.0f;
constexpr int kRareLootAmmoAmount = 100;

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

    // kMaxActiveClones bounds the live registry, so this is only a defensive
    // fallback for stale slots that survived an abnormal entity cleanup.
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
    case 0: return "AMMO_RIFLE_EXPRESS"_J;       // rifles + sniper rifles
    case 1: return "AMMO_SHOTGUN_SLUG"_J;        // rare shotgun load
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

    // Keep both clone classes lootable, but do not create a second inventory or
    // per-frame reward system. The actual rare package is granted once when the
    // local player starts looting the corpse.
    PED::SET_LOOTING_FLAG(clone, 0, true);
    PED::SET_LOOTING_FLAG(clone, 1, true);
    PED::SET_LOOTING_FLAG(clone, 15, true);
}

void GrantCloneRareLoot(int owner, int ammoVariant, int tonicCount)
{
    if (!owner || !ENTITY::DOES_ENTITY_EXIST(owner) || ENTITY::IS_ENTITY_DEAD(owner))
        return;

    const Hash ammoHash = RareLootAmmoHash(ammoVariant);
    WEAPON::_ADD_AMMO_TO_PED_BY_TYPE(owner, ammoHash, kRareLootAmmoAmount, 0x2CD419DC);

    // Reuse the project's existing scripted-item reward path. This work runs in
    // its own fiber and yields in small batches so 30 tonics never stall the
    // shared clone/horse/radar controller.
    const Rewards::RewardInfo miracleTonic{0, "CONSUMABLE_TONIC"_J, "Miracle Tonic"};
    for (int i = 0; i < tonicCount; ++i)
    {
        Rewards::GiveScriptReward(miracleTonic, false);
        if ((i + 1) % 5 == 0)
            ScriptMgr::Yield(1ms);
    }

    std::string message = "Saque raro: 100x ";
    message += RareLootAmmoNamePt(ammoVariant);
    message += " + ";
    message += std::to_string(tonicCount);
    message += "x Tônico Milagroso";
    Notifications::Show("Tenebris", message, NotificationType::Success, 2800);
}

void SyncCloneLifeAndLoot(int owner, Clock::time_point now)
{
    // Clear only slots whose managed clone no longer exists. Dead managed clones
    // deliberately keep their slot while their corpse is available for looting.
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

        if (ENTITY::IS_ENTITY_DEAD(clone))
        {
            if (!state.LootGranted && owner && ENTITY::DOES_ENTITY_EXIST(owner) && ENTITY::_FIND_ENTITY_LOOTING_PED(clone) == owner)
            {
                state.LootGranted = true;
                const int ammoVariant = state.AmmoVariant;
                const int tonicCount = state.TonicCount;
                FiberPool::Push([owner, ammoVariant, tonicCount] {
                    GrantCloneRareLoot(owner, ammoVariant, tonicCount);
                });
            }
            continue;
        }

        const bool incapacitated = PED::IS_PED_INCAPACITATED(clone);
        if (incapacitated)
        {
            if (!state.WasIncapacitated)
            {
                state.WasIncapacitated = true;
                state.IncapacitatedAt = now;
            }

            PED::SET_PAUSE_PED_WRITHE_BLEEDOUT(clone, false);

            // Native bleedout is configured to 15 seconds, and this deadline is a
            // deterministic backstop so a stuck writhe task cannot make a clone
            // remain incapacitated forever.
            if (state.IncapacitatedAt != Clock::time_point{} && now - state.IncapacitatedAt >= 15s)
                ENTITY::SET_ENTITY_HEALTH(clone, 0, 0);
            continue;
        }

        if (!state.WasIncapacitated)
            continue;

        if (managed.Mode == CloneMode::Bodyguard)
        {
            // Leaving incapacitation means the game's allied/bodyguard revive flow
            // succeeded. Give enough recovery health to avoid instantly dropping
            // again at the 50 HP threshold.
            state.WasIncapacitated = false;
            state.IncapacitatedAt = {};
            const int maxHealth = ENTITY::GET_ENTITY_MAX_HEALTH(clone, false);
            const int safeHealth = std::min(maxHealth, std::max(150, kBodyguardIncapacitationHealth + 100));
            if (ENTITY::GET_ENTITY_HEALTH(clone) < safeHealth)
                ENTITY::SET_ENTITY_HEALTH(clone, safeHealth, owner);
        }
        else
        {
            // Hostile clones deliberately have no Tenebris revive path. If some
            // external/game interaction pulls one out of incapacitation before its
            // deadline, it cannot be converted into a saved hostile clone.
            ENTITY::SET_ENTITY_HEALTH(clone, 0, 0);
        }
    }
}
