// Additional runtime layer for active manual clones.
// Runs on one low-frequency fiber (max eight clones) and deliberately reuses
// the existing managed-clone and PED caches instead of creating new pool scans.

struct CloneRuntimeState
{
	int Clone{};
	bool ProfileApplied{};
	bool VoiceApplied{};
	int VoiceIndex{};
	Vector3 LastIdlePosition{};
	int StillChecks{};
	int DamageTarget{};
	int LastTargetHealth{};
	Clock::time_point NextMovementCheck{};
	Clock::time_point NextSpeech{};
	Clock::time_point NextDamageCheck{};
	Clock::time_point NextHorseVisibilityCheck{};
};

struct CloneVoiceProfile
{
	const char* Dictionary{};
	const char* IdleLine{};
	const char* CombatLine{};
};

struct CloneSpeechParams
{
	const char* IndexName{};
	const char* DictionaryName{};
	alignas(8) int Variation{};
	alignas(8) Hash SpeechParamHash{};
	alignas(8) int ListenerPed{};
	alignas(8) int SyncOverNetwork{};
	alignas(8) int V7{};
	alignas(8) int V8{};
};
static_assert(sizeof(CloneSpeechParams) == 0x40);

// Verified voice dictionaries: Armadillo, Strawberry, Saint Denis and Guarma
// (Fussar henchmen). There are eight entries because the active clone cap is 8,
// allowing active hostile clones to keep distinct voices whenever possible.
constexpr std::array<CloneVoiceProfile, 8> kCloneVoices = {{
    {"ARMA_SHERIFF", "BUMP", "CHALLENGE_THREATEN"},
    {"0457_U_M_M_STRSHERRIFF_01", "CHAT_PATROL", "CHALLENGE_THREATEN_ARMED"},
    {"0998_S_M_M_AMBIENTSDPOLICE_WHITE_01", "CHAT_PATROL", "ARRIVAL_COMBAT_NEUTRAL"},
    {"1000_S_M_M_AMBIENTSDPOLICE_WHITE_03", "CHAT_PATROL", "CALL_FOR_SUPPORT"},
    {"1001_S_M_M_AMBIENTSDPOLICE_WHITE_04", "CHAT_PATROL", "CHALLENGE_THREATEN"},
    {"0760_S_M_M_FUSSARHENCHMAN_01_HISPANIC_01", "CHAT_PATROL", "CHALLENGE_THREATEN"},
    {"0762_S_M_M_FUSSARHENCHMAN_01_HISPANIC_03", "CHAT_PATROL", "ARRIVAL_COMBAT_NEUTRAL"},
    {"0763_S_M_M_FUSSARHENCHMAN_01_HISPANIC_04", "CHAT_PATROL", "CALL_FOR_SUPPORT_FAR"},
}};

std::array<CloneRuntimeState, kMaxActiveClones> g_CloneRuntime{};
bool g_CloneSupervisorRunning{};
bool g_CloneRelationshipsReady{};
Hash g_HostileCloneRelationship{};
Hash g_BodyguardCloneRelationship{};
CloneOptions g_FreecamLiveOptions{};
bool g_FreecamLiveOptionsValid{};

int ChooseUniqueCloneVoiceIndex(int clone)
{
	std::array<bool, kCloneVoices.size()> used{};
	for (const auto& state : g_CloneRuntime)
	{
		if (!state.Clone || state.Clone == clone || state.VoiceIndex < 0 || state.VoiceIndex >= static_cast<int>(kCloneVoices.size()))
			continue;
		if (!FindManagedClone(state.Clone) || !ENTITY::DOES_ENTITY_EXIST(state.Clone))
			continue;
		used[static_cast<std::size_t>(state.VoiceIndex)] = true;
	}

	const int start = std::abs(clone) % static_cast<int>(kCloneVoices.size());
	for (int offset = 0; offset < static_cast<int>(kCloneVoices.size()); ++offset)
	{
		const int index = (start + offset) % static_cast<int>(kCloneVoices.size());
		if (!used[static_cast<std::size_t>(index)])
			return index;
	}
	return start;
}

CloneRuntimeState& GetCloneRuntimeState(int clone)
{
	for (auto& state : g_CloneRuntime)
		if (state.Clone == clone)
			return state;
	for (auto& state : g_CloneRuntime)
	{
		if (!state.Clone || !FindManagedClone(state.Clone) || !ENTITY::DOES_ENTITY_EXIST(state.Clone))
		{
			state = {};
			state.Clone = clone;
			state.VoiceIndex = ChooseUniqueCloneVoiceIndex(clone);
			state.NextMovementCheck = Clock::now() + 1700ms;
			state.NextSpeech = Clock::now() + std::chrono::seconds(4 + (std::abs(clone) % 6));
			return state;
		}
	}
	g_CloneRuntime[0] = {};
	g_CloneRuntime[0].Clone = clone;
	g_CloneRuntime[0].VoiceIndex = ChooseUniqueCloneVoiceIndex(clone);
	return g_CloneRuntime[0];
}

void ResetCloneRuntimeState()
{
	for (auto& state : g_CloneRuntime)
		state = {};
	g_FreecamLiveOptionsValid = false;
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
	PED::SET_RELATIONSHIP_BETWEEN_GROUPS(5, g_BodyguardCloneRelationship, law);
	PED::SET_RELATIONSHIP_BETWEEN_GROUPS(5, law, g_BodyguardCloneRelationship);
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

void ApplyHostileCloneVoice(int clone, CloneRuntimeState& state)
{
	if (state.VoiceApplied || !clone || !ENTITY::DOES_ENTITY_EXIST(clone))
		return;
	state.VoiceIndex = std::clamp(state.VoiceIndex, 0, static_cast<int>(kCloneVoices.size()) - 1);
	AUDIO::SET_AMBIENT_VOICE_NAME(clone, kCloneVoices[state.VoiceIndex].Dictionary);
	state.VoiceApplied = true;
}

void PlayCloneSituationSpeech(int clone, CloneRuntimeState& state, bool combat)
{
	if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone) || ENTITY::IS_ENTITY_DEAD(clone) || PED::IS_PED_INCAPACITATED(clone) || PED::IS_PED_ON_MOUNT(clone))
		return;
	ApplyHostileCloneVoice(clone, state);
	const auto& voice = kCloneVoices[state.VoiceIndex];
	CloneSpeechParams params{};
	params.IndexName = combat ? voice.CombatLine : voice.IdleLine;
	params.DictionaryName = voice.Dictionary;
	params.Variation = 0;
	params.SpeechParamHash = Joaat("speech_params_force");
	params.ListenerPed = PLAYER::PLAYER_PED_ID();
	params.SyncOverNetwork = 0;
	params.V7 = 1;
	params.V8 = 1;
	AUDIO::PLAY_PED_AMBIENT_SPEECH_NATIVE(clone, reinterpret_cast<Any*>(&params));
	state.NextSpeech = Clock::now() + std::chrono::seconds(combat ? RandomInt(7, 11) : RandomInt(11, 18));
}

void MaintainHostileNaturalMovement(const ManagedClone& managed, CloneRuntimeState& state, Clock::time_point now)
{
	const int clone = managed.Ped;
	if (!clone || managed.Mode == CloneMode::Bodyguard || !ENTITY::DOES_ENTITY_EXIST(clone) || ENTITY::IS_ENTITY_DEAD(clone) || PED::IS_PED_INCAPACITATED(clone))
		return;
	const int target = static_cast<int>(PED::GET_CURRENT_TARGET_FOR_PED(clone));
	const bool inCombat = (target && IsAnyLiveHumanPedTarget(target, clone)) || PED::IS_PED_IN_COMBAT(clone, 0);
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
	if (!IsAnyLiveHumanPedTarget(target, clone))
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

void MaintainHostileSpeech(const ManagedClone& managed, CloneRuntimeState& state, Clock::time_point now)
{
	if (managed.Mode == CloneMode::Bodyguard || now < state.NextSpeech || !managed.Ped || !ENTITY::DOES_ENTITY_EXIST(managed.Ped))
		return;
	const int target = static_cast<int>(PED::GET_CURRENT_TARGET_FOR_PED(managed.Ped));
	const bool combat = (target && IsAnyLiveHumanPedTarget(target, managed.Ped)) || PED::IS_PED_IN_COMBAT(managed.Ped, 0);
	PlayCloneSituationSpeech(managed.Ped, state, combat);
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
	const auto now = Clock::now();
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
	(void)now;
}

void StartCloneEnhancementSupervisor()
{
	if (g_CloneSupervisorRunning)
		return;
	g_CloneSupervisorRunning = true;
	FiberPool::Push([] {
		while (g_CloneSupervisorRunning)
		{
			const auto now = Clock::now();
			const int owner = PLAYER::PLAYER_PED_ID();
			if (owner && ENTITY::DOES_ENTITY_EXIST(owner))
				EnsureCloneRelationships(owner);
			for (auto& state : g_CloneRuntime)
			{
				if (state.Clone && (!FindManagedClone(state.Clone) || !ENTITY::DOES_ENTITY_EXIST(state.Clone)))
					state = {};
			}
			for (const auto& managed : g_ManagedClones)
			{
				if (!managed.Ped || !ENTITY::DOES_ENTITY_EXIST(managed.Ped))
					continue;
				auto& state = GetCloneRuntimeState(managed.Ped);
				if (!state.ProfileApplied && !ENTITY::IS_ENTITY_DEAD(managed.Ped))
					ApplyRuntimeCloneProfile(managed, state, owner);
				if (managed.Mode != CloneMode::Bodyguard)
					ApplyHostileCloneVoice(managed.Ped, state);
				MaintainAssignedHorseVisibility(managed.Ped, state, now);
				MaintainHostileNaturalMovement(managed, state, now);
				MaintainCloneDamageMultiplier(managed, state, now);
				MaintainHostileSpeech(managed, state, now);
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

void RunCloneSpawnFreecamInteractive(CloneOptions initialOptions)
{
	auto self = Self::GetPed();
	if (!self.IsValid())
		return;
	const int selfHandle = self.GetHandle();
	const bool reopenMenu = GUI::IsOpen();
	g_FreecamLiveOptions = initialOptions;
	g_FreecamLiveOptionsValid = true;
	AdvancedEditor::SetDetachedMode(true);
	if (reopenMenu)
		GUI::Toggle();

	auto& io = ImGui::GetIO();
	io.MouseDrawCursor = true;
	io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;

	int camera = CAM::CREATE_CAM("DEFAULT_SCRIPTED_CAMERA", true);
	if (!camera)
	{
		AdvancedEditor::SetDetachedMode(false);
		if (reopenMenu && !GUI::IsOpen())
			GUI::Toggle();
		return;
	}

	const Vector3 start = ENTITY::GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS(selfHandle, 0.0f, 1.5f, 1.2f);
	CAM::SET_CAM_COORD(camera, start.x, start.y, start.z);
	const Vector3 initialRotation = CAM::GET_GAMEPLAY_CAM_ROT(2);
	CAM::SET_CAM_ROT(camera, initialRotation.x, initialRotation.y, initialRotation.z, 2);
	CAM::SET_CAM_ACTIVE(camera, true);
	CAM::RENDER_SCRIPT_CAMS(true, false, 0, true, true, 0);
	self.SetFrozen(true);
	self.SetVisible(false);
	STREAMING::SET_FOCUS_POS_AND_VEL(start.x, start.y, start.z, 0.0f, 0.0f, 0.0f);

	while (camera && CAM::DOES_CAM_EXIST(camera))
	{
		PAD::DISABLE_ALL_CONTROL_ACTIONS(0);
		auto& frameIo = ImGui::GetIO();
		const bool uiOwnsMouse = frameIo.WantCaptureMouse;
		const bool uiEditingKeyboard = frameIo.WantTextInput || (frameIo.WantCaptureKeyboard && ImGui::IsAnyItemActive());
		if (!uiOwnsMouse)
		{
			PAD::ENABLE_CONTROL_ACTION(0, static_cast<Hash>(NativeInputs::INPUT_LOOK_LR), true);
			PAD::ENABLE_CONTROL_ACTION(0, static_cast<Hash>(NativeInputs::INPUT_LOOK_UD), true);
		}

		Vector3 position = CAM::GET_CAM_COORD(camera);
		Vector3 rotation = CAM::GET_GAMEPLAY_CAM_ROT(2);
		if (!uiOwnsMouse)
			CAM::SET_CAM_ROT(camera, rotation.x, rotation.y, rotation.z, 2);
		else
			rotation = CAM::GET_CAM_ROT(camera, 2);

		if (!uiEditingKeyboard)
		{
			const float speed = PAD::IS_DISABLED_CONTROL_PRESSED(0, static_cast<Hash>(NativeInputs::INPUT_SPRINT)) ? 1.35f : 0.45f;
			const float yaw = rotation.z * 0.017453292519943295f;
			const float pitch = rotation.x * 0.017453292519943295f;
			const float forwardX = -std::sin(yaw) * std::cos(pitch);
			const float forwardY = std::cos(yaw) * std::cos(pitch);
			const float forwardZ = std::sin(pitch);
			const float rightX = std::cos(yaw);
			const float rightY = std::sin(yaw);
			if (PAD::IS_DISABLED_CONTROL_PRESSED(0, static_cast<Hash>(NativeInputs::INPUT_MOVE_UP_ONLY))) { position.x += forwardX * speed; position.y += forwardY * speed; position.z += forwardZ * speed; }
			if (PAD::IS_DISABLED_CONTROL_PRESSED(0, static_cast<Hash>(NativeInputs::INPUT_MOVE_DOWN_ONLY))) { position.x -= forwardX * speed; position.y -= forwardY * speed; position.z -= forwardZ * speed; }
			if (PAD::IS_DISABLED_CONTROL_PRESSED(0, static_cast<Hash>(NativeInputs::INPUT_MOVE_LEFT_ONLY))) { position.x -= rightX * speed; position.y -= rightY * speed; }
			if (PAD::IS_DISABLED_CONTROL_PRESSED(0, static_cast<Hash>(NativeInputs::INPUT_MOVE_RIGHT_ONLY))) { position.x += rightX * speed; position.y += rightY * speed; }
			if (PAD::IS_DISABLED_CONTROL_PRESSED(0, static_cast<Hash>(NativeInputs::INPUT_SPRINT))) position.z += 0.02f;
			if (PAD::IS_DISABLED_CONTROL_PRESSED(0, static_cast<Hash>(NativeInputs::INPUT_DUCK))) position.z -= speed * 0.65f;
		}

		CAM::SET_CAM_COORD(camera, position.x, position.y, position.z);
		STREAMING::SET_FOCUS_POS_AND_VEL(position.x, position.y, position.z, 0.0f, 0.0f, 0.0f);

		Vector3 marker{};
		if (!GetFreecamAimPoint(position, rotation, selfHandle, marker))
		{
			constexpr float kDegToRad = 0.017453292519943295f;
			const float pitch = rotation.x * kDegToRad;
			const float yaw = rotation.z * kDegToRad;
			const float cp = std::cos(pitch);
			marker = {position.x - std::sin(yaw) * cp * 20.0f,
			          position.y + std::cos(yaw) * cp * 20.0f,
			          position.z + std::sin(pitch) * 20.0f};
		}

		// Lightweight screen-centre reticle replaces the GTA-style DRAW_MARKER
		// native, which is not exposed by this RDR2 NativeDB snapshot.
		if (const ImGuiViewport* viewport = ImGui::GetMainViewport())
		{
			const ImVec2 center(viewport->Pos.x + viewport->Size.x * 0.5f, viewport->Pos.y + viewport->Size.y * 0.5f);
			ImGui::GetForegroundDrawList()->AddCircle(center, 4.0f, IM_COL32(225, 235, 205, 210), 12, 1.4f);
		}

		if (!uiEditingKeyboard && PAD::IS_DISABLED_CONTROL_JUST_PRESSED(0, static_cast<Hash>(NativeInputs::INPUT_ENTER)))
		{
			CloneOptions spawnOptions = g_FreecamLiveOptionsValid ? g_FreecamLiveOptions : initialOptions;
			SpawnManualCloneAt(spawnOptions, marker, rotation.z);
			ScriptMgr::Yield(180ms);
		}
		if (!uiEditingKeyboard && PAD::IS_DISABLED_CONTROL_JUST_PRESSED(0, static_cast<Hash>(NativeInputs::INPUT_FRONTEND_CANCEL)))
			break;
		ScriptMgr::Yield();
	}

	if (camera && CAM::DOES_CAM_EXIST(camera))
	{
		CAM::SET_CAM_ACTIVE(camera, false);
		CAM::DESTROY_CAM(camera, false);
	}
	CAM::RENDER_SCRIPT_CAMS(false, false, 0, true, true, 0);
	STREAMING::CLEAR_FOCUS();
	if (ENTITY::DOES_ENTITY_EXIST(selfHandle))
	{
		self.SetVisible(true);
		self.SetFrozen(false);
	}
	AdvancedEditor::SetDetachedMode(false);
	g_FreecamLiveOptionsValid = false;
	if (reopenMenu && !GUI::IsOpen())
		GUI::Toggle();
	io.MouseDrawCursor = GUI::IsOpen();
	if (GUI::IsOpen()) io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
	else io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
}
