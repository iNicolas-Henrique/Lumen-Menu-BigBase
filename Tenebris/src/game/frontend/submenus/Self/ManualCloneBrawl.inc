struct SessionBrawlClone
{
	int Ped{};
	int SourcePlayerId{-1};
	int Team{};
	int Pair{};
	Hash SniperWeapon{};
	Vector3 Destination{};
	bool WantsSleep{};
	bool SleepDone{};
	bool Sleeping{};
	bool ReachedCity{};
	int CityTarget{};
	int CombatTarget{};
	int ObservedTarget{};
	bool ObservedTargetWasAlive{};
	Clock::time_point SleepAt{};
	Clock::time_point SleepUntil{};
	Clock::time_point ResumeAfter{};
	Clock::time_point NextTask{};
	Clock::time_point NextHorse{};
	Clock::time_point NextShot{};
	Clock::time_point CelebratingUntil{};
	Clock::time_point NextCelebrationAllowed{};
};

constexpr Vector3 kArmadilloDestination{-3687.866f, -2630.905f, -13.40f};
constexpr Vector3 kSaintDenisDestination{2519.439f, -1309.522f, 48.985f};
constexpr float kBrawlRowHalfGap = 6.0f;
constexpr float kBrawlColumnSpacing = 2.8f;
constexpr float kCityArrivalRadius = 165.0f;
constexpr float kCityHumanSearchRadius = 260.0f;
constexpr int kExpeditionSleepChancePercent = 40;
constexpr int kKillCelebrationChancePercent = 32;
constexpr int kSniperHeadBone = 21030;
constexpr int kSniperBulletDamage = 90;
constexpr auto kBrawlSpawnSpacing = 140ms;
constexpr auto kBrawlCountdown = 10s;
constexpr auto kBrawlCoordinatorTick = 250ms;
constexpr auto kBrawlRetaskInterval = 1400ms;
constexpr auto kExpeditionRouteRefresh = 6s;
constexpr auto kExpeditionHorseRefresh = 7s;
constexpr auto kExpeditionSleepDuration = 15s;
constexpr auto kExpeditionWakeBlend = 1800ms;
constexpr auto kKillCelebrationDuration = 3200ms;
constexpr auto kKillCelebrationCooldown = 18s;

// Carcano is deliberately weighted as the main rifle. The other three hashes are
// the long-range weapon variants already present in Tenebris/RDR2's weapon table.
constexpr std::array<const char*, 8> kSessionSniperWeapons = {
    "WEAPON_SNIPERRIFLE_CARCANO",
    "WEAPON_SNIPERRIFLE_CARCANO",
    "WEAPON_SNIPERRIFLE_CARCANO",
    "WEAPON_SNIPERRIFLE_CARCANO",
    "WEAPON_SNIPERRIFLE_CARCANO",
    "WEAPON_SNIPERRIFLE_ROLLINGBLOCK",
    "WEAPON_SNIPERRIFLE_ROLLINGBLOCK_EXOTIC",
    "WEAPON_SNIPERRIFLE_ROLLINGBLOCK_LENNY",
};

constexpr std::array<ManualCloneEmotes::Definition, 5> kSniperKillCelebrations = {
    ManualCloneEmotes::Definition{"KIT_EMOTE_ACTION_FIST_PUMP_1", 1},
    ManualCloneEmotes::Definition{"KIT_EMOTE_ACTION_BOAST_1", 1},
    ManualCloneEmotes::Definition{"KIT_EMOTE_REACTION_YEEHAW_1", 0},
    ManualCloneEmotes::Definition{"KIT_EMOTE_TAUNT_BEST_SHOT_1", 2},
    ManualCloneEmotes::Definition{"KIT_EMOTE_REACTION_SLOW_CLAP_1", 0},
};

Hash g_BrawlGroupA{};
Hash g_BrawlGroupB{};

bool EnsureSessionBrawlGroups()
{
	if (!g_BrawlGroupA && !PED::ADD_RELATIONSHIP_GROUP("TENEBRIS_BRAWL_A", &g_BrawlGroupA))
		return false;
	if (!g_BrawlGroupB && !PED::ADD_RELATIONSHIP_GROUP("TENEBRIS_BRAWL_B", &g_BrawlGroupB))
		return false;

	PED::SET_RELATIONSHIP_BETWEEN_GROUPS(1, g_BrawlGroupA, g_BrawlGroupA);
	PED::SET_RELATIONSHIP_BETWEEN_GROUPS(1, g_BrawlGroupB, g_BrawlGroupB);
	PED::SET_RELATIONSHIP_BETWEEN_GROUPS(5, g_BrawlGroupA, g_BrawlGroupB);
	PED::SET_RELATIONSHIP_BETWEEN_GROUPS(5, g_BrawlGroupB, g_BrawlGroupA);
	return true;
}

void MakeSessionBrawlGroupsFriendly()
{
	if (!g_BrawlGroupA || !g_BrawlGroupB)
		return;
	PED::SET_RELATIONSHIP_BETWEEN_GROUPS(1, g_BrawlGroupA, g_BrawlGroupB);
	PED::SET_RELATIONSHIP_BETWEEN_GROUPS(1, g_BrawlGroupB, g_BrawlGroupA);
}

bool IsBrawlCloneCombatReady(int ped)
{
	return ped && ENTITY::DOES_ENTITY_EXIST(ped) && !ENTITY::IS_ENTITY_DEAD(ped) && !PED::IS_PED_INCAPACITATED(ped);
}

Vector3 GetBrawlFormationPoint(int owner, float heading, int pair, int pairCount, int team)
{
	const Vector3 origin = ENTITY::GET_ENTITY_COORDS(owner, true, false);
	const float rad = heading * 0.017453292519943295f;
	const float forwardX = -std::sin(rad);
	const float forwardY = std::cos(rad);
	const float rightX = std::cos(rad);
	const float rightY = std::sin(rad);
	const float lateral = (static_cast<float>(pair) - (static_cast<float>(pairCount - 1) * 0.5f)) * kBrawlColumnSpacing;
	const float forward = team == 0 ? -kBrawlRowHalfGap : kBrawlRowHalfGap;
	return {origin.x + rightX * lateral + forwardX * forward,
	    origin.y + rightY * lateral + forwardY * forward,
	    origin.z + 0.35f};
}

Hash EquipSessionSniper(int ped)
{
	if (!ped || !ENTITY::DOES_ENTITY_EXIST(ped))
		return Joaat("WEAPON_SNIPERRIFLE_CARCANO");

	const char* weaponName = kSessionSniperWeapons[RandomInt(0, static_cast<int>(kSessionSniperWeapons.size()) - 1)];
	const Hash weapon = Joaat(weaponName);
	WEAPON::REMOVE_ALL_PED_WEAPONS(ped, true, true);
	WEAPON::GIVE_WEAPON_TO_PED(ped, weapon, 999, true, true, 0, false, 0.5f, 1.0f, 1.0f, false, 0, false);
	WEAPON::SET_CURRENT_PED_WEAPON(ped, weapon, true, 0, false, false);

	PED::SET_PED_KEEP_TASK(ped, true);
	PED::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(ped, false);
	PED::SET_PED_COMBAT_ABILITY(ped, 3);
	PED::SET_PED_ACCURACY(ped, 100);
	PED::SET_PED_COMBAT_RANGE(ped, 3);
	PED::SET_PED_COMBAT_MOVEMENT(ped, 1);
	PED::SET_PED_SEEING_RANGE(ped, 340.0f);
	PED::SET_PED_HEARING_RANGE(ped, 220.0f);
	PED::SET_PED_COMBAT_ATTRIBUTES(ped, 0, true);
	PED::SET_PED_COMBAT_ATTRIBUTES(ped, 5, true);
	PED::SET_PED_COMBAT_ATTRIBUTES(ped, 21, true);
	PED::SET_PED_COMBAT_ATTRIBUTES(ped, 23, true);
	PED::SET_PED_COMBAT_ATTRIBUTES(ped, 24, true);
	PED::SET_PED_COMBAT_ATTRIBUTES(ped, 27, true);
	PED::SET_PED_COMBAT_ATTRIBUTES(ped, 46, true);
	PED::SET_PED_COMBAT_ATTRIBUTES(ped, 50, true);
	PED::SET_PED_COMBAT_ATTRIBUTES(ped, 58, true);

	// Native AI remains responsible for combat movement/cover. Its own firing rate
	// is reduced to practically zero; the coordinator emits the deliberate single
	// sniper shot at the target's head on a staggered timer.
	PED::SET_PED_SHOOT_RATE(ped, 0);
	PED::SET_PED_FIRING_PATTERN(ped, Joaat("FIRING_PATTERN_SINGLE_SHOT"));
	return weapon;
}

void SetSniperCombatStance(int ped, bool crouched)
{
	if (!ped || !ENTITY::DOES_ENTITY_EXIST(ped) || ENTITY::IS_ENTITY_DEAD(ped))
		return;
	PED::_SET_PED_CROUCH_MOVEMENT(ped, crouched && !PED::IS_PED_ON_MOUNT(ped), 0, false);
	PED::SET_PED_COMBAT_MOVEMENT(ped, 1);
	PED::SET_PED_COMBAT_RANGE(ped, 3);
	PED::SET_PED_MOVE_RATE_OVERRIDE(ped, crouched ? 0.82f : 1.15f);
}

void TrackSniperTarget(SessionBrawlClone& state, int target)
{
	if (target == state.ObservedTarget)
		return;
	state.ObservedTarget = target;
	state.ObservedTargetWasAlive = target && ENTITY::DOES_ENTITY_EXIST(target) && !ENTITY::IS_ENTITY_DEAD(target);
}

bool UpdateKillCelebration(SessionBrawlClone& state, Clock::time_point now)
{
	if (state.CelebratingUntil != Clock::time_point{})
	{
		if (now < state.CelebratingUntil)
			return true;
		state.CelebratingUntil = {};
		if (IsBrawlCloneCombatReady(state.Ped))
		{
			WEAPON::SET_CURRENT_PED_WEAPON(state.Ped, state.SniperWeapon, true, 0, false, false);
			SetSniperCombatStance(state.Ped, state.ReachedCity || state.CombatTarget != 0);
			state.NextTask = now;
			state.NextShot = now + std::chrono::milliseconds(RandomInt(900, 1700));
		}
	}

	if (!state.ObservedTarget || !state.ObservedTargetWasAlive || !ENTITY::DOES_ENTITY_EXIST(state.ObservedTarget))
		return false;
	if (!ENTITY::IS_ENTITY_DEAD(state.ObservedTarget))
		return false;

	state.ObservedTargetWasAlive = false;
	const bool confirmed = ENTITY::HAS_ENTITY_BEEN_DAMAGED_BY_ENTITY(state.ObservedTarget, state.Ped, true, true);
	if (!confirmed || now < state.NextCelebrationAllowed || RandomInt(1, 100) > kKillCelebrationChancePercent)
		return false;

	if (!IsBrawlCloneCombatReady(state.Ped))
		return false;
	TASK::CLEAR_PED_TASKS(state.Ped, true, false);
	PED::_SET_PED_CROUCH_MOVEMENT(state.Ped, false, 0, false);
	WEAPON::SET_CURRENT_PED_WEAPON(state.Ped, Joaat("WEAPON_UNARMED"), true, 0, false, false);
	PlayFullBodyEmote(state.Ped, kSniperKillCelebrations[RandomInt(0, static_cast<int>(kSniperKillCelebrations.size()) - 1)]);
	state.CelebratingUntil = now + kKillCelebrationDuration;
	state.NextCelebrationAllowed = now + kKillCelebrationCooldown;
	state.CombatTarget = 0;
	state.CityTarget = 0;
	return true;
}

void FireGuaranteedSniperHeadshot(SessionBrawlClone& state, int target, Clock::time_point now)
{
	if (!IsBrawlCloneCombatReady(state.Ped) || !target || !ENTITY::DOES_ENTITY_EXIST(target) || ENTITY::IS_ENTITY_DEAD(target))
		return;
	if (state.CelebratingUntil != Clock::time_point{} || now < state.NextShot)
		return;

	SetSniperCombatStance(state.Ped, true);
	WEAPON::SET_CURRENT_PED_WEAPON(state.Ped, state.SniperWeapon, true, 0, false, false);
	FaceEntity(state.Ped, target);
	const Vector3 head = PED::GET_PED_BONE_COORDS(target, kSniperHeadBone, 0.0f, 0.0f, 0.0f);
	const Vector3 muzzle = ENTITY::GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS(state.Ped, 0.12f, 0.72f, 1.35f);
	TASK::TASK_AIM_GUN_AT_COORD(state.Ped, head.x, head.y, head.z, 520, true, true);
	MISC::SHOOT_SINGLE_BULLET_BETWEEN_COORDS(muzzle.x,
	    muzzle.y,
	    muzzle.z,
	    head.x,
	    head.y,
	    head.z,
	    kSniperBulletDamage,
	    true,
	    state.SniperWeapon,
	    state.Ped,
	    true,
	    false,
	    1000.0f,
	    true);
	TrackSniperTarget(state, target);
	state.NextShot = now + std::chrono::milliseconds(RandomInt(2800, 4400));
	state.NextTask = now + 650ms;
}

int FindNearestOpposingClone(const SessionBrawlClone& state, const std::vector<SessionBrawlClone>& states)
{
	if (!IsBrawlCloneCombatReady(state.Ped))
		return 0;
	const Vector3 origin = ENTITY::GET_ENTITY_COORDS(state.Ped, true, false);
	int best{};
	float bestDistanceSq = 500.0f * 500.0f;
	for (const auto& candidate : states)
	{
		if (candidate.Team == state.Team || !IsBrawlCloneCombatReady(candidate.Ped))
			continue;
		const float d = DistanceSquared(origin, ENTITY::GET_ENTITY_COORDS(candidate.Ped, true, false));
		if (d < bestDistanceSq)
		{
			bestDistanceSq = d;
			best = candidate.Ped;
		}
	}
	return best;
}

int FindNearestHumanNpcTarget(int clone, int owner, float radius)
{
	if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone))
		return 0;
	const Vector3 origin = ENTITY::GET_ENTITY_COORDS(clone, true, false);
	const float radiusSq = radius * radius;
	const int ownerMount = GetPlayerMountHandle();
	int best{};
	float bestDistanceSq = radiusSq;
	for (const auto& ped : GetCachedPeds())
	{
		if (!ped.Handle || ped.Handle == clone || ped.Handle == owner || ped.Handle == ownerMount || ped.Player || ped.Dead || IsManagedClone(ped.Handle))
			continue;
		if (!PED::IS_PED_HUMAN(ped.Handle))
			continue;
		const float distanceSq = DistanceSquared(origin, ped.Position);
		if (distanceSq < bestDistanceSq)
		{
			bestDistanceSq = distanceSq;
			best = ped.Handle;
		}
	}
	return IsValidNpcTarget(best, clone, owner) && PED::IS_PED_HUMAN(best) ? best : 0;
}

bool IsValidCityTarget(int target, int clone, int owner)
{
	return IsValidNpcTarget(target, clone, owner) && PED::IS_PED_HUMAN(target);
}

void StartExpeditionSleep(SessionBrawlClone& state)
{
	if (!IsBrawlCloneCombatReady(state.Ped))
		return;
	PED::_SET_PED_CROUCH_MOVEMENT(state.Ped, false, 0, false);
	WEAPON::SET_CURRENT_PED_WEAPON(state.Ped, Joaat("WEAPON_UNARMED"), true, 0, false, false);
	TASK::CLEAR_PED_TASKS(state.Ped, true, false);
	const float heading = ENTITY::GET_ENTITY_HEADING(state.Ped);
	TASK::TASK_START_SCENARIO_IN_PLACE_HASH(state.Ped, Joaat("WORLD_HUMAN_SLEEP_GROUND_ARM"), -1, true, 0, heading, false);
	state.Sleeping = true;
	state.SleepUntil = Clock::now() + kExpeditionSleepDuration;
	state.CityTarget = 0;
	state.CombatTarget = 0;
}

void WakeExpeditionClone(SessionBrawlClone& state, Clock::time_point now)
{
	if (!state.Ped || !ENTITY::DOES_ENTITY_EXIST(state.Ped))
		return;
	TASK::CLEAR_PED_TASKS(state.Ped, true, false);
	state.Sleeping = false;
	state.SleepDone = true;
	state.ResumeAfter = now + kExpeditionWakeBlend;
	state.NextTask = state.ResumeAfter;
}

void RestoreExpeditionWeaponAfterSleep(SessionBrawlClone& state)
{
	if (!state.Ped || !ENTITY::DOES_ENTITY_EXIST(state.Ped) || ENTITY::IS_ENTITY_DEAD(state.Ped))
		return;
	WEAPON::SET_CURRENT_PED_WEAPON(state.Ped, state.SniperWeapon, true, 0, false, false);
	PED::_SET_PED_CROUCH_MOVEMENT(state.Ped, false, 0, false);
	state.ResumeAfter = {};
}

void RouteExpeditionClone(SessionBrawlClone& state, int owner, Clock::time_point now)
{
	if (!IsBrawlCloneCombatReady(state.Ped))
		return;
	if (UpdateKillCelebration(state, now))
		return;

	if (state.Sleeping)
	{
		if (now >= state.SleepUntil)
			WakeExpeditionClone(state, now);
		return;
	}
	if (state.ResumeAfter != Clock::time_point{})
	{
		if (now < state.ResumeAfter)
			return;
		RestoreExpeditionWeaponAfterSleep(state);
	}

	if (state.WantsSleep && !state.SleepDone && now >= state.SleepAt && !state.ReachedCity && !PED::IS_PED_IN_COMBAT(state.Ped, 0))
	{
		if (PED::IS_PED_ON_MOUNT(state.Ped))
		{
			if (now >= state.NextTask)
			{
				TASK::TASK_DISMOUNT_ANIMAL(state.Ped, 0, 0, 0, 0, 0);
				state.NextTask = now + 1600ms;
			}
			return;
		}
		StartExpeditionSleep(state);
		return;
	}

	const Vector3 position = ENTITY::GET_ENTITY_COORDS(state.Ped, true, false);
	const float destinationDistanceSq = DistanceSquared(position, state.Destination);
	if (!state.ReachedCity && destinationDistanceSq <= kCityArrivalRadius * kCityArrivalRadius)
	{
		state.ReachedCity = true;
		state.CityTarget = 0;
		state.CombatTarget = 0;
		TASK::CLEAR_PED_TASKS(state.Ped, true, false);
		SetSniperCombatStance(state.Ped, true);
		state.NextTask = now;
		state.NextShot = now + std::chrono::milliseconds(RandomInt(900, 2000));
	}

	if (!state.ReachedCity)
	{
		PED::_SET_PED_CROUCH_MOVEMENT(state.Ped, false, 0, false);
		if (!PED::IS_PED_ON_MOUNT(state.Ped) && destinationDistanceSq > 220.0f * 220.0f && now >= state.NextHorse)
		{
			const int horse = FindNearestFreeHorse(state.Ped, owner);
			state.NextHorse = now + kExpeditionHorseRefresh;
			if (horse)
			{
				TASK::TASK_MOUNT_ANIMAL(state.Ped, horse, 5000, -1, 2.0f, 1, 0, 0);
				state.NextTask = now + 3200ms;
				return;
			}
		}
		if (now >= state.NextTask)
		{
			TASK::TASK_FOLLOW_NAV_MESH_TO_COORD(state.Ped,
			    state.Destination.x, state.Destination.y, state.Destination.z,
			    PED::IS_PED_ON_MOUNT(state.Ped) ? 4.0f : 3.0f,
			    -1, 16.0f, 1, 0.0f);
			state.NextTask = now + kExpeditionRouteRefresh;
		}
		return;
	}

	if (state.CityTarget && (!ENTITY::DOES_ENTITY_EXIST(state.CityTarget) || ENTITY::IS_ENTITY_DEAD(state.CityTarget)))
	{
		TrackSniperTarget(state, state.CityTarget);
		UpdateKillCelebration(state, now);
		state.CityTarget = 0;
		state.CombatTarget = 0;
		if (state.CelebratingUntil != Clock::time_point{})
			return;
	}
	if (state.CityTarget && !IsValidCityTarget(state.CityTarget, state.Ped, owner))
	{
		state.CityTarget = 0;
		state.CombatTarget = 0;
	}
	if (state.CityTarget && ENTITY::DOES_ENTITY_EXIST(state.CityTarget))
	{
		const float targetDistanceSq = DistanceSquared(ENTITY::GET_ENTITY_COORDS(state.Ped, true, false), ENTITY::GET_ENTITY_COORDS(state.CityTarget, true, false));
		if (targetDistanceSq > 300.0f * 300.0f)
		{
			state.CityTarget = 0;
			state.CombatTarget = 0;
		}
	}

	if (!state.CityTarget && now >= state.NextTask)
	{
		state.CityTarget = FindNearestHumanNpcTarget(state.Ped, owner, kCityHumanSearchRadius);
		state.CombatTarget = state.CityTarget;
		if (state.CityTarget)
		{
			TrackSniperTarget(state, state.CityTarget);
			SetSniperCombatStance(state.Ped, true);
			TASK::TASK_COMBAT_PED(state.Ped, state.CityTarget, 0, 16);
			state.NextTask = now + kBrawlRetaskInterval;
		}
		else
		{
			TASK::TASK_WANDER_IN_AREA(state.Ped, state.Destination.x, state.Destination.y, state.Destination.z, 145.0f, 1.2f, 8.0f, 0);
			state.NextTask = now + 4s;
		}
	}
	else if (state.CityTarget && now >= state.NextTask)
	{
		SetSniperCombatStance(state.Ped, true);
		TASK::TASK_COMBAT_PED(state.Ped, state.CityTarget, 0, 16);
		state.NextTask = now + kBrawlRetaskInterval;
	}

	if (state.CityTarget)
		FireGuaranteedSniperHeadshot(state, state.CityTarget, now);
}

void FinishSessionBrawlRun(std::uint32_t generation)
{
	if (generation == g_BrawlGeneration)
		g_SessionBrawlRunning = false;
}

void RunSessionCloneBrawl(CloneOptions options)
{
	if (g_SessionBrawlRunning)
	{
		Notifications::Show("Tenebris", Localization::IsPortuguese() ? "Já existe uma batalha de clones em andamento. Apague os clones para cancelar." : "A session clone brawl is already running. Delete all clones to cancel it.", NotificationType::Warning, 3000);
		return;
	}

	auto self = Self::GetPed();
	if (!self.IsValid() || self.GetHealth() <= 0)
		return;
	const int owner = self.GetHandle();

	std::vector<int> playerIds;
	playerIds.reserve(32);
	const int localId = PLAYER::PLAYER_ID();
	for (int id = 0; id < 32; ++id)
	{
		if (id != localId && !NETWORK::NETWORK_IS_PLAYER_ACTIVE(id))
			continue;
		if (IsSourcePlayerStable(id))
			playerIds.push_back(id);
	}
	if (playerIds.size() < 2)
	{
		Notifications::Show("Tenebris", Localization::IsPortuguese() ? "São necessários pelo menos 2 jogadores válidos na sessão para formar a batalha." : "At least 2 valid session players are required to form the brawl.", NotificationType::Warning, 3000);
		return;
	}
	if (!EnsureSessionBrawlGroups())
	{
		Notifications::Show("Tenebris", Localization::IsPortuguese() ? "Não foi possível preparar os grupos de combate da IA." : "Could not prepare the AI combat relationship groups.", NotificationType::Warning, 3000);
		return;
	}

	g_SessionBrawlRunning = true;
	const std::uint32_t generation = ++g_BrawlGeneration;
	const float ownerHeading = ENTITY::GET_ENTITY_HEADING(owner);
	const int pairCount = static_cast<int>((playerIds.size() + 1) / 2);
	std::vector<SessionBrawlClone> states;
	states.reserve(playerIds.size());

	Notifications::Show("Tenebris", Localization::IsPortuguese() ? "Clonando a sessão em etapas e preparando os atiradores..." : "Cloning the session in staggered steps and preparing the snipers...", NotificationType::Info, 3200);

	for (std::size_t index = 0; index < playerIds.size() && generation == g_BrawlGeneration; ++index)
	{
		const int sourcePlayerId = playerIds[index];
		const int sourcePed = ResolveSourcePed(sourcePlayerId);
		if (!sourcePed)
			continue;
		const int team = static_cast<int>(index & 1u);
		const int pair = static_cast<int>(index / 2u);
		const Vector3 spawn = GetBrawlFormationPoint(owner, ownerHeading, pair, pairCount, team);
		float spawnHeading = ownerHeading + (team == 0 ? 0.0f : 180.0f);
		if (spawnHeading >= 360.0f)
			spawnHeading -= 360.0f;

		int clone = CreateCloneShell(sourcePlayerId, sourcePed, spawn, spawnHeading, options.Networked);
		if (!clone)
			continue;
		ENTITY::PLACE_ENTITY_ON_GROUND_PROPERLY(clone, true);
		ENTITY::SET_ENTITY_MAX_HEALTH(clone, kCloneHealth);
		ENTITY::SET_ENTITY_HEALTH(clone, kCloneHealth, 0);
		ApplyPlayerPromptName(clone, sourcePlayerId);
		ConfigureBleedout(clone);
		ConfigureCloneLoot(clone);
		const Hash sniper = EquipSessionSniper(clone);
		PED::SET_PED_RELATIONSHIP_GROUP_HASH(clone, team == 0 ? g_BrawlGroupA : g_BrawlGroupB);
		SetSniperCombatStance(clone, true);
		ENTITY::CLEAR_ENTITY_LAST_DAMAGE_ENTITY(clone);
		ENTITY::FREEZE_ENTITY_POSITION(clone, true);
		g_ManagedClones.push_back({clone, sourcePlayerId, CloneMode::BrawlExpedition, options.Networked, {}});
		g_NextPedCacheRefresh = {};
		++g_TotalSpawned;

		SessionBrawlClone state{};
		state.Ped = clone;
		state.SourcePlayerId = sourcePlayerId;
		state.Team = team;
		state.Pair = pair;
		state.SniperWeapon = sniper;
		state.NextTask = Clock::now();
		state.NextHorse = Clock::now();
		states.push_back(state);
		ScriptMgr::Yield(kBrawlSpawnSpacing);
	}

	if (generation != g_BrawlGeneration)
		return;
	if (states.size() < 2)
	{
		for (auto& state : states)
			if (state.Ped && ENTITY::DOES_ENTITY_EXIST(state.Ped)) ENTITY::FREEZE_ENTITY_POSITION(state.Ped, false);
		FinishSessionBrawlRun(generation);
		Notifications::Show("Tenebris", Localization::IsPortuguese() ? "Menos de 2 clones puderam ser criados; batalha cancelada." : "Fewer than 2 clones could be created; brawl cancelled.", NotificationType::Warning, 3000);
		return;
	}

	for (auto& state : states)
	{
		const int opponent = FindNearestOpposingClone(state, states);
		if (opponent)
		{
			FaceEntity(state.Ped, opponent);
			TrackSniperTarget(state, opponent);
		}
	}

	Notifications::Show("Tenebris", Localization::IsPortuguese() ? "Atiradores em duas fileiras. Em 10 segundos começa o duelo de precisão." : "Snipers are lined up in two rows. The precision duel starts in 10 seconds.", NotificationType::Info, 5000);
	const auto fightStartsAt = Clock::now() + kBrawlCountdown;
	while (generation == g_BrawlGeneration && Clock::now() < fightStartsAt)
		ScriptMgr::Yield(100ms);
	if (generation != g_BrawlGeneration)
		return;

	const auto brawlStartedAt = Clock::now();
	for (std::size_t index = 0; index < states.size(); ++index)
	{
		auto& state = states[index];
		if (!IsBrawlCloneCombatReady(state.Ped))
			continue;
		ENTITY::FREEZE_ENTITY_POSITION(state.Ped, false);
		SetSniperCombatStance(state.Ped, true);
		state.CombatTarget = FindNearestOpposingClone(state, states);
		TrackSniperTarget(state, state.CombatTarget);
		if (state.CombatTarget)
			TASK::TASK_COMBAT_PED(state.Ped, state.CombatTarget, 0, 16);
		state.NextTask = brawlStartedAt + std::chrono::milliseconds(RandomInt(700, 1600));
		state.NextShot = brawlStartedAt + std::chrono::milliseconds(RandomInt(500, 1900));
	}

	Notifications::Show("Tenebris", Localization::IsPortuguese() ? "DUELO! IA normal cuida da movimentação e cobertura; os disparos são lentos e miram a cabeça." : "DUEL! Stock AI handles movement and cover; shots are slow and aimed at the head.", NotificationType::Success, 4300);

	while (generation == g_BrawlGeneration)
	{
		int teamAlive[2]{};
		const auto now = Clock::now();
		for (auto& state : states)
		{
			if (state.Ped && ENTITY::DOES_ENTITY_EXIST(state.Ped) && ENTITY::IS_ENTITY_DEAD(state.Ped))
			{
				MarkCloneDead(state.Ped);
				continue;
			}
			if (!IsBrawlCloneCombatReady(state.Ped))
				continue;
			++teamAlive[state.Team];

			if (UpdateKillCelebration(state, now))
				continue;
			if (state.CombatTarget && (!ENTITY::DOES_ENTITY_EXIST(state.CombatTarget) || ENTITY::IS_ENTITY_DEAD(state.CombatTarget) || PED::IS_PED_INCAPACITATED(state.CombatTarget)))
			{
				TrackSniperTarget(state, state.CombatTarget);
				UpdateKillCelebration(state, now);
				state.CombatTarget = 0;
				if (state.CelebratingUntil != Clock::time_point{})
					continue;
			}
			if (!state.CombatTarget)
			{
				state.CombatTarget = FindNearestOpposingClone(state, states);
				TrackSniperTarget(state, state.CombatTarget);
				state.NextTask = now;
			}
			if (!state.CombatTarget)
				continue;

			if (now >= state.NextTask)
			{
				SetSniperCombatStance(state.Ped, true);
				TASK::TASK_COMBAT_PED(state.Ped, state.CombatTarget, 0, 16);
				state.NextTask = now + kBrawlRetaskInterval;
			}
			FireGuaranteedSniperHeadshot(state, state.CombatTarget, now);
		}
		if (teamAlive[0] == 0 || teamAlive[1] == 0)
			break;
		ScriptMgr::Yield(kBrawlCoordinatorTick);
	}
	if (generation != g_BrawlGeneration)
		return;

	MakeSessionBrawlGroupsFriendly();
	const auto expeditionStarts = Clock::now();
	int survivors{};
	for (auto& state : states)
	{
		if (!IsBrawlCloneCombatReady(state.Ped))
			continue;
		++survivors;
		state.CelebratingUntil = {};
		TASK::CLEAR_PED_TASKS(state.Ped, true, false);
		WEAPON::SET_CURRENT_PED_WEAPON(state.Ped, state.SniperWeapon, true, 0, false, false);
		PED::_SET_PED_CROUCH_MOVEMENT(state.Ped, false, 0, false);
		state.CombatTarget = 0;
		state.CityTarget = 0;
		state.ObservedTarget = 0;
		state.ObservedTargetWasAlive = false;
		state.Destination = RandomInt(0, 1) == 0 ? kArmadilloDestination : kSaintDenisDestination;
		state.WantsSleep = RandomInt(1, 100) <= kExpeditionSleepChancePercent;
		state.SleepDone = !state.WantsSleep;
		state.SleepAt = expeditionStarts + std::chrono::seconds(RandomInt(12, 45));
		state.NextTask = expeditionStarts;
		state.NextHorse = expeditionStarts + std::chrono::seconds(RandomInt(0, 3));
		state.NextShot = expeditionStarts + std::chrono::milliseconds(RandomInt(800, 2200));
	}

	if (!survivors)
	{
		FinishSessionBrawlRun(generation);
		Notifications::Show("Tenebris", Localization::IsPortuguese() ? "A batalha terminou sem clones aptos a continuar." : "The brawl ended with no clones able to continue.", NotificationType::Info, 3000);
		return;
	}

	Notifications::Show("Tenebris", Localization::IsPortuguese() ? "Sobreviventes seguem com rifles de precisão para Armadillo ou Saint Denis. Alguns ainda vão dormir 15 s uma única vez no caminho." : "Survivors are heading to Armadillo or Saint Denis with sniper rifles. Some will still sleep for 15 seconds once on the way.", NotificationType::Success, 5400);

	while (generation == g_BrawlGeneration)
	{
		const auto now = Clock::now();
		bool anyLiving{};
		for (auto& state : states)
		{
			if (!state.Ped || !ENTITY::DOES_ENTITY_EXIST(state.Ped))
				continue;
			if (ENTITY::IS_ENTITY_DEAD(state.Ped))
			{
				MarkCloneDead(state.Ped);
				continue;
			}
			if (PED::IS_PED_INCAPACITATED(state.Ped))
				continue;
			anyLiving = true;
			RouteExpeditionClone(state, owner, now);
		}
		if (!anyLiving)
			break;
		ScriptMgr::Yield(kBrawlCoordinatorTick);
	}

	FinishSessionBrawlRun(generation);
}
