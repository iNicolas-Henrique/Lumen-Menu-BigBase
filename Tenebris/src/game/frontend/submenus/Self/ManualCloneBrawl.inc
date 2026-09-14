// Professional lightweight clone combat/support layer.
// The active manual-clone path keeps its target scan cached/staggered while
// state storage grows with the number of managed clones instead of imposing an
// artificial eight-clone tracking ceiling.

constexpr float kAggressiveHorizontalCosSq = 0.25f;      // ~120 degree horizontal FOV.
constexpr float kAggressiveVerticalTanSq = 0.7040882f;   // ~80 degree vertical FOV.
constexpr std::size_t kAggressiveLosCandidateCount = 5;
constexpr auto kAggressiveTargetMemory = 550ms;
constexpr auto kAggressiveMinScanInterval = 520ms;
constexpr float kAggressiveSwitchMargin = 0.14f;
constexpr float kAggressiveMemoryLeash = 1.20f;

constexpr float kGuardHorseSearchRadius = 110.0f;
constexpr float kGuardExtremeCatchupDistance = 420.0f;
constexpr auto kGuardHorseAcquireInterval = 1300ms;
constexpr auto kGuardMountTaskTimeout = 8s;
constexpr auto kGuardThreatCacheInterval = 450ms;
constexpr auto kWeaponMismatchGrace = 900ms;
constexpr auto kWeaponCombatRetry = 1100ms;
constexpr auto kWeaponMountedCombatRetry = 2500ms;
constexpr int kCloneHeadBone = 21030;

bool g_BodyguardsDismissed{};
int g_CloneDamagePercent{100};

struct AggressivePerceptionState
{
	int Clone{};
	int Target{};
	float TargetScore{};
	Clock::time_point LastVisible{};
	Clock::time_point NextScan{};
};

struct CloneSupportState
{
	int Clone{};
	int Horse{};
	bool HorseOwned{};
	Hash DesiredWeapon{};
	Hash AppliedCombatProfileWeapon{};
	Clock::time_point WeaponMismatchSince{};
	Clock::time_point NextWeaponCorrection{};
	Clock::time_point NextHorseAttempt{};
	Clock::time_point MountTaskUntil{};
};

struct BodyguardThreatCache
{
	int Anchor{};
	int Threat{};
	Clock::time_point NextScan{};
};

std::vector<AggressivePerceptionState> g_AggressivePerception{};
std::vector<CloneSupportState> g_CloneSupport{};
BodyguardThreatCache g_BodyguardThreatCache{};

void DeleteSupportHorse(CloneSupportState& state);

bool IsLawPed(int ped)
{
	if (!ped || !ENTITY::DOES_ENTITY_EXIST(ped) || ENTITY::IS_ENTITY_DEAD(ped) || !PED::IS_PED_HUMAN(ped))
		return false;
	return PED::GET_PED_RELATIONSHIP_GROUP_HASH(ped) == Joaat("REL_COP");
}

bool IsHostileCloneAlly(int source, int candidate)
{
	auto* sourceManaged = FindManagedClone(source);
	auto* candidateManaged = FindManagedClone(candidate);
	return sourceManaged && candidateManaged &&
		sourceManaged->Mode == CloneMode::FrenzyNpcs && candidateManaged->Mode == CloneMode::FrenzyNpcs;
}

bool IsAssignedCloneHorse(int candidate)
{
	if (!candidate)
		return false;
	for (const auto& support : g_CloneSupport)
		if (support.Horse == candidate)
			return true;
	return false;
}

bool IsAnyLiveDamageablePedTarget(int candidate, int clone)
{
	if (!candidate || candidate == clone || !ENTITY::DOES_ENTITY_EXIST(candidate) || ENTITY::IS_ENTITY_DEAD(candidate))
		return false;
	if (!PED::IS_PED_HUMAN(candidate))
		return false; // Never target horses, wildlife, dogs or any other animal.
	if (IsHostileCloneAlly(clone, candidate) || IsAssignedCloneHorse(candidate))
		return false;
	// Protected shopkeepers and similar script-owned NPCs are commonly marked
	// non-damageable. Do not let the combat task lock onto an immortal target.
	if (!ENTITY::_GET_ENTITY_CAN_BE_DAMAGED(candidate))
		return false;
	return true;
}

AggressivePerceptionState& GetAggressivePerception(int clone)
{
	for (auto& state : g_AggressivePerception)
		if (state.Clone == clone)
			return state;
	for (auto& state : g_AggressivePerception)
	{
		if (!state.Clone || !FindManagedClone(state.Clone) || !ENTITY::DOES_ENTITY_EXIST(state.Clone) || ENTITY::IS_ENTITY_DEAD(state.Clone))
		{
			state = {};
			state.Clone = clone;
			state.NextScan = Clock::now() + std::chrono::milliseconds((std::abs(clone) % 5) * 73);
			return state;
		}
	}
	g_AggressivePerception.emplace_back();
	auto& state = g_AggressivePerception.back();
	state.Clone = clone;
	state.NextScan = Clock::now() + std::chrono::milliseconds((std::abs(clone) % 5) * 73);
	return state;
}

CloneSupportState& GetCloneSupportState(int clone)
{
	for (auto& state : g_CloneSupport)
		if (state.Clone == clone)
			return state;
	for (auto& state : g_CloneSupport)
	{
		if (!state.Clone || !FindManagedClone(state.Clone) || !ENTITY::DOES_ENTITY_EXIST(state.Clone) || ENTITY::IS_ENTITY_DEAD(state.Clone))
		{
			DeleteSupportHorse(state);
			state = {};
			state.Clone = clone;
			return state;
		}
	}
	g_CloneSupport.emplace_back();
	g_CloneSupport.back().Clone = clone;
	return g_CloneSupport.back();
}

void EnsureSupportHorseVisible(int horse)
{
	if (!horse || !ENTITY::DOES_ENTITY_EXIST(horse))
		return;
	ENTITY::SET_ENTITY_VISIBLE(horse, true);
	ENTITY::SET_ENTITY_ALPHA(horse, 255, false);
	ENTITY::RESET_ENTITY_ALPHA(horse);
	ENTITY::SET_ENTITY_COLLISION(horse, true, true);
}

bool RequestCleanupControl(int entity)
{
	if (!entity || !ENTITY::DOES_ENTITY_EXIST(entity))
		return false;
	if (!NETWORK::NETWORK_GET_ENTITY_IS_NETWORKED(entity))
		return true;
	for (int attempt = 0; attempt < 8 && !NETWORK::NETWORK_HAS_CONTROL_OF_ENTITY(entity); ++attempt)
	{
		NETWORK::NETWORK_REQUEST_CONTROL_OF_ENTITY(entity);
		ScriptMgr::Yield(8ms);
	}
	return NETWORK::NETWORK_HAS_CONTROL_OF_ENTITY(entity);
}

void ForceCleanupPedEntity(int ped)
{
	if (!ped || !ENTITY::DOES_ENTITY_EXIST(ped))
		return;
	RequestCleanupControl(ped);
	int handle = ped;
	ENTITY::SET_ENTITY_AS_MISSION_ENTITY(handle, true, true);
	PED::DELETE_PED(&handle);
	if (ENTITY::DOES_ENTITY_EXIST(ped))
	{
		handle = ped;
		ENTITY::DELETE_ENTITY(&handle);
	}
	if (ENTITY::DOES_ENTITY_EXIST(ped))
	{
		handle = ped;
		ENTITY::SET_ENTITY_AS_NO_LONGER_NEEDED(&handle);
	}
}

void DeleteSupportHorse(CloneSupportState& state)
{
	if (state.HorseOwned && state.Horse && ENTITY::DOES_ENTITY_EXIST(state.Horse))
		ForceCleanupPedEntity(state.Horse);
	state.Horse = 0;
	state.HorseOwned = false;
	state.MountTaskUntil = {};
}

void ReleaseCloneSupportState(int clone)
{
	for (auto& state : g_CloneSupport)
	{
		if (state.Clone != clone)
			continue;
		DeleteSupportHorse(state);
		state = {};
		return;
	}
}

bool IsMountPlanActive(int clone)
{
	for (const auto& state : g_CloneSupport)
		if (state.Clone == clone && state.MountTaskUntil > Clock::now())
			return true;
	return false;
}

struct ScoredTarget
{
	int Ped{};
	float Score{-1.0f};
	float DistanceSq{};
};

int FindProfessionalVisibleCloneTarget(int clone, float radius)
{
	if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone))
		return 0;
	radius = std::clamp(radius, 20.0f, 500.0f);
	const auto now = Clock::now();
	auto& memory = GetAggressivePerception(clone);
	if (now < memory.NextScan && IsAnyLiveDamageablePedTarget(memory.Target, clone))
		return memory.Target;
	memory.NextScan = now + kAggressiveMinScanInterval + std::chrono::milliseconds((std::abs(clone) % 4) * 31);

	const Vector3 origin = ENTITY::GET_ENTITY_COORDS(clone, true, false);
	const Vector3 forward = ENTITY::GET_ENTITY_FORWARD_VECTOR(clone);
	const float horizontalLenSq = std::max(0.0001f, forward.x * forward.x + forward.y * forward.y);
	const float radiusSq = radius * radius;
	std::array<ScoredTarget, kAggressiveLosCandidateCount> best{};
	for (auto& candidate : best)
		candidate.Score = -1.0f;

	for (const auto& cached : GetCachedPeds())
	{
		const int candidate = cached.Handle;
		if (!IsAnyLiveDamageablePedTarget(candidate, clone))
			continue;
		const float dx = cached.Position.x - origin.x;
		const float dy = cached.Position.y - origin.y;
		const float dz = cached.Position.z - origin.z;
		const float horizontalSq = dx * dx + dy * dy;
		const float distanceSq = horizontalSq + dz * dz;
		if (distanceSq < 0.25f || distanceSq > radiusSq || horizontalSq < 0.01f)
			continue;

		const float dot = forward.x * dx + forward.y * dy;
		if (dot <= 0.0f || dot * dot < horizontalLenSq * horizontalSq * kAggressiveHorizontalCosSq)
			continue;
		if (dz * dz > horizontalSq * kAggressiveVerticalTanSq)
			continue;

		const float directionScore = std::clamp((dot * dot) / (horizontalLenSq * horizontalSq), 0.0f, 1.0f);
		const float distanceScore = 1.0f - std::clamp(distanceSq / radiusSq, 0.0f, 1.0f);
		float score = directionScore * 0.58f + distanceScore * 0.42f;
		if (candidate == memory.Target)
			score += 0.10f;

		ScoredTarget entry{candidate, score, distanceSq};
		for (std::size_t i = 0; i < best.size(); ++i)
		{
			if (entry.Score <= best[i].Score)
				continue;
			for (std::size_t j = best.size() - 1; j > i; --j)
				best[j] = best[j - 1];
			best[i] = entry;
			break;
		}
	}

	int visibleTarget{};
	float visibleScore{-1.0f};
	for (const auto& candidate : best)
	{
		if (!candidate.Ped)
			continue;
		if (ENTITY::HAS_ENTITY_CLEAR_LOS_TO_ENTITY(clone, candidate.Ped, 17))
		{
			visibleTarget = candidate.Ped;
			visibleScore = candidate.Score;
			break;
		}
	}

	if (visibleTarget)
	{
		const bool keepCurrent = IsAnyLiveDamageablePedTarget(memory.Target, clone) &&
			memory.Target != visibleTarget && memory.TargetScore + kAggressiveSwitchMargin >= visibleScore &&
			now - memory.LastVisible <= kAggressiveTargetMemory;
		if (!keepCurrent)
		{
			memory.Target = visibleTarget;
			memory.TargetScore = visibleScore;
		}
		memory.LastVisible = now;
		return memory.Target;
	}

	if (IsAnyLiveDamageablePedTarget(memory.Target, clone) && now - memory.LastVisible <= kAggressiveTargetMemory)
	{
		const float leash = radius * kAggressiveMemoryLeash;
		if (DistanceSquared(origin, ENTITY::GET_ENTITY_COORDS(memory.Target, true, false)) <= leash * leash)
			return memory.Target;
	}
	memory.Target = 0;
	memory.TargetScore = 0.0f;
	return 0;
}

bool IsMeleeWeaponHash(Hash weapon)
{
	return weapon == Joaat("WEAPON_MELEE_KNIFE") || weapon == Joaat("WEAPON_MELEE_MACHETE") ||
		weapon == Joaat("WEAPON_MELEE_MACHETE_COLLECTOR") || weapon == Joaat("WEAPON_MELEE_HATCHET") ||
		weapon == Joaat("WEAPON_MELEE_CLEAVER") || weapon == Joaat("WEAPON_UNARMED");
}

bool IsShotgunWeaponHash(Hash weapon)
{
	return weapon == Joaat("WEAPON_SHOTGUN_PUMP") || weapon == Joaat("WEAPON_SHOTGUN_REPEATING") ||
		weapon == Joaat("WEAPON_SHOTGUN_DOUBLEBARREL") || weapon == Joaat("WEAPON_SHOTGUN_SEMIAUTO") ||
		weapon == Joaat("WEAPON_SHOTGUN_SAWEDOFF");
}

bool IsBowWeaponHash(Hash weapon)
{
	return weapon == Joaat("WEAPON_BOW") || weapon == Joaat("WEAPON_BOW_IMPROVED");
}

bool IsSniperWeaponHash(Hash weapon)
{
	return weapon == Joaat("WEAPON_SNIPERRIFLE_CARCANO") || weapon == Joaat("WEAPON_SNIPERRIFLE_ROLLINGBLOCK");
}

bool IsSidearmWeaponHash(Hash weapon)
{
	for (const auto& def : kCloneWeapons)
		if (Joaat(def.HashName) == weapon)
			return IsSidearm(def.HashName);
	return false;
}

bool IsRepeaterWeaponHash(Hash weapon)
{
	return weapon == Joaat("WEAPON_REPEATER_CARBINE") || weapon == Joaat("WEAPON_REPEATER_EVANS") ||
		weapon == Joaat("WEAPON_REPEATER_HENRY") || weapon == Joaat("WEAPON_REPEATER_WINCHESTER");
}

bool IsRifleWeaponHash(Hash weapon)
{
	return weapon == Joaat("WEAPON_RIFLE_BOLTACTION") || weapon == Joaat("WEAPON_RIFLE_SPRINGFIELD") ||
		weapon == Joaat("WEAPON_RIFLE_VARMINT") || weapon == Joaat("WEAPON_RIFLE_ELEPHANT");
}

void ApplyProfessionalCombatProfile(int clone, Hash weapon)
{
	if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone))
		return;

	auto& support = GetCloneSupportState(clone);
	if (support.AppliedCombatProfileWeapon == weapon && weapon)
		return;
	support.AppliedCombatProfileWeapon = weapon;

	PED::SET_PED_COMBAT_ABILITY(clone, 3);
	PED::SET_PED_ACCURACY(clone, IsBowWeaponHash(weapon) ? 100 : 96);
	PED::SET_PED_COMBAT_ATTRIBUTES(clone, 4, true);
	PED::SET_PED_COMBAT_ATTRIBUTES(clone, 5, true);
	PED::SET_PED_COMBAT_ATTRIBUTES(clone, 13, true);
	PED::SET_PED_COMBAT_ATTRIBUTES(clone, 21, true);
	PED::SET_PED_COMBAT_ATTRIBUTES(clone, 23, true);
	PED::SET_PED_COMBAT_ATTRIBUTES(clone, 24, true);
	PED::SET_PED_COMBAT_ATTRIBUTES(clone, 27, true);
	PED::SET_PED_COMBAT_ATTRIBUTES(clone, 58, true);
	PED::SET_COMBAT_FLOAT(clone, 26, 0.0f);

	constexpr int kProfileAttributes[] = {0, 8, 12, 16, 28, 31, 42, 43, 44, 46, 47, 50, 71};
	for (const int attribute : kProfileAttributes)
		PED::SET_PED_COMBAT_ATTRIBUTES(clone, attribute, false);

	auto setAttribute = [clone](int attribute) { PED::SET_PED_COMBAT_ATTRIBUTES(clone, attribute, true); };
	auto setFloats = [clone](float maxShootingDistance, float timeBetweenBurstsInCover,
		float strafeChance, float minimumDistance, float optimalCoverDistance,
		float crouchChance, float hipFireChance, float minTimeBeforeCharge,
		float standAndFireDistance, float minimumInCoverDistance) {
		PED::SET_COMBAT_FLOAT(clone, 2, maxShootingDistance);
		PED::SET_COMBAT_FLOAT(clone, 3, timeBetweenBurstsInCover);
		PED::SET_COMBAT_FLOAT(clone, 5, strafeChance);
		PED::SET_COMBAT_FLOAT(clone, 13, minimumDistance);
		PED::SET_COMBAT_FLOAT(clone, 17, optimalCoverDistance);
		PED::SET_COMBAT_FLOAT(clone, 28, crouchChance);
		PED::SET_COMBAT_FLOAT(clone, 39, hipFireChance);
		PED::SET_COMBAT_FLOAT(clone, 47, minTimeBeforeCharge);
		PED::SET_COMBAT_FLOAT(clone, 48, standAndFireDistance);
		PED::SET_COMBAT_FLOAT(clone, 54, minimumInCoverDistance);
	};

	if (IsMeleeWeaponHash(weapon))
	{
		for (int attr : {8, 28, 43, 46, 50, 71}) setAttribute(attr);
		PED::SET_PED_COMBAT_RANGE(clone, 0);
		PED::SET_PED_COMBAT_MOVEMENT(clone, 3);
		setFloats(8.0f, 0.25f, 0.85f, 0.0f, 0.0f, 0.0f, 0.0f, 0.20f, 0.0f, 0.0f);
	}
	else if (IsShotgunWeaponHash(weapon))
	{
		for (int attr : {8, 16, 28, 42, 43, 50, 71}) setAttribute(attr);
		PED::SET_PED_COMBAT_RANGE(clone, 0);
		PED::SET_PED_COMBAT_MOVEMENT(clone, 3);
		setFloats(55.0f, 0.75f, 0.85f, 3.0f, 10.0f, 0.08f, 0.90f, 0.80f, 14.0f, 4.0f);
	}
	else if (IsSniperWeaponHash(weapon))
	{
		for (int attr : {0, 16, 31, 43, 44, 47}) setAttribute(attr);
		PED::SET_PED_COMBAT_RANGE(clone, 3);
		PED::SET_PED_COMBAT_MOVEMENT(clone, 2);
		setFloats(280.0f, 1.60f, 0.25f, 45.0f, 58.0f, 0.78f, 0.0f, 7.0f, 48.0f, 24.0f);
	}
	else if (IsBowWeaponHash(weapon))
	{
		for (int attr : {0, 8, 16, 31, 42, 43, 44, 47}) setAttribute(attr);
		PED::SET_PED_COMBAT_RANGE(clone, 3);
		PED::SET_PED_COMBAT_MOVEMENT(clone, 2);
		setFloats(140.0f, 1.25f, 0.45f, 24.0f, 34.0f, 0.62f, 0.0f, 3.0f, 32.0f, 14.0f);
	}
	else if (IsSidearmWeaponHash(weapon))
	{
		for (int attr : {0, 8, 12, 16, 42, 43, 44, 47}) setAttribute(attr);
		PED::SET_PED_COMBAT_RANGE(clone, 1);
		PED::SET_PED_COMBAT_MOVEMENT(clone, 3);
		setFloats(85.0f, 0.80f, 0.90f, 7.0f, 18.0f, 0.20f, 0.72f, 1.50f, 20.0f, 6.0f);
	}
	else if (IsRepeaterWeaponHash(weapon))
	{
		for (int attr : {0, 8, 12, 16, 31, 42, 43, 44, 47}) setAttribute(attr);
		PED::SET_PED_COMBAT_RANGE(clone, 2);
		PED::SET_PED_COMBAT_MOVEMENT(clone, 2);
		setFloats(145.0f, 1.00f, 0.65f, 16.0f, 28.0f, 0.42f, 0.35f, 2.50f, 28.0f, 10.0f);
	}
	else if (IsRifleWeaponHash(weapon))
	{
		for (int attr : {0, 8, 12, 16, 31, 42, 43, 44, 47}) setAttribute(attr);
		PED::SET_PED_COMBAT_RANGE(clone, 2);
		PED::SET_PED_COMBAT_MOVEMENT(clone, 2);
		setFloats(180.0f, 1.20f, 0.50f, 24.0f, 36.0f, 0.58f, 0.18f, 3.50f, 34.0f, 14.0f);
	}
	else
	{
		for (int attr : {0, 8, 16, 42, 43, 44}) setAttribute(attr);
		PED::SET_PED_COMBAT_RANGE(clone, 2);
		PED::SET_PED_COMBAT_MOVEMENT(clone, 2);
		setFloats(120.0f, 1.00f, 0.60f, 14.0f, 26.0f, 0.35f, 0.30f, 2.50f, 26.0f, 10.0f);
	}
}

void StableEnsureCloneWeaponEquipped(int clone, Hash desiredWeapon)
{
	if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone) || ENTITY::IS_ENTITY_DEAD(clone) || PED::IS_PED_INCAPACITATED(clone))
		return;
	if (!desiredWeapon || desiredWeapon == Joaat("WEAPON_UNARMED") || TASK::IS_EMOTE_TASK_RUNNING(clone, 0))
		return;
	auto& support = GetCloneSupportState(clone);
	support.DesiredWeapon = desiredWeapon;
	const auto now = Clock::now();
	Hash current{};
	if (GetCurrentWeapon(clone, current) && current == desiredWeapon)
	{
		support.WeaponMismatchSince = {};
		return;
	}
	const bool inCombat = PED::IS_PED_IN_COMBAT(clone, 0);
	if (!inCombat)
	{
		support.WeaponMismatchSince = {};
		return;
	}
	if (PED::IS_PED_ON_MOUNT(clone) && IsMeleeWeaponHash(desiredWeapon))
		return;
	if (support.WeaponMismatchSince == Clock::time_point{})
	{
		support.WeaponMismatchSince = now;
		return;
	}
	if (now - support.WeaponMismatchSince < kWeaponMismatchGrace || now < support.NextWeaponCorrection)
		return;
	WEAPON::GIVE_WEAPON_TO_PED(clone, desiredWeapon, 999, true, true, 0, false, 0.5f, 1.0f, 1.0f, false, 0, false);
	WEAPON::SET_CURRENT_PED_WEAPON(clone, desiredWeapon, true, 0, false, false);
	support.NextWeaponCorrection = now + (PED::IS_PED_ON_MOUNT(clone) ? kWeaponMountedCombatRetry : kWeaponCombatRetry);
}

bool IsHorseClaimedByAnotherClone(int horse, int clone)
{
	for (const auto& state : g_CloneSupport)
		if (state.Clone && state.Clone != clone && state.Horse == horse)
			return true;
	return false;
}

int FindUnclaimedFreeHorse(int clone, int anchor)
{
	if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone))
		return 0;
	const Vector3 clonePos = ENTITY::GET_ENTITY_COORDS(clone, true, false);
	float bestSq = kGuardHorseSearchRadius * kGuardHorseSearchRadius;
	int best{};
	for (const auto& cached : GetCachedPeds())
	{
		const int horse = cached.Handle;
		if (!horse || horse == clone || horse == anchor || cached.Player || cached.Dead || PED::IS_PED_HUMAN(horse) || !ENTITY::DOES_ENTITY_EXIST(horse))
			continue;
		const Hash model = ENTITY::GET_ENTITY_MODEL(horse);
		if (!model || !PED::_IS_THIS_MODEL_A_HORSE(model) || !PED::_IS_MOUNT_SEAT_FREE(horse, -1) || IsHorseClaimedByAnotherClone(horse, clone))
			continue;
		const float d = DistanceSquared(clonePos, cached.Position);
		if (d < bestSq)
		{
			bestSq = d;
			best = horse;
		}
	}
	return best;
}

int SpawnDedicatedGuardHorse(int clone)
{
	const Hash model = Joaat("A_C_Horse_KentuckySaddle_Black");
	if (!EnsureModelLoaded(model))
		return 0;
	const Vector3 spawn = ENTITY::GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS(clone, 2.0f, -4.0f, 0.5f);
	int horse = PED::CREATE_PED(model, spawn.x, spawn.y, spawn.z, ENTITY::GET_ENTITY_HEADING(clone), false, false, false, false);
	if (!horse || !ENTITY::DOES_ENTITY_EXIST(horse))
	{
		STREAMING::SET_MODEL_AS_NO_LONGER_NEEDED(model);
		return 0;
	}
	ENTITY::SET_ENTITY_AS_MISSION_ENTITY(horse, true, true);
	PED::_SET_RANDOM_OUTFIT_VARIATION(horse, true);
	PED::_UPDATE_PED_VARIATION(horse, false, true, true, true, false);
	ENTITY::PLACE_ENTITY_ON_GROUND_PROPERLY(horse, true);
	EnsureSupportHorseVisible(horse);
	PED::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(horse, false);
	STREAMING::SET_MODEL_AS_NO_LONGER_NEEDED(model);
	return horse;
}

bool ForceGuardMount(int clone, int anchor, bool allowSpawn)
{
	if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone) || PED::IS_PED_ON_MOUNT(clone))
		return PED::IS_PED_ON_MOUNT(clone);
	auto& state = GetCloneSupportState(clone);
	const auto now = Clock::now();
	if (now < state.NextHorseAttempt || now < state.MountTaskUntil)
		return false;

	int horse = state.Horse;
	if (!horse || !ENTITY::DOES_ENTITY_EXIST(horse) || ENTITY::IS_ENTITY_DEAD(horse) || !PED::_IS_MOUNT_SEAT_FREE(horse, -1))
	{
		if (state.HorseOwned)
			DeleteSupportHorse(state);
		else
			state.Horse = 0;
		horse = FindUnclaimedFreeHorse(clone, anchor);
		if (horse)
		{
			state.Horse = horse;
			state.HorseOwned = false;
		}
		else if (allowSpawn)
		{
			horse = SpawnDedicatedGuardHorse(clone);
			state.Horse = horse;
			state.HorseOwned = horse != 0;
		}
	}
	state.NextHorseAttempt = now + kGuardHorseAcquireInterval;
	if (!horse)
		return false;
	EnsureSupportHorseVisible(horse);
	TASK::TASK_MOUNT_ANIMAL(clone, horse, 6500, -1, 2.5f, 1, 0, 0);
	state.MountTaskUntil = now + kGuardMountTaskTimeout;
	return true;
}

int GetBodyguardAnchor(int clone, int owner)
{
	if (!g_BodyguardsDismissed)
		return owner;
	auto* managed = FindManagedClone(clone);
	if (!managed)
		return owner;
	const int sourceId = managed->SourcePlayerId < 0 ? PLAYER::PLAYER_ID() : managed->SourcePlayerId;
	if (!NETWORK::NETWORK_IS_PLAYER_ACTIVE(sourceId))
		return owner;
	const int sourcePed = PLAYER::GET_PLAYER_PED_SCRIPT_INDEX(sourceId);
	return sourcePed && ENTITY::DOES_ENTITY_EXIST(sourcePed) && !ENTITY::IS_ENTITY_DEAD(sourcePed) ? sourcePed : owner;
}

int FindProfessionalBodyguardThreat(int clone, int owner, float radius)
{
	const int anchor = GetBodyguardAnchor(clone, owner);
	if (!clone || !anchor || !ENTITY::DOES_ENTITY_EXIST(clone) || !ENTITY::DOES_ENTITY_EXIST(anchor))
		return 0;
	const auto now = Clock::now();
	if (g_BodyguardThreatCache.Anchor == anchor && now < g_BodyguardThreatCache.NextScan && IsAnyLiveDamageablePedTarget(g_BodyguardThreatCache.Threat, clone))
		return g_BodyguardThreatCache.Threat;
	g_BodyguardThreatCache.Anchor = anchor;
	g_BodyguardThreatCache.NextScan = now + kGuardThreatCacheInterval;

	const Vector3 anchorPos = ENTITY::GET_ENTITY_COORDS(anchor, true, false);
	float bestDistanceSq = std::clamp(radius, 30.0f, 500.0f);
	bestDistanceSq *= bestDistanceSq;
	int best{};

	for (int id = 0; id < 32; ++id)
	{
		if (!NETWORK::NETWORK_IS_PLAYER_ACTIVE(id))
			continue;
		const int ped = PLAYER::GET_PLAYER_PED_SCRIPT_INDEX(id);
		if (!ped || ped == anchor || ped == clone || !IsAnyLiveDamageablePedTarget(ped, clone))
			continue;
		const bool aiming = PLAYER::IS_PLAYER_FREE_AIMING_AT_ENTITY(id, anchor) || PLAYER::IS_PLAYER_TARGETTING_ENTITY(id, anchor);
		if (!aiming && !PED::IS_PED_IN_COMBAT(ped, anchor) && !ENTITY::HAS_ENTITY_BEEN_DAMAGED_BY_ENTITY(anchor, ped, true, true))
			continue;
		const float d = DistanceSquared(anchorPos, ENTITY::GET_ENTITY_COORDS(ped, true, false));
		if (d < bestDistanceSq)
		{
			bestDistanceSq = d;
			best = ped;
		}
	}

	for (const auto& cached : GetCachedPeds())
	{
		const int ped = cached.Handle;
		if (!ped || ped == anchor || ped == clone || cached.Dead || !IsAnyLiveDamageablePedTarget(ped, clone))
			continue;
		const bool threatening = PED::IS_PED_IN_COMBAT(ped, anchor) || PED::IS_PED_IN_COMBAT(anchor, ped) || ENTITY::HAS_ENTITY_BEEN_DAMAGED_BY_ENTITY(anchor, ped, true, true);
		if (!threatening)
			continue;
		const float d = DistanceSquared(anchorPos, cached.Position);
		if (d < bestDistanceSq)
		{
			bestDistanceSq = d;
			best = ped;
		}
	}
	g_BodyguardThreatCache.Threat = best;
	return best;
}

int GetBodyguardFormationIndex(int clone)
{
	int index{};
	for (const auto& managed : g_ManagedClones)
	{
		if (managed.Mode != CloneMode::Bodyguard || !managed.Ped || !ENTITY::DOES_ENTITY_EXIST(managed.Ped) || ENTITY::IS_ENTITY_DEAD(managed.Ped))
			continue;
		if (managed.Ped == clone)
			return index;
		++index;
	}
	return index;
}

void MaintainProfessionalBodyguardFollow(int clone, int owner, Clock::time_point now, Clock::time_point& nextHorse, Clock::time_point& nextFollow, bool& followIssued)
{
	const int anchor = GetBodyguardAnchor(clone, owner);
	if (!clone || !anchor || !ENTITY::DOES_ENTITY_EXIST(clone) || !ENTITY::DOES_ENTITY_EXIST(anchor))
		return;
	PED::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(clone, true);
	const Vector3 clonePos = ENTITY::GET_ENTITY_COORDS(clone, true, false);
	const Vector3 anchorPos = ENTITY::GET_ENTITY_COORDS(anchor, true, false);
	const float dSq = DistanceSquared(clonePos, anchorPos);
	const int formation = GetBodyguardFormationIndex(clone);
	const float sideOffset = static_cast<float>((formation % 3) - 1) * 2.4f;
	const float backOffset = -2.8f - static_cast<float>(formation / 3) * 2.0f;

	const bool dismissed = g_BodyguardsDismissed;
	const bool ownerMounted = !dismissed && GetPlayerMountHandle() != 0;
	if ((dismissed || ownerMounted) && now >= nextHorse && !PED::IS_PED_IN_COMBAT(clone, 0))
	{
		ForceGuardMount(clone, anchor, true);
		nextHorse = now + kGuardHorseAcquireInterval;
	}

	if (dSq > kGuardExtremeCatchupDistance * kGuardExtremeCatchupDistance && !PED::IS_PED_IN_COMBAT(clone, 0))
	{
		const Vector3 catchup = ENTITY::GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS(anchor, sideOffset * 2.0f, backOffset * 2.0f, 0.25f);
		const int mount = PED::IS_PED_ON_MOUNT(clone) ? PED::GET_MOUNT(clone) : 0;
		const int travelEntity = mount && ENTITY::DOES_ENTITY_EXIST(mount) ? mount : clone;
		ENTITY::SET_ENTITY_COORDS_NO_OFFSET(travelEntity, catchup.x, catchup.y, catchup.z, true, true, true);
		if (mount) EnsureSupportHorseVisible(mount);
		followIssued = false;
		nextFollow = now;
		return;
	}

	const bool mounted = PED::IS_PED_ON_MOUNT(clone);
	if (dSq <= kFollowStopRadius * kFollowStopRadius && !mounted && !dismissed)
	{
		if (followIssued)
		{
			TASK::CLEAR_PED_TASKS(clone, true, false);
			followIssued = false;
		}
		PED::SET_PED_MOVE_RATE_OVERRIDE(clone, 1.0f);
		nextFollow = now + 700ms;
		return;
	}
	if (now < nextFollow && followIssued)
		return;
	const bool isFar = dSq > 70.0f * 70.0f;
	const bool isVeryFar = dSq > 180.0f * 180.0f;
	const float speed = mounted ? (isVeryFar ? 7.0f : (isFar ? 6.0f : 4.0f)) : (isVeryFar ? 5.0f : (isFar ? 4.0f : 2.4f));
	PED::SET_PED_MOVE_RATE_OVERRIDE(clone, isVeryFar ? 1.65f : (isFar ? 1.45f : 1.1f));
	TASK::TASK_FOLLOW_TO_OFFSET_OF_ENTITY(clone, anchor, sideOffset, backOffset, 0.0f, speed, -1, mounted ? 4.0f : 3.0f, true, true, false, true, true, true);
	followIssued = true;
	nextFollow = now + (dismissed ? 800ms : (isVeryFar ? 900ms : (isFar ? 1400ms : 3200ms)));
}

bool StartCloneGunTwirlUnmounted(int clone, Hash desiredWeapon, std::string_view weaponName, Clock::time_point now, Clock::time_point& endsAt)
{
	if (PED::IS_PED_ON_MOUNT(clone) || IsMountPlanActive(clone))
		return false;
	return StartCloneGunTwirl(clone, desiredWeapon, weaponName, now, endsAt);
}

void PlayTimedEmoteUnmounted(int clone, int faceTarget, const ManualCloneEmotes::Definition& emote, bool flourish, bool force, bool restoreWeapon = true)
{
	if (!clone || PED::IS_PED_ON_MOUNT(clone) || IsMountPlanActive(clone))
		return;
	PlayTimedEmote(clone, faceTarget, emote, flourish, force, restoreWeapon);
}

void PlayRareSocialReactionUnmounted(int clone, int faceTarget, bool flourish, bool insult = false)
{
	if (!clone || PED::IS_PED_ON_MOUNT(clone) || IsMountPlanActive(clone))
		return;
	PlayRareSocialReaction(clone, faceTarget, flourish, insult);
}

void MaybeStartMourningOnFoot(int deadClone, CloneMode deadMode)
{
	ReleaseCloneSupportState(deadClone);
	if (!deadClone || !ENTITY::DOES_ENTITY_EXIST(deadClone))
		return;
	const auto now = Clock::now();
	if ((g_MourningClone && now < g_MourningFleeUntil) || now < g_NextMourningAllowed || RandomInt(1, 100) > kMourningChancePercent)
		return;
	const Vector3 deadPos = ENTITY::GET_ENTITY_COORDS(deadClone, true, false);
	int mourner{};
	float bestDistanceSq = kMourningRadius * kMourningRadius;
	for (const auto& managed : g_ManagedClones)
	{
		if (!managed.Ped || managed.Ped == deadClone || managed.Mode != deadMode || !ENTITY::DOES_ENTITY_EXIST(managed.Ped) || ENTITY::IS_ENTITY_DEAD(managed.Ped) || PED::IS_PED_INCAPACITATED(managed.Ped) || PED::IS_PED_ON_MOUNT(managed.Ped) || IsMountPlanActive(managed.Ped))
			continue;
		const float d = DistanceSquared(deadPos, ENTITY::GET_ENTITY_COORDS(managed.Ped, true, false));
		if (d < bestDistanceSq)
		{
			bestDistanceSq = d;
			mourner = managed.Ped;
		}
	}
	if (!mourner)
		return;
	g_MourningClone = mourner;
	g_MourningDeadClone = deadClone;
	g_MourningEmoteUntil = now + kMourningEmoteTime;
	g_MourningFleeUntil = g_MourningEmoteUntil + kMourningFleeTime;
	g_MourningFleeIssued = false;
	g_NextMourningAllowed = now + kMourningCooldown;
	TASK::CLEAR_PED_TASKS(mourner, true, false);
	FaceEntity(mourner, deadClone);
	PlayFullBodyEmote(mourner, ManualCloneEmotes::kMourningEmote);
}

void TaskProfessionalCloneCombat(int clone, int target, Hash desiredWeapon)
{
	if (!IsAnyLiveDamageablePedTarget(target, clone))
		return;
	StableEnsureCloneWeaponEquipped(clone, desiredWeapon);
	ApplyProfessionalCombatProfile(clone, desiredWeapon);
	PED::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(clone, false);
	PED::SET_PED_MOVE_RATE_OVERRIDE(clone, 1.0f);
	TASK::TASK_LOOK_AT_ENTITY(clone, target, 1200, 0, 0, 0);
	TASK::TASK_COMBAT_PED(clone, target, 0, 16);
}

void DeleteAllManagedClonesAndMounts()
{
	++g_BrawlGeneration;
	g_SessionBrawlRunning = false;
	g_RemoteControlledClone = 0;
	for (auto& state : g_CloneSupport)
		DeleteSupportHorse(state);
	g_CloneSupport.clear();
	for (auto& managed : g_ManagedClones)
	{
		if (managed.Ped && ENTITY::DOES_ENTITY_EXIST(managed.Ped))
			ForceCleanupPedEntity(managed.Ped);
		managed.Ped = 0;
	}
	g_ManagedClones.clear();
	g_PedCache.clear();
	g_NextPedCacheRefresh = {};
	g_BodyguardsBetrayed = false;
	g_BodyguardsDismissed = false;
	g_TotalSpawned = 0;
	g_AggressivePerception.clear();
	g_BodyguardThreatCache = {};
	ResetMourning();
	Notifications::Show("Tenebris", Localization::IsPortuguese() ? "Todos os clones e cavalos foram removidos." : "All clones and horses were removed.", NotificationType::Success, 2200);
}

void RunSessionCloneBrawl(CloneOptions)
{
	g_SessionBrawlRunning = false;
	Notifications::Show("Tenebris",
		Localization::IsPortuguese() ? "Batalha/Expedição foi removida da versão otimizada." : "Brawl/Expedition was removed from the optimized build.",
		NotificationType::Info, 2200);
}

#define FindNearestNpcTarget(clone, owner, radius) FindProfessionalVisibleCloneTarget((clone), (radius))
#define IsValidNpcTarget(candidate, clone, owner) IsAnyLiveDamageablePedTarget((candidate), (clone))
#define TaskVanillaCombat(clone, target, owner, desiredWeapon) TaskProfessionalCloneCombat((clone), (target), (desiredWeapon))
#define EnsureCloneWeaponEquipped(clone, desiredWeapon) StableEnsureCloneWeaponEquipped((clone), (desiredWeapon))
#define FindBodyguardThreat(clone, owner, radius) FindProfessionalBodyguardThreat((clone), (owner), (radius))
#define MaintainBodyguardFollow(clone, owner, now, nextHorse, nextFollow, followIssued) MaintainProfessionalBodyguardFollow((clone), (owner), (now), (nextHorse), (nextFollow), (followIssued))
#define StartCloneGunTwirl(clone, desiredWeapon, weaponName, now, endsAt) StartCloneGunTwirlUnmounted((clone), (desiredWeapon), (weaponName), (now), (endsAt))
#define PlayTimedEmote(...) PlayTimedEmoteUnmounted(__VA_ARGS__)
#define PlayRareSocialReaction(...) PlayRareSocialReactionUnmounted(__VA_ARGS__)
#define MaybeStartMourning(deadClone, deadMode) MaybeStartMourningOnFoot((deadClone), (deadMode))
#define DeleteAllManagedClones() DeleteAllManagedClonesAndMounts()
