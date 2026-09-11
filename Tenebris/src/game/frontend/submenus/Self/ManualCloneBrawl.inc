struct SessionBrawlClone
{
	int Ped{};
	int SourcePlayerId{-1};
	int Team{};
	int Pair{};
	Hash TravelWeapon{};
	Vector3 Destination{};
	bool WantsSleep{};
	bool SleepDone{};
	bool Sleeping{};
	bool ReachedCity{};
	int CityTarget{};
	Clock::time_point SleepAt{};
	Clock::time_point SleepUntil{};
	Clock::time_point ResumeAfter{};
	Clock::time_point NextTask{};
	Clock::time_point NextHorse{};
};

constexpr Vector3 kArmadilloDestination{-3687.866f, -2630.905f, -13.40f};
constexpr Vector3 kSaintDenisDestination{2519.439f, -1309.522f, 48.985f};
constexpr float kBrawlRowHalfGap = 6.0f;
constexpr float kBrawlColumnSpacing = 2.8f;
constexpr float kCityArrivalRadius = 165.0f;
constexpr float kCityHumanSearchRadius = 230.0f;
constexpr int kExpeditionSleepChancePercent = 40;
constexpr auto kBrawlSpawnSpacing = 140ms;
constexpr auto kBrawlCountdown = 10s;
constexpr auto kBrawlCoordinatorTick = 250ms;
constexpr auto kBrawlRetaskInterval = 1800ms;
constexpr auto kExpeditionRouteRefresh = 6s;
constexpr auto kExpeditionHorseRefresh = 7s;
constexpr auto kExpeditionSleepDuration = 15s;
constexpr auto kExpeditionWakeBlend = 1800ms;

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
	Hash currentWeapon{};
	if (GetCurrentWeapon(state.Ped, currentWeapon) && currentWeapon != Joaat("WEAPON_UNARMED"))
		WEAPON::SET_CURRENT_PED_WEAPON(state.Ped, Joaat("WEAPON_UNARMED"), true, 0, false, false);
	TASK::CLEAR_PED_TASKS(state.Ped, true, false);
	const float heading = ENTITY::GET_ENTITY_HEADING(state.Ped);
	TASK::TASK_START_SCENARIO_IN_PLACE_HASH(state.Ped, Joaat("WORLD_HUMAN_SLEEP_GROUND_ARM"), -1, true, 0, heading, false);
	state.Sleeping = true;
	state.SleepUntil = Clock::now() + kExpeditionSleepDuration;
	state.CityTarget = 0;
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
	if (state.TravelWeapon != Joaat("WEAPON_UNARMED"))
	{
		Hash current{};
		if (GetCurrentWeapon(state.Ped, current) && current == Joaat("WEAPON_UNARMED"))
			WEAPON::SET_CURRENT_PED_WEAPON(state.Ped, state.TravelWeapon, true, 0, false, false);
	}
	state.ResumeAfter = {};
}

void RouteExpeditionClone(SessionBrawlClone& state, int owner, Clock::time_point now)
{
	if (!IsBrawlCloneCombatReady(state.Ped))
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
		TASK::CLEAR_PED_TASKS(state.Ped, true, false);
		state.NextTask = now;
	}

	if (!state.ReachedCity)
	{
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

	if (state.CityTarget && !IsValidCityTarget(state.CityTarget, state.Ped, owner))
		state.CityTarget = 0;
	if (state.CityTarget && ENTITY::DOES_ENTITY_EXIST(state.CityTarget))
	{
		const float targetDistanceSq = DistanceSquared(ENTITY::GET_ENTITY_COORDS(state.Ped, true, false), ENTITY::GET_ENTITY_COORDS(state.CityTarget, true, false));
		if (targetDistanceSq > 280.0f * 280.0f)
			state.CityTarget = 0;
	}

	if (!state.CityTarget && now >= state.NextTask)
	{
		state.CityTarget = FindNearestHumanNpcTarget(state.Ped, owner, kCityHumanSearchRadius);
		if (state.CityTarget)
		{
			TASK::TASK_COMBAT_PED(state.Ped, state.CityTarget, 0, 16);
			state.NextTask = now + 2s;
		}
		else
		{
			TASK::TASK_WANDER_IN_AREA(state.Ped, state.Destination.x, state.Destination.y, state.Destination.z, 145.0f, 2.0f, 8.0f, 0);
			state.NextTask = now + 4s;
		}
	}
	else if (state.CityTarget && now >= state.NextTask)
	{
		if (!PED::IS_PED_IN_COMBAT(state.Ped, state.CityTarget))
			TASK::TASK_COMBAT_PED(state.Ped, state.CityTarget, 0, 16);
		state.NextTask = now + 2s;
	}
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
	options.WeaponIndex = std::clamp(options.WeaponIndex, 0, static_cast<int>(kCloneWeapons.size()) - 1);
	const Hash travelWeapon = Joaat(kCloneWeapons[options.WeaponIndex].HashName);
	std::vector<SessionBrawlClone> states;
	states.reserve(playerIds.size());

	Notifications::Show("Tenebris", Localization::IsPortuguese() ? "Clonando a sessão em etapas para evitar travadas..." : "Cloning the session in staggered steps to avoid hitches...", NotificationType::Info, 3200);

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
		EquipCloneWeapon(clone, 0);
		ConfigureCloneCombat(clone, CloneMode::BrawlExpedition, "WEAPON_UNARMED");
		PED::SET_PED_RELATIONSHIP_GROUP_HASH(clone, team == 0 ? g_BrawlGroupA : g_BrawlGroupB);
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
		state.TravelWeapon = travelWeapon;
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

	for (auto& left : states)
	{
		if (left.Team != 0 || !IsBrawlCloneCombatReady(left.Ped))
			continue;
		for (auto& right : states)
		{
			if (right.Team == 1 && right.Pair == left.Pair && IsBrawlCloneCombatReady(right.Ped))
			{
				FaceEntity(left.Ped, right.Ped);
				FaceEntity(right.Ped, left.Ped);
				break;
			}
		}
	}

	Notifications::Show("Tenebris", Localization::IsPortuguese() ? "Duas fileiras prontas. A briga começa em 10 segundos." : "Two lines are ready. The brawl starts in 10 seconds.", NotificationType::Info, 5000);
	const auto fightStartsAt = Clock::now() + kBrawlCountdown;
	while (generation == g_BrawlGeneration && Clock::now() < fightStartsAt)
		ScriptMgr::Yield(100ms);
	if (generation != g_BrawlGeneration)
		return;

	for (auto& state : states)
	{
		if (!IsBrawlCloneCombatReady(state.Ped))
			continue;
		ENTITY::FREEZE_ENTITY_POSITION(state.Ped, false);
		PED::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(state.Ped, false);
		TASK::TASK_COMBAT_HATED_TARGETS_AROUND_PED(state.Ped, 400.0f, 0, 0);
		state.NextTask = Clock::now() + kBrawlRetaskInterval;
	}

	Notifications::Show("Tenebris", Localization::IsPortuguese() ? "BRIGA! A movimentação e o combate agora são controlados pela IA normal do jogo." : "FIGHT! Movement and combat are now controlled by the game's stock AI.", NotificationType::Success, 3500);

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
			if (now >= state.NextTask && !PED::IS_PED_IN_COMBAT(state.Ped, 0))
			{
				TASK::TASK_COMBAT_HATED_TARGETS_AROUND_PED(state.Ped, 500.0f, 0, 0);
				state.NextTask = now + kBrawlRetaskInterval;
			}
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
	for (std::size_t index = 0; index < states.size(); ++index)
	{
		auto& state = states[index];
		if (!IsBrawlCloneCombatReady(state.Ped))
			continue;
		++survivors;
		TASK::CLEAR_PED_TASKS(state.Ped, true, false);
		const Hash armed = EquipCloneWeapon(state.Ped, options.WeaponIndex);
		state.TravelWeapon = armed;
		ConfigureCloneCombat(state.Ped, CloneMode::BrawlExpedition, kCloneWeapons[options.WeaponIndex].HashName);
		state.Destination = RandomInt(0, 1) == 0 ? kArmadilloDestination : kSaintDenisDestination;
		state.WantsSleep = RandomInt(1, 100) <= kExpeditionSleepChancePercent;
		state.SleepDone = !state.WantsSleep;
		state.SleepAt = expeditionStarts + std::chrono::seconds(RandomInt(12, 45));
		state.NextTask = expeditionStarts;
		state.NextHorse = expeditionStarts + std::chrono::seconds(RandomInt(0, 3));
	}

	if (!survivors)
	{
		FinishSessionBrawlRun(generation);
		Notifications::Show("Tenebris", Localization::IsPortuguese() ? "A batalha terminou sem clones aptos a continuar." : "The brawl ended with no clones able to continue.", NotificationType::Info, 3000);
		return;
	}

	Notifications::Show("Tenebris", Localization::IsPortuguese() ? "Sobreviventes indo para Armadillo ou Saint Denis. Alguns vão dormir 15 s uma única vez no caminho." : "Survivors are heading to Armadillo or Saint Denis. Some will sleep for 15 seconds once along the way.", NotificationType::Success, 5000);

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
