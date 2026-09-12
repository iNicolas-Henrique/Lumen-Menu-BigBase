// Lean compatibility layer for manual clone aggression.
// The old session brawl/expedition controller was intentionally removed from the
// optimized build. This file remains as the hook immediately before RunCloneBrain.

constexpr float kAggressiveFovCosSq = 0.25f; // 120-degree horizontal field of view.
constexpr std::size_t kAggressiveLosCandidateCount = 6;

bool IsAnyLivePedTarget(int candidate, int clone)
{
	if (!candidate || candidate == clone)
		return false;
	return ENTITY::DOES_ENTITY_EXIST(candidate) && !ENTITY::IS_ENTITY_DEAD(candidate);
}

int FindNearestVisibleCloneTarget(int clone, float radius)
{
	if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone) || ENTITY::IS_ENTITY_DEAD(clone))
		return 0;

	const Vector3 origin = ENTITY::GET_ENTITY_COORDS(clone, true, false);
	const auto forward = ENTITY::GET_ENTITY_FORWARD_VECTOR(clone);
	const float radiusSq = radius * radius;

	std::array<int, kAggressiveLosCandidateCount> candidates{};
	std::array<float, kAggressiveLosCandidateCount> distances{};
	distances.fill(radiusSq);

	for (const auto& ped : GetCachedPeds())
	{
		if (!ped.Handle || ped.Handle == clone || ped.Dead)
			continue;

		const float dx = ped.Position.x - origin.x;
		const float dy = ped.Position.y - origin.y;
		const float dz = ped.Position.z - origin.z;
		const float distanceSq = dx * dx + dy * dy + dz * dz;
		if (distanceSq < 0.01f || distanceSq > radiusSq)
			continue;

		const float horizontalSq = dx * dx + dy * dy;
		if (horizontalSq > 0.001f)
		{
			const float forwardDot = forward.x * dx + forward.y * dy;
			if (forwardDot <= 0.0f || forwardDot * forwardDot < kAggressiveFovCosSq * horizontalSq)
				continue;
		}

		// Keep only a tiny nearest-candidate set. LOS natives are called after the
		// cheap cache/range/FOV pass instead of once for every PED in the pool.
		for (std::size_t slot = 0; slot < candidates.size(); ++slot)
		{
			if (distanceSq >= distances[slot])
				continue;
			for (std::size_t move = candidates.size() - 1; move > slot; --move)
			{
				candidates[move] = candidates[move - 1];
				distances[move] = distances[move - 1];
			}
			candidates[slot] = ped.Handle;
			distances[slot] = distanceSq;
			break;
		}
	}

	for (const int candidate : candidates)
	{
		if (!IsAnyLivePedTarget(candidate, clone))
			continue;
		if (ENTITY::HAS_ENTITY_CLEAR_LOS_TO_ENTITY(clone, candidate, 17))
			return candidate;
	}
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
