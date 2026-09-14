// Additional runtime layer for active manual clones.
// Runs on one low-frequency supervisor and reuses the managed-clone/PED caches.
// Runtime state grows with the managed clone list; there is no Tenebris-side
// active-clone tracking ceiling here.

struct CloneRuntimeState
{
	int Clone{};
	bool ProfileApplied{};
	Vector3 LastIdlePosition{};
	int StillChecks{};
	int DamageTarget{};
	int LastTargetHealth{};
	Clock::time_point NextMovementCheck{};
	Clock::time_point NextDamageCheck{};
	Clock::time_point NextHorseVisibilityCheck{};
};

std::vector<CloneRuntimeState> g_CloneRuntime{};
bool g_CloneSupervisorRunning{};
bool g_CloneRelationshipsReady{};
Hash g_HostileCloneRelationship{};
Hash g_BodyguardCloneRelationship{};

CloneRuntimeState& GetCloneRuntimeState(int clone)
{
	for (auto& state : g_CloneRuntime)
		if (state.Clone == clone)
			return state;
	for (auto& state : g_CloneRuntime)
	{
		if (!state.Clone || !FindManagedClone(state.Clone) || !ENTITY::DOES_ENTITY_EXIST(state.Clone) || ENTITY::IS_ENTITY_DEAD(state.Clone))
		{
			state = {};
			state.Clone = clone;
			state.NextMovementCheck = Clock::now() + 1700ms;
			return state;
		}
	}
	g_CloneRuntime.emplace_back();
	auto& state = g_CloneRuntime.back();
	state.Clone = clone;
	state.NextMovementCheck = Clock::now() + 1700ms;
	return state;
}

void ResetCloneRuntimeState()
{
	g_CloneRuntime.clear();
}

void EnsureCloneRelationships(int owner)
{
	if (!g_CloneRelationshipsReady)
	{
		const bool hostileOk = PED::ADD_RELATIONSHIP_GROUP("TENEBRIS_HOSTILE", &g_HostileCloneRelationship);
		const bool guardOk = PED::ADD_RELATIONSHIP_GROUP("TENEBRIS_GUARD", &g_BodyguardCloneRelationship);
		g_CloneRelationshipsReady = hostileOk && guardOk;
	}
	if (!g_CloneRelationshipsReady)
		return;

	const Hash law = Joaat("REL_COP");
	PED::SET_RELATIONSHIP_BETWEEN_GROUPS(1, g_HostileCloneRelationship, g_HostileCloneRelationship);
	PED::SET_RELATIONSHIP_BETWEEN_GROUPS(1, g_BodyguardCloneRelationship, g_BodyguardCloneRelationship);
	PED::SET_RELATIONSHIP_BETWEEN_GROUPS(5, g_HostileCloneRelationship, g_BodyguardCloneRelationship);
	PED::SET_RELATIONSHIP_BETWEEN_GROUPS(5, g_BodyguardCloneRelationship, g_HostileCloneRelationship);
	PED::SET_RELATIONSHIP_BETWEEN_GROUPS(5, g_HostileCloneRelationship, law);
	PED::SET_RELATIONSHIP_BETWEEN_GROUPS(5, law, g_HostileCloneRelationship);
	PED::SET_RELATIONSHIP_BETWEEN_GROUPS(3, g_BodyguardCloneRelationship, law);
	PED::SET_RELATIONSHIP_BETWEEN_GROUPS(3, law, g_BodyguardCloneRelationship);
	if (owner && ENTITY::DOES_ENTITY_EXIST(owner))
	{
		const Hash ownerGroup = PED::GET_PED_RELATIONSHIP_GROUP_HASH(owner);
		PED::SET_RELATIONSHIP_BETWEEN_GROUPS(1, g_BodyguardCloneRelationship, ownerGroup);
		PED::SET_RELATIONSHIP_BETWEEN_GROUPS(1, ownerGroup, g_BodyguardCloneRelationship);
	}
}

void ApplyRuntimeCloneProfile(const ManagedClone& managed, CloneRuntimeState& state, int owner)
{
	const int clone = managed.Ped;
	if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone) || ENTITY::IS_ENTITY_DEAD(clone))
		return;
	EnsureCloneRelationships(owner);
	if (g_CloneRelationshipsReady)
	{
		PED::SET_PED_RELATIONSHIP_GROUP_HASH(clone, managed.Mode == CloneMode::Bodyguard ? g_BodyguardCloneRelationship : g_HostileCloneRelationship);
		PED::SET_PED_CONFIG_FLAG(clone, static_cast<int>(PedConfigFlag::CanAttackFriendly), false);
	}
	ApplyProfessionalCombatProfile(clone, managed.Weapon);
	state.ProfileApplied = true;
}

void MaintainHostileNaturalMovement(const ManagedClone& managed, CloneRuntimeState& state, Clock::time_point now)
{
	const int clone = managed.Ped;
	if (!clone || managed.Mode == CloneMode::Bodyguard || !ENTITY::DOES_ENTITY_EXIST(clone) ||
		ENTITY::IS_ENTITY_DEAD(clone) || PED::IS_PED_INCAPACITATED(clone))
		return;

	const int target = static_cast<int>(PED::GET_CURRENT_TARGET_FOR_PED(clone));
	if (target && IsHostileCloneAlly(clone, target))
	{
		TASK::CLEAR_PED_TASKS(clone, true, false);
		TASK::TASK_WANDER_STANDARD(clone, 10.0f, 10);
		state.StillChecks = 0;
		state.LastIdlePosition = ENTITY::GET_ENTITY_COORDS(clone, true, false);
		state.NextMovementCheck = now + 1700ms;
		return;
	}

	const bool inCombat = (target && IsAnyLiveDamageablePedTarget(target, clone)) || PED::IS_PED_IN_COMBAT(clone, 0);
	if (inCombat)
	{
		state.StillChecks = 0;
		state.LastIdlePosition = ENTITY::GET_ENTITY_COORDS(clone, true, false);
		return;
	}
	if (now < state.NextMovementCheck)
		return;

	state.NextMovementCheck = now + 1700ms;
	const Vector3 pos = ENTITY::GET_ENTITY_COORDS(clone, true, false);
	const float movedSq = DistanceSquared(pos, state.LastIdlePosition);
	state.LastIdlePosition = pos;
	if (ENTITY::GET_ENTITY_SPEED(clone) < 0.18f && movedSq < 1.5f * 1.5f)
		++state.StillChecks;
	else
		state.StillChecks = 0;
	if (state.StillChecks >= 2)
	{
		TASK::CLEAR_PED_TASKS(clone, true, false);
		TASK::TASK_WANDER_STANDARD(clone, 10.0f, 10);
		state.StillChecks = 0;
	}
}

void MaintainCloneDamageMultiplier(const ManagedClone& managed, CloneRuntimeState& state, Clock::time_point now)
{
	if (g_CloneDamagePercent <= 100 || now < state.NextDamageCheck)
		return;
	state.NextDamageCheck = now + 120ms;
	const int clone = managed.Ped;
	if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone) || ENTITY::IS_ENTITY_DEAD(clone))
		return;

	const int target = static_cast<int>(PED::GET_CURRENT_TARGET_FOR_PED(clone));
	if (!IsAnyLiveDamageablePedTarget(target, clone))
	{
		state.DamageTarget = 0;
		state.LastTargetHealth = 0;
		return;
	}
	const int health = ENTITY::GET_ENTITY_HEALTH(target);
	if (state.DamageTarget != target)
	{
		state.DamageTarget = target;
		state.LastTargetHealth = health;
		return;
	}
	if (health >= state.LastTargetHealth)
	{
		state.LastTargetHealth = health;
		return;
	}

	const int observedDamage = state.LastTargetHealth - health;
	if (observedDamage > 0 && ENTITY::HAS_ENTITY_BEEN_DAMAGED_BY_ENTITY(target, clone, true, true))
	{
		const float extraScale = static_cast<float>(g_CloneDamagePercent - 100) / 100.0f;
		int extra = static_cast<int>(std::lround(static_cast<float>(observedDamage) * extraScale));
		if (IsBowWeaponHash(managed.Weapon))
			extra = std::max(extra, 1);
		if (extra > 0)
			PED::APPLY_DAMAGE_TO_PED(target, extra, 0, kCloneHeadBone, clone);
	}
	state.LastTargetHealth = ENTITY::GET_ENTITY_HEALTH(target);
}

void MaintainAssignedHorseVisibility(int clone, CloneRuntimeState& state, Clock::time_point now)
{
	if (now < state.NextHorseVisibilityCheck)
		return;
	state.NextHorseVisibilityCheck = now + 900ms;
	for (auto& support : g_CloneSupport)
	{
		if (support.Clone != clone || !support.Horse || !ENTITY::DOES_ENTITY_EXIST(support.Horse))
			continue;
		EnsureSupportHorseVisible(support.Horse);
		break;
	}
}

void SetBodyguardsDismissed(bool dismissed)
{
	g_BodyguardsDismissed = dismissed;
	g_BodyguardThreatCache = {};
	for (const auto& managed : g_ManagedClones)
	{
		if (managed.Mode != CloneMode::Bodyguard || !managed.Ped || !ENTITY::DOES_ENTITY_EXIST(managed.Ped) || ENTITY::IS_ENTITY_DEAD(managed.Ped))
			continue;
		TASK::CLEAR_PED_TASKS(managed.Ped, true, false);
		auto& support = GetCloneSupportState(managed.Ped);
		support.NextHorseAttempt = {};
		support.MountTaskUntil = {};
		if (dismissed)
		{
			const int anchor = GetBodyguardAnchor(managed.Ped, PLAYER::PLAYER_PED_ID());
			ForceGuardMount(managed.Ped, anchor, true);
		}
	}
	Notifications::Show("Tenebris",
		dismissed ? (Localization::IsPortuguese() ? "Guarda-costas dispensados: retornando aos jogadores originais." : "Bodyguards dismissed: returning to their source players.")
		          : (Localization::IsPortuguese() ? "Guarda-costas convocados novamente." : "Bodyguards recalled."),
		NotificationType::Info, 2600);
}

void StartCloneEnhancementSupervisor()
{
	if (g_CloneSupervisorRunning)
		return;
	g_CloneSupervisorRunning = true;
	FiberPool::Push([] {
		while (g_CloneSupervisorRunning)
		{
			if (g_ManagedClones.empty())
			{
				ScriptMgr::Yield(500ms);
				continue;
			}

			const auto now = Clock::now();
			const int owner = PLAYER::PLAYER_PED_ID();
			if (owner && ENTITY::DOES_ENTITY_EXIST(owner))
				EnsureCloneRelationships(owner);

			for (auto& state : g_CloneRuntime)
			{
				if (state.Clone && (!FindManagedClone(state.Clone) || !ENTITY::DOES_ENTITY_EXIST(state.Clone) || ENTITY::IS_ENTITY_DEAD(state.Clone)))
					state = {};
			}
			for (auto& state : g_AggressivePerception)
			{
				if (state.Clone && (!FindManagedClone(state.Clone) || !ENTITY::DOES_ENTITY_EXIST(state.Clone) || ENTITY::IS_ENTITY_DEAD(state.Clone)))
					state = {};
			}

			for (const auto& managed : g_ManagedClones)
			{
				if (!managed.Ped || !ENTITY::DOES_ENTITY_EXIST(managed.Ped) || ENTITY::IS_ENTITY_DEAD(managed.Ped))
					continue;
				auto& state = GetCloneRuntimeState(managed.Ped);
				if (!state.ProfileApplied)
					ApplyRuntimeCloneProfile(managed, state, owner);
				MaintainAssignedHorseVisibility(managed.Ped, state, now);
				MaintainHostileNaturalMovement(managed, state, now);
				MaintainCloneDamageMultiplier(managed, state, now);
			}
			ScriptMgr::Yield(240ms);
		}
	});
}

void ClearActiveCloneExtensionState()
{
	ResetCloneRuntimeState();
	g_BodyguardsDismissed = false;
	g_BodyguardThreatCache = {};
}

// The legacy core still contains an eight-clone guard in SpawnManualCloneAt.
// The active optimized editor deliberately bypasses that guard. The game itself
// may still refuse PED creation when its own engine/network budgets are exhausted.
void SpawnManualCloneAtUnlimited(CloneOptions options, const Vector3& spawn, float heading)
{
	PruneManagedClones(false);
	auto self = Self::GetPed();
	if (!self.IsValid() || self.GetHealth() <= 0)
		return;
	const int owner = self.GetHandle();
	if (options.SourcePlayerId < 0)
		options.SourcePlayerId = PLAYER::PLAYER_ID();
	options.Health = std::clamp(options.Health, 50, 5000);
	options.SeeingRange = std::clamp(options.SeeingRange, 20.0f, 500.0f);
	options.WalkStyleIndex = std::clamp(options.WalkStyleIndex, 0, static_cast<int>(kCloneWalkStyles.size()) - 1);

	const int sourcePed = ResolveSourcePed(options.SourcePlayerId);
	if (!sourcePed)
	{
		Notifications::Show("Tenebris", Localization::IsPortuguese() ? "O jogador escolhido não está mais disponível na sessão." : "The selected player is no longer available in the session.", NotificationType::Warning, 2600);
		return;
	}
	int clone = CreateCloneShell(options.SourcePlayerId, sourcePed, spawn, heading, options.Networked);
	if (!clone)
	{
		Notifications::Show("Tenebris", Localization::IsPortuguese() ? "Não foi possível criar o clone com segurança." : "Could not create the clone safely.", NotificationType::Warning, 2600);
		return;
	}

	ENTITY::PLACE_ENTITY_ON_GROUND_PROPERLY(clone, true);
	ENTITY::SET_ENTITY_MAX_HEALTH(clone, options.Health);
	ENTITY::SET_ENTITY_HEALTH(clone, options.Health, 0);
	ApplyPlayerPromptName(clone, options.SourcePlayerId);
	ConfigureBleedout(clone);
	ConfigureCloneLoot(clone);
	ApplyCloneWalkStyle(clone, options.WalkStyleIndex);
	options.WeaponIndex = std::clamp(options.WeaponIndex, 0, static_cast<int>(kCloneWeapons.size()) - 1);
	const Hash weapon = EquipCloneWeapon(clone, options.WeaponIndex);
	ScriptMgr::Yield();
	StableEnsureCloneWeaponEquipped(clone, weapon);
	ConfigureCloneCombat(clone, options.Mode, kCloneWeapons[options.WeaponIndex].HashName, options.SeeingRange);
	ENTITY::CLEAR_ENTITY_LAST_DAMAGE_ENTITY(clone);

	g_ManagedClones.push_back({clone, options.SourcePlayerId, options.Mode, options.Networked, {}});
	g_ManagedClones.back().Weapon = weapon;
	g_NextPedCacheRefresh = {};
	++g_TotalSpawned;
	if (g_TotalSpawned % 5 == 0)
		PruneManagedClones(true);
	FiberPool::Push([clone, owner, options, weapon] { RunCloneBrain(clone, owner, options, weapon); });

	const char* sourceName = PLAYER::GET_PLAYER_NAME(options.SourcePlayerId);
	std::string msg = Localization::IsPortuguese() ? (options.Networked ? "Clone de rede criado" : "Clone local criado") : (options.Networked ? "Network clone created" : "Local clone created");
	if (sourceName && *sourceName)
		msg += std::string(" - ") + sourceName;
	Notifications::Show("Tenebris", msg, NotificationType::Success, 1800);
}

void SpawnManualCloneNearPlayerUnlimited(CloneOptions options)
{
	auto self = Self::GetPed();
	if (!self.IsValid())
		return;
	const int handle = self.GetHandle();
	SpawnManualCloneAtUnlimited(options,
		ENTITY::GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS(handle, 0.0f, 3.0f, 0.25f),
		ENTITY::GET_ENTITY_HEADING(handle));
}

// Redirect only the active editor code that is parsed after this include. The
// legacy implementation above remains untouched but is not the exposed menu.
#define SpawnManualCloneNearPlayer(options) SpawnManualCloneNearPlayerUnlimited((options))
