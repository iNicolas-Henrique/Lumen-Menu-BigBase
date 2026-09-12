// Lean compatibility layer for manual clone aggression and bodyguard support.
// Session brawl/expedition stays removed. The code below reuses the existing
// clone brain and shared PED cache instead of adding a second per-frame system.

constexpr float kAggressiveHorizontalCosSq = 0.25f; // 120-degree horizontal FOV.
constexpr float kAggressiveVerticalTanSq = 0.7040882f; // ~80-degree vertical FOV.
constexpr std::size_t kAggressiveLosCandidateCount = 5;
constexpr auto kAggressiveTargetMemory = 550ms;
constexpr auto kAggressiveMinScanInterval = 520ms;
constexpr float kAggressiveSwitchMargin = 0.14f;
constexpr float kAggressiveMemoryLeash = 1.20f;

constexpr float kGuardHorseSearchRadius = 110.0f;
constexpr float kGuardHorseTriggerDistance = 28.0f;
constexpr float kGuardCatchUpWarpDistance = 420.0f;
constexpr auto kGuardHorseAcquireInterval = 1300ms;
constexpr auto kGuardMountTaskTimeout = 8s;
constexpr auto kGuardThreatCacheInterval = 450ms;
constexpr auto kWeaponMismatchGrace = 900ms;
constexpr auto kWeaponCombatRetry = 1100ms;
constexpr auto kWeaponIdleRetry = 5500ms;
constexpr auto kWeaponMountedRetry = 8s;

struct AggressivePerceptionState
{
	int Clone{};
	int Target{};
	float LastScore{};
	Clock::time_point LastSeen{};
	Clock::time_point NextScan{};
};

struct CloneSupportState
{
	int Clone{};
	int Horse{};
	bool HorseOwned{};
	Hash DesiredWeapon{};
	Clock::time_point WeaponMismatchSince{};
	Clock::time_point LastWeaponForce{};
	Clock::time_point NextHorseAcquire{};
	Clock::time_point MountTaskIssuedAt{};
};

struct BodyguardThreatCache
{
	int Owner{};
	int Threat{};
	Clock::time_point Expires{};
};

std::array<AggressivePerceptionState, kMaxActiveClones> g_AggressivePerception{};
std::array<CloneSupportState, kMaxActiveClones> g_CloneSupport{};
BodyguardThreatCache g_BodyguardThreatCache{};

bool IsAnyLiveHumanPedTarget(int candidate, int clone)
{
	if (!candidate || candidate == clone || !ENTITY::DOES_ENTITY_EXIST(candidate) || ENTITY::IS_ENTITY_DEAD(candidate))
		return false;
	return PED::IS_PED_HUMAN(candidate);
}

void DeleteSupportHorse(CloneSupportState& state)
{
	if (state.HorseOwned && state.Horse && ENTITY::DOES_ENTITY_EXIST(state.Horse))
	{
		int horse = state.Horse;
		PED::DELETE_PED(&horse);
	}
	state.Horse = 0;
	state.HorseOwned = false;
	state.MountTaskIssuedAt = {};
}

CloneSupportState& GetCloneSupportState(int clone)
{
	for (auto& state : g_CloneSupport)
	{
		if (state.Clone == clone)
			return state;
	}

	for (std::size_t i = 0; i < g_CloneSupport.size(); ++i)
	{
		auto& state = g_CloneSupport[i];
		if (!state.Clone || !ENTITY::DOES_ENTITY_EXIST(state.Clone) || ENTITY::IS_ENTITY_DEAD(state.Clone))
		{
			DeleteSupportHorse(state);
			state = {};
			state.Clone = clone;
			state.NextHorseAcquire = Clock::now() + std::chrono::milliseconds(static_cast<int>(i) * 140);
			return state;
		}
	}

	DeleteSupportHorse(g_CloneSupport[0]);
	g_CloneSupport[0] = {};
	g_CloneSupport[0].Clone = clone;
	return g_CloneSupport[0];
}

void ReleaseCloneSupportState(int clone)
{
	for (auto& state : g_CloneSupport)
	{
		if (state.Clone != clone)
			continue;
		DeleteSupportHorse(state);
		state = {};
		break;
	}
}

bool IsHorseClaimedByOtherClone(int horse, int clone)
{
	if (!horse)
		return false;
	for (const auto& state : g_CloneSupport)
	{
		if (state.Clone && state.Clone != clone && state.Horse == horse)
			return true;
	}
	return false;
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

AggressivePerceptionState& GetAggressivePerceptionState(int clone)
{
	for (auto& state : g_AggressivePerception)
	{
		if (state.Clone == clone)
			return state;
	}
	for (auto& state : g_AggressivePerception)
	{
		if (!state.Clone || !ENTITY::DOES_ENTITY_EXIST(state.Clone) || ENTITY::IS_ENTITY_DEAD(state.Clone))
		{
			state = {};
			state.Clone = clone;
			return state;
		}
	}
	g_AggressivePerception[0] = {};
	g_AggressivePerception[0].Clone = clone;
	return g_AggressivePerception[0];
}

float ScoreAggressiveCandidate(float distanceSq, float radiusSq, float forwardDot, float horizontalSq, bool currentTarget)
{
	const float distanceScore = std::clamp(1.0f - (distanceSq / radiusSq), 0.0f, 1.0f);
	const float alignmentSq = horizontalSq > 0.001f ? std::clamp((forwardDot * forwardDot) / horizontalSq, 0.0f, 1.0f) : 1.0f;
	const float alignmentScore = std::clamp((alignmentSq - kAggressiveHorizontalCosSq) / (1.0f - kAggressiveHorizontalCosSq), 0.0f, 1.0f);
	return alignmentScore * 0.58f + distanceScore * 0.42f + (currentTarget ? 0.12f : 0.0f);
}

bool IsTargetInsideMemoryLeash(int clone, int target, float radius)
{
	if (!IsAnyLiveHumanPedTarget(target, clone))
		return false;
	const Vector3 a = ENTITY::GET_ENTITY_COORDS(clone, true, false);
	const Vector3 b = ENTITY::GET_ENTITY_COORDS(target, true, false);
	const float dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
	const float leash = radius * kAggressiveMemoryLeash;
	return dx * dx + dy * dy + dz * dz <= leash * leash;
}

int FindProfessionalVisibleCloneTarget(int clone, float radius)
{
	if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone) || ENTITY::IS_ENTITY_DEAD(clone))
		return 0;

	auto& state = GetAggressivePerceptionState(clone);
	const auto now = Clock::now();
	if (state.Target && now < state.NextScan && now - state.LastSeen <= kAggressiveTargetMemory && IsTargetInsideMemoryLeash(clone, state.Target, radius))
		return state.Target;

	state.NextScan = now + kAggressiveMinScanInterval + std::chrono::milliseconds((clone & 0x7) * 23);
	const Vector3 origin = ENTITY::GET_ENTITY_COORDS(clone, true, false);
	const auto forward = ENTITY::GET_ENTITY_FORWARD_VECTOR(clone);
	const float radiusSq = radius * radius;
	std::array<int, kAggressiveLosCandidateCount> candidates{};
	std::array<float, kAggressiveLosCandidateCount> scores{};
	scores.fill(-1.0f);

	for (const auto& ped : GetCachedPeds())
	{
		if (!ped.Handle || ped.Handle == clone || ped.Dead || !PED::IS_PED_HUMAN(ped.Handle))
			continue;
		const float dx = ped.Position.x - origin.x, dy = ped.Position.y - origin.y, dz = ped.Position.z - origin.z;
		const float horizontalSq = dx * dx + dy * dy;
		const float distanceSq = horizontalSq + dz * dz;
		if (distanceSq < 0.01f || distanceSq > radiusSq)
			continue;
		const float forwardDot = forward.x * dx + forward.y * dy;
		if (forwardDot <= 0.0f || (horizontalSq > 0.001f && forwardDot * forwardDot < kAggressiveHorizontalCosSq * horizontalSq))
			continue;
		if ((horizontalSq <= 0.001f && dz * dz > 1.0f) || (horizontalSq > 0.001f && dz * dz > kAggressiveVerticalTanSq * horizontalSq))
			continue;

		const float score = ScoreAggressiveCandidate(distanceSq, radiusSq, forwardDot, horizontalSq, ped.Handle == state.Target);
		for (std::size_t slot = 0; slot < candidates.size(); ++slot)
		{
			if (score <= scores[slot])
				continue;
			for (std::size_t move = candidates.size() - 1; move > slot; --move)
			{
				candidates[move] = candidates[move - 1];
				scores[move] = scores[move - 1];
			}
			candidates[slot] = ped.Handle;
			scores[slot] = score;
			break;
		}
	}

	int bestTarget{};
	float bestScore = -1.0f, currentVisibleScore = -1.0f;
	for (std::size_t i = 0; i < candidates.size(); ++i)
	{
		const int candidate = candidates[i];
		if (!IsAnyLiveHumanPedTarget(candidate, clone) || !ENTITY::HAS_ENTITY_CLEAR_LOS_TO_ENTITY(clone, candidate, 17))
			continue;
		if (candidate == state.Target)
			currentVisibleScore = scores[i];
		if (scores[i] > bestScore)
		{
			bestScore = scores[i];
			bestTarget = candidate;
		}
	}

	if (state.Target && currentVisibleScore >= 0.0f && bestTarget && bestTarget != state.Target && bestScore < currentVisibleScore + kAggressiveSwitchMargin)
	{
		bestTarget = state.Target;
		bestScore = currentVisibleScore;
	}
	if (bestTarget)
	{
		state.Target = bestTarget;
		state.LastScore = bestScore;
		state.LastSeen = now;
		return bestTarget;
	}
	if (state.Target && now - state.LastSeen <= kAggressiveTargetMemory && IsTargetInsideMemoryLeash(clone, state.Target, radius))
		return state.Target;
	state.Target = 0;
	state.LastScore = 0.0f;
	state.LastSeen = {};
	return 0;
}

void StableEnsureCloneWeaponEquipped(int clone, Hash desiredWeapon)
{
	if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone) || ENTITY::IS_ENTITY_DEAD(clone) || PED::IS_PED_INCAPACITATED(clone))
		return;
	if (!desiredWeapon || desiredWeapon == Joaat("WEAPON_UNARMED") || TASK::IS_EMOTE_TASK_RUNNING(clone, 0))
		return;

	auto& state = GetCloneSupportState(clone);
	state.DesiredWeapon = desiredWeapon;
	Hash currentWeapon{};
	if (GetCurrentWeapon(clone, currentWeapon) && currentWeapon == desiredWeapon)
	{
		state.WeaponMismatchSince = {};
		return;
	}
	const auto now = Clock::now();
	if (state.WeaponMismatchSince == Clock::time_point{})
	{
		state.WeaponMismatchSince = now;
		return;
	}
	if (now - state.WeaponMismatchSince < kWeaponMismatchGrace)
		return;
	const bool inCombat = PED::IS_PED_IN_COMBAT(clone, 0);
	const bool mounted = PED::IS_PED_ON_MOUNT(clone);
	const auto retry = inCombat ? kWeaponCombatRetry : (mounted ? kWeaponMountedRetry : kWeaponIdleRetry);
	if (state.LastWeaponForce != Clock::time_point{} && now - state.LastWeaponForce < retry)
		return;

	// The weapon is given once during spawn. Repeated GIVE+SET calls were able to
	// restart the draw/stow transition forever, so audits only reselect it after
	// a persistent mismatch and use long cooldowns outside combat.
	WEAPON::SET_CURRENT_PED_WEAPON(clone, desiredWeapon, true, 0, false, false);
	state.LastWeaponForce = now;
	state.WeaponMismatchSince = now;
}

bool IsMountPlanActive(int clone)
{
	for (const auto& state : g_CloneSupport)
	{
		if (state.Clone != clone || !state.Horse)
			continue;
		if (PED::IS_PED_ON_MOUNT(clone))
			return true;
		if (state.MountTaskIssuedAt != Clock::time_point{} && Clock::now() - state.MountTaskIssuedAt < kGuardMountTaskTimeout)
			return true;
	}
	return false;
}

int FindUnclaimedFreeHorse(int clone, int owner)
{
	const Vector3 origin = ENTITY::GET_ENTITY_COORDS(clone, true, false);
	const int ownerMount = GetPlayerMountHandle();
	int best{};
	float bestDistanceSq = kGuardHorseSearchRadius * kGuardHorseSearchRadius;
	for (const auto& ped : GetCachedPeds())
	{
		if (!ped.Handle || ped.Handle == owner || ped.Handle == ownerMount || ped.Player || ped.Dead || PED::IS_PED_HUMAN(ped.Handle) || IsHorseClaimedByOtherClone(ped.Handle, clone))
			continue;
		if (!PED::_IS_THIS_MODEL_A_HORSE(ENTITY::GET_ENTITY_MODEL(ped.Handle)) || !PED::_IS_MOUNT_SEAT_FREE(ped.Handle, -1))
			continue;
		const float d = DistanceSquared(origin, ped.Position);
		if (d < bestDistanceSq)
		{
			bestDistanceSq = d;
			best = ped.Handle;
		}
	}
	return best;
}

int SpawnDedicatedGuardHorse(int clone)
{
	if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone))
		return 0;
	const Hash model = Joaat("A_C_Horse_KentuckySaddle_Black");
	if (!EnsureModelLoaded(model))
		return 0;
	const int formation = GetBodyguardFormationIndex(clone);
	const float side = static_cast<float>((formation % 3) - 1) * 2.8f;
	const Vector3 spawn = ENTITY::GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS(clone, side, -3.5f - static_cast<float>(formation / 3), 0.35f);
	const float heading = ENTITY::GET_ENTITY_HEADING(clone);
	int horse = PED::CREATE_PED(model, spawn.x, spawn.y, spawn.z, heading, false, 0, 0, 0);
	STREAMING::SET_MODEL_AS_NO_LONGER_NEEDED(model);
	if (!horse || !ENTITY::DOES_ENTITY_EXIST(horse))
		return 0;
	ENTITY::SET_ENTITY_AS_MISSION_ENTITY(horse, true, true);
	ENTITY::PLACE_ENTITY_ON_GROUND_PROPERLY(horse, true);
	PED::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(horse, false);
	return horse;
}

int AcquireGuardHorse(int clone, int owner, bool allowSpawn)
{
	if (!clone || !owner || !ENTITY::DOES_ENTITY_EXIST(clone) || !ENTITY::DOES_ENTITY_EXIST(owner))
		return 0;
	auto& state = GetCloneSupportState(clone);
	if (PED::IS_PED_ON_MOUNT(clone))
	{
		const int actualMount = PED::GET_MOUNT(clone);
		if (actualMount && ENTITY::DOES_ENTITY_EXIST(actualMount))
		{
			if (state.HorseOwned && state.Horse && state.Horse != actualMount)
				DeleteSupportHorse(state);
			state.Horse = actualMount;
			state.MountTaskIssuedAt = {};
			return actualMount;
		}
	}
	const auto now = Clock::now();
	if (state.Horse && ENTITY::DOES_ENTITY_EXIST(state.Horse) && !ENTITY::IS_ENTITY_DEAD(state.Horse))
	{
		const bool mountTaskFresh = state.MountTaskIssuedAt != Clock::time_point{} && now - state.MountTaskIssuedAt < kGuardMountTaskTimeout;
		if (mountTaskFresh || PED::_IS_MOUNT_SEAT_FREE(state.Horse, -1))
			return state.Horse;
	}
	else
	{
		DeleteSupportHorse(state);
	}
	if (now < state.NextHorseAcquire)
		return 0;
	state.NextHorseAcquire = now + kGuardHorseAcquireInterval + std::chrono::milliseconds((clone & 0x7) * 70);
	if (const int nearby = FindUnclaimedFreeHorse(clone, owner))
	{
		if (state.HorseOwned && state.Horse != nearby)
			DeleteSupportHorse(state);
		state.Horse = nearby;
		state.HorseOwned = false;
		return nearby;
	}
	if (!allowSpawn)
		return 0;
	const int spawned = SpawnDedicatedGuardHorse(clone);
	if (spawned)
	{
		state.Horse = spawned;
		state.HorseOwned = true;
	}
	return spawned;
}

bool ForceGuardMount(int clone, int owner, bool allowSpawn)
{
	if (!clone || !owner || !ENTITY::DOES_ENTITY_EXIST(clone) || !ENTITY::DOES_ENTITY_EXIST(owner))
		return false;
	if (PED::IS_PED_ON_MOUNT(clone))
		return true;
	auto& state = GetCloneSupportState(clone);
	const auto now = Clock::now();
	if (state.MountTaskIssuedAt != Clock::time_point{} && now - state.MountTaskIssuedAt < 1300ms)
		return false;
	const int horse = AcquireGuardHorse(clone, owner, allowSpawn);
	if (!horse || !ENTITY::DOES_ENTITY_EXIST(horse))
		return false;
	TASK::TASK_MOUNT_ANIMAL(clone, horse, 8000, -1, 3.0f, 1, 0, 0);
	state.MountTaskIssuedAt = now;
	return true;
}

bool IsRemotePlayerThreateningOwner(int playerId, int owner)
{
	if (playerId < 0 || playerId >= 32 || playerId == PLAYER::PLAYER_ID() || !NETWORK::NETWORK_IS_PLAYER_ACTIVE(playerId))
		return false;
	const int ped = PLAYER::GET_PLAYER_PED_SCRIPT_INDEX(playerId);
	if (!ped || ped == owner || !ENTITY::DOES_ENTITY_EXIST(ped) || ENTITY::IS_ENTITY_DEAD(ped) || !PED::IS_PED_HUMAN(ped))
		return false;
	if (PLAYER::IS_PLAYER_FREE_AIMING_AT_ENTITY(playerId, owner))
		return true;
	if (PLAYER::IS_PLAYER_TARGETTING_ENTITY(playerId, owner, true))
		return true;
	return PED::IS_PED_IN_COMBAT(ped, owner) || PED::IS_PED_IN_COMBAT(owner, ped) || ENTITY::HAS_ENTITY_BEEN_DAMAGED_BY_ENTITY(owner, ped, true, true);
}

int FindProfessionalBodyguardThreat(int clone, int owner, float)
{
	if (!clone || !owner || !ENTITY::DOES_ENTITY_EXIST(clone) || !ENTITY::DOES_ENTITY_EXIST(owner))
		return 0;
	const auto now = Clock::now();
	if (g_BodyguardThreatCache.Owner == owner && now < g_BodyguardThreatCache.Expires)
	{
		const int cached = g_BodyguardThreatCache.Threat;
		return IsAnyLiveHumanPedTarget(cached, clone) ? cached : 0;
	}
	g_BodyguardThreatCache.Owner = owner;
	g_BodyguardThreatCache.Threat = 0;
	g_BodyguardThreatCache.Expires = now + kGuardThreatCacheInterval;
	const Vector3 ownerPos = ENTITY::GET_ENTITY_COORDS(owner, true, false);
	int best{};
	float bestDistanceSq = 3.0e30f;

	for (const auto& managed : g_ManagedClones)
	{
		if (!managed.Ped || managed.Ped == clone || managed.Mode != CloneMode::AttackOwner || !IsAnyLiveHumanPedTarget(managed.Ped, clone))
			continue;
		const float d = DistanceSquared(ownerPos, ENTITY::GET_ENTITY_COORDS(managed.Ped, true, false));
		if (d < bestDistanceSq)
		{
			bestDistanceSq = d;
			best = managed.Ped;
		}
	}
	for (int id = 0; id < 32; ++id)
	{
		if (!IsRemotePlayerThreateningOwner(id, owner))
			continue;
		const int ped = PLAYER::GET_PLAYER_PED_SCRIPT_INDEX(id);
		const float d = DistanceSquared(ownerPos, ENTITY::GET_ENTITY_COORDS(ped, true, false));
		if (d < bestDistanceSq)
		{
			bestDistanceSq = d;
			best = ped;
		}
	}
	for (const auto& ped : GetCachedPeds())
	{
		if (!ped.Handle || ped.Handle == owner || ped.Handle == clone || ped.Player || ped.Dead || !PED::IS_PED_HUMAN(ped.Handle) || IsManagedClone(ped.Handle))
			continue;
		if (!PED::IS_PED_IN_COMBAT(ped.Handle, owner) && !PED::IS_PED_IN_COMBAT(owner, ped.Handle) && !ENTITY::HAS_ENTITY_BEEN_DAMAGED_BY_ENTITY(owner, ped.Handle, true, true))
			continue;
		const float d = DistanceSquared(ownerPos, ped.Position);
		if (d < bestDistanceSq)
		{
			bestDistanceSq = d;
			best = ped.Handle;
		}
	}
	g_BodyguardThreatCache.Threat = best;
	return best;
}

void MaintainProfessionalBodyguardFollow(int clone, int owner, Clock::time_point now, Clock::time_point& nextHorse, Clock::time_point& nextFollow, bool& followIssued)
{
	if (!clone || !owner || !ENTITY::DOES_ENTITY_EXIST(clone) || !ENTITY::DOES_ENTITY_EXIST(owner))
		return;
	PED::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(clone, true);
	Vector3 clonePos = ENTITY::GET_ENTITY_COORDS(clone, true, false);
	const Vector3 ownerPos = ENTITY::GET_ENTITY_COORDS(owner, true, false);
	float dSq = DistanceSquared(clonePos, ownerPos);
	const int formation = GetBodyguardFormationIndex(clone);
	const float sideOffset = static_cast<float>((formation % 3) - 1) * 2.4f;
	const float backOffset = -2.8f - static_cast<float>(formation / 3) * 2.0f;

	if (dSq > kGuardCatchUpWarpDistance * kGuardCatchUpWarpDistance && !PED::IS_PED_IN_COMBAT(clone, 0))
	{
		const Vector3 catchUp = ENTITY::GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS(owner, sideOffset, backOffset - 5.0f, 0.35f);
		if (PED::IS_PED_ON_MOUNT(clone))
		{
			const int mount = PED::GET_MOUNT(clone);
			if (mount && ENTITY::DOES_ENTITY_EXIST(mount))
				ENTITY::SET_ENTITY_COORDS_NO_OFFSET(mount, catchUp.x, catchUp.y, catchUp.z, true, true, true);
		}
		else
		{
			ENTITY::SET_ENTITY_COORDS_NO_OFFSET(clone, catchUp.x, catchUp.y, catchUp.z, true, true, true);
		}
		clonePos = catchUp;
		dSq = DistanceSquared(clonePos, ownerPos);
		followIssued = false;
		nextFollow = now;
	}

	const bool ownerMounted = GetPlayerMountHandle() != 0;
	const bool shouldUseHorse = ownerMounted || dSq > kGuardHorseTriggerDistance * kGuardHorseTriggerDistance;
	if (shouldUseHorse && now >= nextHorse && !PED::IS_PED_IN_COMBAT(clone, 0))
	{
		ForceGuardMount(clone, owner, true);
		nextHorse = now + kGuardHorseAcquireInterval;
	}

	const bool mounted = PED::IS_PED_ON_MOUNT(clone);
	if (dSq <= kFollowStopRadius * kFollowStopRadius && !mounted)
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
	TASK::TASK_FOLLOW_TO_OFFSET_OF_ENTITY(clone, owner, sideOffset, backOffset, 0.0f, speed, -1, mounted ? 4.0f : 3.0f, true, true, false, true, true, true);
	followIssued = true;
	nextFollow = now + (isVeryFar ? 900ms : (isFar ? 1400ms : 3200ms));
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
	if (!IsAnyLiveHumanPedTarget(target, clone))
		return;
	StableEnsureCloneWeaponEquipped(clone, desiredWeapon);
	PED::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(clone, false);
	PED::SET_PED_MOVE_RATE_OVERRIDE(clone, 1.0f);
	TASK::TASK_COMBAT_PED(clone, target, 0, 16);
}

void DeleteAllManagedClonesAndMounts()
{
	for (auto& state : g_CloneSupport)
	{
		DeleteSupportHorse(state);
		state = {};
	}
	g_AggressivePerception.fill({});
	g_BodyguardThreatCache = {};
	DeleteAllManagedClones();
}

void RunSessionCloneBrawl(CloneOptions)
{
	g_SessionBrawlRunning = false;
	Notifications::Show(
	    "Tenebris",
	    Localization::IsPortuguese() ? "Batalha/Expedição foi removida da versão otimizada." : "Brawl/Expedition was removed from the optimized build.",
	    NotificationType::Info,
	    2200);
}

#define FindNearestNpcTarget(clone, owner, radius) FindProfessionalVisibleCloneTarget((clone), (radius))
#define IsValidNpcTarget(candidate, clone, owner) IsAnyLiveHumanPedTarget((candidate), (clone))
#define TaskVanillaCombat(clone, target, owner, desiredWeapon) TaskProfessionalCloneCombat((clone), (target), (desiredWeapon))
#define EnsureCloneWeaponEquipped(clone, desiredWeapon) StableEnsureCloneWeaponEquipped((clone), (desiredWeapon))
#define FindBodyguardThreat(clone, owner, radius) FindProfessionalBodyguardThreat((clone), (owner), (radius))
#define MaintainBodyguardFollow(clone, owner, now, nextHorse, nextFollow, followIssued) MaintainProfessionalBodyguardFollow((clone), (owner), (now), (nextHorse), (nextFollow), (followIssued))
#define StartCloneGunTwirl(clone, desiredWeapon, weaponName, now, endsAt) StartCloneGunTwirlUnmounted((clone), (desiredWeapon), (weaponName), (now), (endsAt))
#define PlayTimedEmote(...) PlayTimedEmoteUnmounted(__VA_ARGS__)
#define PlayRareSocialReaction(...) PlayRareSocialReactionUnmounted(__VA_ARGS__)
#define MaybeStartMourning(deadClone, deadMode) MaybeStartMourningOnFoot((deadClone), (deadMode))
#define DeleteAllManagedClones() DeleteAllManagedClonesAndMounts()
