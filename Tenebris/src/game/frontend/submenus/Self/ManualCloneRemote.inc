ManagedClone* GetLatestRemoteControllableClone()
{
	for (auto it = g_ManagedClones.rbegin(); it != g_ManagedClones.rend(); ++it)
	{
		if (it->Mode == CloneMode::BrawlExpedition)
			continue;
		if (!it->Ped || !ENTITY::DOES_ENTITY_EXIST(it->Ped) || ENTITY::IS_ENTITY_DEAD(it->Ped) || PED::IS_PED_INCAPACITATED(it->Ped))
			continue;
		return &*it;
	}
	return nullptr;
}

bool AcquireRemoteCloneControl(int clone, bool networked)
{
	if (!clone || !ENTITY::DOES_ENTITY_EXIST(clone))
		return false;
	if (!networked || NETWORK::NETWORK_HAS_CONTROL_OF_ENTITY(clone))
		return true;

	for (int attempt = 0; attempt < 6 && !NETWORK::NETWORK_HAS_CONTROL_OF_ENTITY(clone); ++attempt)
	{
		NETWORK::NETWORK_REQUEST_CONTROL_OF_ENTITY(clone);
		ScriptMgr::Yield(60ms);
	}
	return NETWORK::NETWORK_HAS_CONTROL_OF_ENTITY(clone);
}

void TaskRemoteCloneMove(int clone, const Vector3& target, float speed)
{
	CloneNativeCompat::InvokeByHash(
	    static_cast<rage::scrNativeHash>(0xD76B57B44F1E6F8BULL),
	    clone,
	    target.x,
	    target.y,
	    target.z,
	    speed,
	    450,
	    0.35f,
	    0.0f,
	    0);
}

void TaskRemoteCloneJump(int clone)
{
	CloneNativeCompat::InvokeByHash(
	    static_cast<rage::scrNativeHash>(0x0AE4086104E067B1ULL),
	    clone,
	    false);
}

void TaskRemoteCloneShoot(int clone, const Vector3& target)
{
	CloneNativeCompat::InvokeByHash(
	    static_cast<rage::scrNativeHash>(0x46A6CC01E0826106ULL),
	    clone,
	    target.x,
	    target.y,
	    target.z,
	    240,
	    Joaat("FIRING_PATTERN_SINGLE_SHOT"),
	    0);
}

void RunCloneRemoteControl(int clone)
{
	if (g_RemoteControlledClone)
	{
		Notifications::Show("Tenebris", Localization::IsPortuguese() ? "Já existe um clone em controle remoto." : "A clone is already under remote control.", NotificationType::Warning, 2200);
		return;
	}

	auto* managed = FindManagedClone(clone);
	if (!managed || managed->Mode == CloneMode::BrawlExpedition || !clone || !ENTITY::DOES_ENTITY_EXIST(clone) || ENTITY::IS_ENTITY_DEAD(clone) || PED::IS_PED_INCAPACITATED(clone))
	{
		Notifications::Show("Tenebris", Localization::IsPortuguese() ? "Nenhum clone manual válido para controlar." : "No valid manual clone is available to control.", NotificationType::Warning, 2400);
		return;
	}

	const bool networked = managed->Networked;
	const CloneMode originalMode = managed->Mode;
	const Hash desiredWeapon = managed->Weapon;
	if (!AcquireRemoteCloneControl(clone, networked))
	{
		Notifications::Show("Tenebris", Localization::IsPortuguese() ? "Não consegui obter controle da entidade de rede do clone." : "Could not obtain ownership of the network clone.", NotificationType::Warning, 2800);
		return;
	}

	auto self = Self::GetPed();
	if (!self.IsValid() || self.GetHealth() <= 0)
		return;
	const bool reopenMenu = GUI::IsOpen();
	if (reopenMenu)
		GUI::Toggle();

	int camera = CAM::CREATE_CAM("DEFAULT_SCRIPTED_CAMERA", 0);
	if (!camera)
	{
		if (reopenMenu && !GUI::IsOpen())
			GUI::Toggle();
		return;
	}

	g_RemoteControlledClone = clone;
	StopCloneGunTwirl(clone, desiredWeapon, true);
	TASK::CLEAR_PED_TASKS(clone, true, false);
	PED::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(clone, true);
	if (desiredWeapon && desiredWeapon != Joaat("WEAPON_UNARMED"))
		EnsureCloneWeaponEquipped(clone, desiredWeapon);

	Vector3 rotation = CAM::GET_GAMEPLAY_CAM_ROT(2);
	const Vector3 initial = ENTITY::GET_ENTITY_COORDS(clone, true, false);
	CAM::SET_CAM_COORD(camera, initial.x, initial.y - 4.0f, initial.z + 2.0f);
	CAM::SET_CAM_ROT(camera, rotation.x, rotation.y, rotation.z, 2);
	CAM::SET_CAM_ACTIVE(camera, true);
	CAM::RENDER_SCRIPT_CAMS(true, true, 250, true, true, 0);
	self.SetFrozen(true);
	self.SetVisible(false);

	Notifications::Show(
	    "Tenebris",
	    Localization::IsPortuguese()
	        ? "CONTROLE DO CLONE: WASD move, SHIFT corre, ESPAÇO pula, botão direito mira, esquerdo atira; BACK sai."
	        : "CLONE CONTROL: WASD moves, SHIFT sprints, SPACE jumps, right mouse aims, left mouse fires; BACK exits.",
	    NotificationType::Info,
	    6500);

	bool movementIssued{};
	auto nextMove = Clock::now();
	auto nextAim = Clock::now();
	auto nextShot = Clock::now();
	auto nextOwnershipCheck = Clock::now();

	while (camera && CAM::DOES_CAM_EXIST(camera) && clone && ENTITY::DOES_ENTITY_EXIST(clone) && !ENTITY::IS_ENTITY_DEAD(clone) && !PED::IS_PED_INCAPACITATED(clone))
	{
		PAD::DISABLE_ALL_CONTROL_ACTIONS(0);
		for (Hash control : {(Hash)NativeInputs::INPUT_LOOK_LR, (Hash)NativeInputs::INPUT_LOOK_UD, (Hash)NativeInputs::INPUT_LOOK_UP_ONLY, (Hash)NativeInputs::INPUT_LOOK_DOWN_ONLY, (Hash)NativeInputs::INPUT_LOOK_LEFT_ONLY, (Hash)NativeInputs::INPUT_LOOK_RIGHT_ONLY})
			PAD::ENABLE_CONTROL_ACTION(0, control, true);

		if (PAD::IS_DISABLED_CONTROL_JUST_PRESSED(0, (Hash)NativeInputs::INPUT_FRONTEND_CANCEL))
			break;

		const auto now = Clock::now();
		if (networked && now >= nextOwnershipCheck)
		{
			if (!NETWORK::NETWORK_HAS_CONTROL_OF_ENTITY(clone))
				NETWORK::NETWORK_REQUEST_CONTROL_OF_ENTITY(clone);
			nextOwnershipCheck = now + 1s;
		}

		rotation = CAM::GET_GAMEPLAY_CAM_ROT(2);
		const float pitch = std::clamp(rotation.x, -60.0f, 50.0f) * 0.017453292519943295f;
		const float yaw = rotation.z * 0.017453292519943295f;
		const float cp = std::cos(pitch);
		const Vector3 lookDirection{-std::sin(yaw) * cp, std::cos(yaw) * cp, std::sin(pitch)};
		const Vector3 clonePos = ENTITY::GET_ENTITY_COORDS(clone, true, false);
		const Vector3 lookTarget{clonePos.x, clonePos.y, clonePos.z + 1.30f};
		const Vector3 cameraPos{
		    lookTarget.x - lookDirection.x * 4.35f,
		    lookTarget.y - lookDirection.y * 4.35f,
		    lookTarget.z - lookDirection.z * 4.35f};

		CAM::SET_CAM_COORD(camera, cameraPos.x, cameraPos.y, cameraPos.z);
		CAM::SET_CAM_ROT(camera, rotation.x, rotation.y, rotation.z, 2);
		STREAMING::SET_FOCUS_POS_AND_VEL(clonePos.x, clonePos.y, clonePos.z, 0.0f, 0.0f, 0.0f);

		Vector3 aimPoint{};
		const bool hitSomething = GetFreecamAimPoint(cameraPos, rotation, clone, aimPoint);
		if (!hitSomething)
		{
			aimPoint = {
			    cameraPos.x + lookDirection.x * 350.0f,
			    cameraPos.y + lookDirection.y * 350.0f,
			    cameraPos.z + lookDirection.z * 350.0f};
		}

		const bool aiming = PAD::IS_DISABLED_CONTROL_PRESSED(0, (Hash)NativeInputs::INPUT_AIM);
		const bool attacking = PAD::IS_DISABLED_CONTROL_PRESSED(0, (Hash)NativeInputs::INPUT_ATTACK);
		const bool canShoot = desiredWeapon && desiredWeapon != Joaat("WEAPON_UNARMED") && WEAPON::IS_WEAPON_A_GUN(desiredWeapon);

		if (aiming && canShoot)
		{
			if (movementIssued)
			{
				TASK::CLEAR_PED_TASKS(clone, true, false);
				movementIssued = false;
			}
			EnsureCloneWeaponEquipped(clone, desiredWeapon);
			const float dx = aimPoint.x - clonePos.x;
			const float dy = aimPoint.y - clonePos.y;
			if (std::abs(dx) + std::abs(dy) > 0.001f)
			{
				float heading = std::atan2(-dx, dy) * 57.29577951308232f;
				if (heading < 0.0f)
					heading += 360.0f;
				ENTITY::SET_ENTITY_HEADING(clone, heading);
			}
			if (now >= nextAim)
			{
				TASK::TASK_AIM_GUN_AT_COORD(clone, aimPoint.x, aimPoint.y, aimPoint.z, 320, true, true);
				nextAim = now + 180ms;
			}
			if (attacking && now >= nextShot)
			{
				TaskRemoteCloneShoot(clone, aimPoint);
				nextShot = now + 210ms;
			}
			if (hitSomething)
				GRAPHICS::_DRAW_MARKER(0x6903B113, aimPoint.x, aimPoint.y, aimPoint.z, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.10f, 0.10f, 0.10f, 255, 255, 255, 220, false, true, 2, false, nullptr, nullptr, false);
		}
		else
		{
			float inputX{};
			float inputY{};
			if (PAD::IS_DISABLED_CONTROL_PRESSED(0, (Hash)NativeInputs::INPUT_MOVE_UP_ONLY)) inputY += 1.0f;
			if (PAD::IS_DISABLED_CONTROL_PRESSED(0, (Hash)NativeInputs::INPUT_MOVE_DOWN_ONLY)) inputY -= 1.0f;
			if (PAD::IS_DISABLED_CONTROL_PRESSED(0, (Hash)NativeInputs::INPUT_MOVE_RIGHT_ONLY)) inputX += 1.0f;
			if (PAD::IS_DISABLED_CONTROL_PRESSED(0, (Hash)NativeInputs::INPUT_MOVE_LEFT_ONLY)) inputX -= 1.0f;

			const float inputLengthSq = inputX * inputX + inputY * inputY;
			if (inputLengthSq > 0.01f)
			{
				const float invLength = 1.0f / std::sqrt(inputLengthSq);
				inputX *= invLength;
				inputY *= invLength;
				const float forwardX = -std::sin(yaw);
				const float forwardY = std::cos(yaw);
				const float rightX = std::cos(yaw);
				const float rightY = std::sin(yaw);
				const float worldX = forwardX * inputY + rightX * inputX;
				const float worldY = forwardY * inputY + rightY * inputX;

				if (now >= nextMove)
				{
					const Vector3 target{clonePos.x + worldX * 2.6f, clonePos.y + worldY * 2.6f, clonePos.z};
					float heading = std::atan2(-worldX, worldY) * 57.29577951308232f;
					if (heading < 0.0f)
						heading += 360.0f;
					ENTITY::SET_ENTITY_HEADING(clone, heading);
					const float speed = PAD::IS_DISABLED_CONTROL_PRESSED(0, (Hash)NativeInputs::INPUT_SPRINT) ? 3.0f : 1.55f;
					TaskRemoteCloneMove(clone, target, speed);
					movementIssued = true;
					nextMove = now + 180ms;
				}
			}
			else if (movementIssued)
			{
				TASK::CLEAR_PED_TASKS(clone, true, false);
				movementIssued = false;
			}

			if (PAD::IS_DISABLED_CONTROL_JUST_PRESSED(0, (Hash)NativeInputs::INPUT_JUMP))
			{
				TASK::CLEAR_PED_TASKS(clone, true, false);
				TaskRemoteCloneJump(clone);
				movementIssued = false;
				nextMove = now + 450ms;
			}
		}

		ScriptMgr::Yield();
	}

	if (clone && ENTITY::DOES_ENTITY_EXIST(clone) && !ENTITY::IS_ENTITY_DEAD(clone))
	{
		TASK::CLEAR_PED_TASKS(clone, true, false);
		if (desiredWeapon && desiredWeapon != Joaat("WEAPON_UNARMED") && !PED::IS_PED_INCAPACITATED(clone))
			EnsureCloneWeaponEquipped(clone, desiredWeapon);
		PED::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(clone, originalMode == CloneMode::Bodyguard);
	}

	g_RemoteControlledClone = 0;
	CAM::SET_CAM_ACTIVE(camera, false);
	CAM::RENDER_SCRIPT_CAMS(false, true, 250, true, true, 0);
	CAM::DESTROY_CAM(camera, false);
	STREAMING::CLEAR_FOCUS();
	if (self.IsValid())
	{
		self.SetFrozen(false);
		self.SetVisible(true);
	}
	if (reopenMenu && !GUI::IsOpen())
		GUI::Toggle();

	Notifications::Show("Tenebris", Localization::IsPortuguese() ? "Controle remoto encerrado; a IA do clone voltou." : "Remote control ended; clone AI resumed.", NotificationType::Info, 2200);
}
