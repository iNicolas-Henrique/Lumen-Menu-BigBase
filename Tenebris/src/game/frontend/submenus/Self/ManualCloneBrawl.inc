// Lean compatibility layer for manual clone aggression.
// The old session brawl/expedition controller was intentionally removed from the
// optimized build. This file remains as the hook immediately before RunCloneBrain.

// Perception is deliberately math-heavy only in the cheap broad phase. Expensive
// LOS natives are budgeted to a tiny ranked candidate set.
constexpr float kAggressiveHorizontalCosSq = 0.25f; // 120-degree horizontal FOV.
constexpr float kAggressiveVerticalTanSq = 0.7040882f; // ~80-degree vertical FOV.
constexpr std::size_t kAggressiveLosCandidateCount = 5;
constexpr auto kAggressiveTargetMemory = 550ms;
constexpr auto kAggressiveMinScanInterval = 520ms;
constexpr float kAggressiveSwitchMargin = 0.14f;
constexpr float kAggressiveMemoryLeash = 1.20f;

struct AggressivePerceptionState
{
	int Clone{};
	int Target{};
	float LastScore{};
	Clock::time_point LastSeen{};
	Clock::time_point NextScan{};
};

std::array<AggressivePerceptionState, kMaxActiveClones> g_AggressivePerception{};

bool IsAnyLivePedTarget(int candidate, int clone)
{
	if (!candidate || candidate == clone)
		return false;
	return ENTITY::DOES_ENTITY_EXIST(candidate) && !ENTITY::IS_ENTITY_DEAD(candidate);
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

	// There are at most kMaxActiveClones managed clones. This fallback is only
	// reachable if stale external handles survive unexpectedly.
	g_AggressivePerception[0] = {};
	g_AggressivePerception[0].Clone = clone;
	return g_AggressivePerception[0];
}

float ScoreAggressiveCandidate(float distanceSq, float radiusSq, float forwardDot, float horizontalSq, bool currentTarget)
{
	const float distanceScore = std::clamp(1.0f - (distanceSq / radiusSq), 0.0f, 1.0f);
	const float alignmentSq = horizontalSq > 0.001f ? std::clamp((forwardDot * forwardDot) / horizontalSq, 0.0f, 1.0f) : 1.0f;
	const float alignmentScore = std::clamp((alignmentSq - kAggressiveHorizontalCosSq) / (1.0f - kAggressiveHorizontalCosSq), 0.0f, 1.0f);

	// Central targets are preferred, but distance still matters. The retention
	// bonus implements hysteresis so tiny score changes do not cause target spam.
	return alignmentScore * 0.58f + distanceScore * 0.42f + (currentTarget ? 0.12f : 0.0f);
}

bool IsTargetInsideMemoryLeash(int clone, int target, float radius)
{
	if (!IsAnyLivePedTarget(target, clone))
		return false;
	const Vector3 a = ENTITY::GET_ENTITY_COORDS(clone, true, false);
	const Vector3 b = ENTITY::GET_ENTITY_COORDS(target, true, false);
	const float dx = b.x - a.x;
	const float dy = b.y - a.y;
	const float dz = b.z - a.z;
	const float leash = radius * kAggressiveMemoryLeash;
	return dx * dx + dy * dy + dz * dz <= leash * leash;
}

int FindNearestVisibleCloneTarget(int clone, float radius)
{
	if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone) || ENTITY::IS_ENTITY_DEAD(clone))
		return 0;

	auto& state = GetAggressivePerceptionState(clone);
	const auto now = Clock::now();

	// Temporal coherence: if this brain asks again before its staggered scan slot,
	// keep the recent target instead of repeating the whole perception pass.
	if (state.Target && now < state.NextScan && now - state.LastSeen <= kAggressiveTargetMemory && IsTargetInsideMemoryLeash(clone, state.Target, radius))
		return state.Target;

	// Small handle-derived offset prevents several clones from doing LOS work on
	// the same game tick even if their brains were spawned together.
	const auto stagger = std::chrono::milliseconds((clone & 0x7) * 23);
	state.NextScan = now + kAggressiveMinScanInterval + stagger;

	const Vector3 origin = ENTITY::GET_ENTITY_COORDS(clone, true, false);
	const auto forward = ENTITY::GET_ENTITY_FORWARD_VECTOR(clone);
	const float radiusSq = radius * radius;

	std::array<int, kAggressiveLosCandidateCount> candidates{};
	std::array<float, kAggressiveLosCandidateCount> scores{};
	scores.fill(-1.0f);

	for (const auto& ped : GetCachedPeds())
	{
		if (!ped.Handle || ped.Handle == clone || ped.Dead)
			continue;

		const float dx = ped.Position.x - origin.x;
		const float dy = ped.Position.y - origin.y;
		const float dz = ped.Position.z - origin.z;
		const float horizontalSq = dx * dx + dy * dy;
		const float distanceSq = horizontalSq + dz * dz;
		if (distanceSq < 0.01f || distanceSq > radiusSq)
			continue;

		// Horizontal FOV without sqrt/acos: compare squared dot product against a
		// precomputed cosine threshold. Targets behind the clone are rejected first.
		const float forwardDot = forward.x * dx + forward.y * dy;
		if (forwardDot <= 0.0f)
			continue;
		if (horizontalSq > 0.001f && forwardDot * forwardDot < kAggressiveHorizontalCosSq * horizontalSq)
			continue;

		// Vertical FOV is also squared: |z| <= tan(halfFov) * horizontalDistance.
		// It prevents targets far above/below the clone from being treated as seen.
		if (horizontalSq <= 0.001f)
		{
			if (dz * dz > 1.0f)
				continue;
		}
		else if (dz * dz > kAggressiveVerticalTanSq * horizontalSq)
		{
			continue;
		}

		const float score = ScoreAggressiveCandidate(distanceSq, radiusSq, forwardDot, horizontalSq, ped.Handle == state.Target);

		// Top-K insertion by score. Only these candidates reach the expensive LOS
		// native; every other PED was handled entirely with cached coordinates/math.
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
	float bestScore = -1.0f;
	float currentVisibleScore = -1.0f;

	for (std::size_t i = 0; i < candidates.size(); ++i)
	{
		const int candidate = candidates[i];
		if (!IsAnyLivePedTarget(candidate, clone))
			continue;
		if (!ENTITY::HAS_ENTITY_CLEAR_LOS_TO_ENTITY(clone, candidate, 17))
			continue;

		if (candidate == state.Target)
			currentVisibleScore = scores[i];
		if (scores[i] > bestScore)
		{
			bestScore = scores[i];
			bestTarget = candidate;
		}
	}

	// Hysteresis: a challenger must be meaningfully better before replacing a
	// still-visible target. This prevents rapid target swapping in crowds.
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

	// Short occlusion memory: preserve the last target through momentary poles,
	// doors, animation crossings, etc. without performing extra LOS calls.
	if (state.Target && now - state.LastSeen <= kAggressiveTargetMemory && IsTargetInsideMemoryLeash(clone, state.Target, radius))
		return state.Target;

	state.Target = 0;
	state.LastScore = 0.0f;
	state.LastSeen = {};
	return 0;
}

void TaskAggressiveCloneCombat(int clone, int target, Hash desiredWeapon)
{
	if (!IsAnyLivePedTarget(target, clone))
		return;
	EnsureCloneWeaponEquipped(clone, desiredWeapon);
	PED::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(clone, false);
	PED::SET_PED_MOVE_RATE_OVERRIDE(clone, 1.0f);
	TASK::TASK_COMBAT_PED(clone, target, 0, 16);
}

// Legacy symbol retained only so the old hidden editor implementation still
// compiles. The optimized editor no longer exposes Brawl/Expedition.
void RunSessionCloneBrawl(CloneOptions)
{
	g_SessionBrawlRunning = false;
	Notifications::Show(
	    "Tenebris",
	    Localization::IsPortuguese() ? "Batalha/Expedição foi removida da versão otimizada." : "Brawl/Expedition was removed from the optimized build.",
	    NotificationType::Info,
	    2200);
}

// These substitutions start here, after the original helper definitions, and
// therefore affect only RunCloneBrain and its following manual-clone path.
// Bodyguards still acquire targets through FindBodyguardThreat, so their
// protection/follow logic is not widened to all visible PEDs.
#define FindNearestNpcTarget(clone, owner, radius) FindNearestVisibleCloneTarget((clone), (radius))
#define IsValidNpcTarget(candidate, clone, owner) IsAnyLivePedTarget((candidate), (clone))
#define TaskVanillaCombat(clone, target, owner, desiredWeapon) TaskAggressiveCloneCombat((clone), (target), (desiredWeapon))
