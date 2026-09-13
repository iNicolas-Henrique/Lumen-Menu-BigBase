#include "core/commands/FloatCommand.hpp"
#include "core/commands/LoopedCommand.hpp"
#include "core/frontend/manager/AdvancedEditor.hpp"
#include "game/backend/Self.hpp"
#include "game/frontend/GUI.hpp"
#include "game/rdr/Enums.hpp"
#include "game/rdr/Natives.hpp"
#include "util/Math.hpp"
#include "util/teleport.hpp"

namespace YimMenu::Features
{
	static FloatCommand _FreecamSpeed{"freecamspeed", "Freecam Speed", "How fast the freecam will move positions", 0.01f, 10.0f, 0.10f};

	class Freecam : public LoopedCommand
	{
		using LoopedCommand::LoopedCommand;

		static inline int camEntity = 0;
		static inline Vector3 position{};
		static inline Vector3 rotation{};
		static inline bool detachedEditorByFreecam{};

		void CreateFreecam()
		{
			if (camEntity)
				return;

			camEntity = CAM::CREATE_CAM("DEFAULT_SCRIPTED_CAMERA", 0);
			position = CAM::GET_GAMEPLAY_CAM_COORD();
			rotation = CAM::GET_GAMEPLAY_CAM_ROT(2);

			CAM::SET_CAM_COORD(camEntity, position.x, position.y, position.z);
			CAM::SET_CAM_ROT(camEntity, rotation.x, rotation.y, rotation.z, 2);
			CAM::SET_CAM_ACTIVE(camEntity, true);
			CAM::RENDER_SCRIPT_CAMS(true, true, 500, true, true, 0);
		}

		void SyncFreecamEditor()
		{
			if (!AdvancedEditor::IsOpen())
			{
				// AdvancedEditor resets detached mode itself when it closes. Only
				// forget our ownership here; do not reopen or recreate an editor.
				detachedEditorByFreecam = false;
				return;
			}

			if (!AdvancedEditor::IsDetachedMode())
			{
				AdvancedEditor::SetDetachedMode(true);
				detachedEditorByFreecam = true;
			}

			// Detached editors are intentionally still interactive while the camera
			// runs. AdvancedEditor positions this workspace on the right side and
			// clamps it to the active viewport (including 1280x720).
			if (AdvancedEditor::IsDetachedMode())
			{
				auto& io = ImGui::GetIO();
				io.MouseDrawCursor = true;
				io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
			}
		}

		void RestoreFreecamEditor()
		{
			if (detachedEditorByFreecam && AdvancedEditor::IsOpen() && AdvancedEditor::IsDetachedMode())
				AdvancedEditor::SetDetachedMode(false);
			detachedEditorByFreecam = false;

			auto& io = ImGui::GetIO();
			const bool mouseEnabled = GUI::IsOpen() || AdvancedEditor::ShouldRenderWhenMenuClosed();
			io.MouseDrawCursor = mouseEnabled;
			if (mouseEnabled)
				io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
			else
				io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
		}

		void UpdateFreecamPosition(bool uiEditingKeyboard)
		{
			Vector3 PosChange{};
			static float accel = 0.0f;

			// Do not make WASD edit the game camera while the user is typing or
			// actively manipulating an ImGui item with keyboard input.
			if (!uiEditingKeyboard)
			{
				// Left Shift
				if (PAD::IS_DISABLED_CONTROL_PRESSED(0, (Hash)NativeInputs::INPUT_SPRINT))
					PosChange.z += _FreecamSpeed.GetState() / 2;

				// Left Control
				if (PAD::IS_DISABLED_CONTROL_PRESSED(0, (int)NativeInputs::INPUT_DUCK) || PAD::IS_DISABLED_CONTROL_PRESSED(0, (int)NativeInputs::INPUT_HORSE_STOP))
					PosChange.z -= _FreecamSpeed.GetState() / 2;

				// Forward / backward / strafe
				if (PAD::IS_DISABLED_CONTROL_PRESSED(0, (Hash)NativeInputs::INPUT_MOVE_UP_ONLY))
					PosChange.y += _FreecamSpeed.GetState();
				if (PAD::IS_DISABLED_CONTROL_PRESSED(0, (Hash)NativeInputs::INPUT_MOVE_DOWN_ONLY))
					PosChange.y -= _FreecamSpeed.GetState();
				if (PAD::IS_DISABLED_CONTROL_PRESSED(0, (Hash)NativeInputs::INPUT_MOVE_LEFT_ONLY))
					PosChange.x -= _FreecamSpeed.GetState();
				if (PAD::IS_DISABLED_CONTROL_PRESSED(0, (Hash)NativeInputs::INPUT_MOVE_RIGHT_ONLY))
					PosChange.x += _FreecamSpeed.GetState();
			}

			if (PosChange.x == 0.0f && PosChange.y == 0.0f && PosChange.z == 0.0f)
				accel = 0.0f;
			else if (accel < 10.0f)
				accel += 0.15f;

			Vector3 rot = CAM::GET_CAM_ROT(camEntity, 2);
			const float yaw = Math::DegToRad(rot.z);

			position.x += (PosChange.x * cos(yaw) - PosChange.y * sin(yaw)) * accel;
			position.y += (PosChange.x * sin(yaw) + PosChange.y * cos(yaw)) * accel;
			position.z += PosChange.z * accel;

			CAM::SET_CAM_COORD(camEntity, position.x, position.y, position.z);
			STREAMING::SET_FOCUS_POS_AND_VEL(position.x, position.y, position.z, 0.0f, 0.0f, 0.0f);
		}

		virtual void OnTick() override
		{
			CreateFreecam();
			if (!camEntity)
				return;

			SyncFreecamEditor();

			auto& io = ImGui::GetIO();
			const bool uiVisible = GUI::IsOpen() || AdvancedEditor::ShouldRenderWhenMenuClosed();
			const bool uiOwnsMouse = uiVisible && io.WantCaptureMouse;
			const bool uiEditingKeyboard = uiVisible && (io.WantTextInput || (io.WantCaptureKeyboard && ImGui::IsAnyItemActive()));

			// Game controls stay disabled while freecam owns the player. Look input
			// is passed through only when ImGui is not using the mouse. Movement is
			// read through IS_DISABLED_CONTROL_* so the editor and camera can coexist.
			PAD::DISABLE_ALL_CONTROL_ACTIONS(0);
			if (!uiOwnsMouse)
			{
				static Hash lookControls[]{
				    (Hash)NativeInputs::INPUT_LOOK_LR,
				    (Hash)NativeInputs::INPUT_LOOK_UD,
				    (Hash)NativeInputs::INPUT_LOOK_UP_ONLY,
				    (Hash)NativeInputs::INPUT_LOOK_DOWN_ONLY,
				    (Hash)NativeInputs::INPUT_LOOK_LEFT_ONLY,
				    (Hash)NativeInputs::INPUT_LOOK_RIGHT_ONLY,
				};
				for (Hash control : lookControls)
					PAD::ENABLE_CONTROL_ACTION(0, control, true);
			}

			UpdateFreecamPosition(uiEditingKeyboard);

			if (!uiOwnsMouse)
				rotation = CAM::GET_GAMEPLAY_CAM_ROT(2);
			else
				rotation = CAM::GET_CAM_ROT(camEntity, 2);
			CAM::SET_CAM_ROT(camEntity, rotation.x, rotation.y, rotation.z, 2);

			const int selfPed = Self::GetPed().GetHandle();
			TASK::CLEAR_PED_TASKS(selfPed, false, true);
			TASK::CLEAR_PED_SECONDARY_TASK(selfPed);
			TASK::CLEAR_PED_TASKS_IMMEDIATELY(selfPed, true, true);
			Self::GetPed().SetFrozen(true);
			Self::GetPed().SetVisible(false);

			// ENTER keeps its original teleport behavior, but never fires while an
			// ImGui control is actively being edited. BACK remains available as the
			// guaranteed escape from the detached right-side editor.
			if (!uiEditingKeyboard && PAD::IS_DISABLED_CONTROL_JUST_PRESSED(0, (Hash)NativeInputs::INPUT_FRONTEND_ACCEPT))
				Teleport::TeleportPlayerToCoords(Self::GetPlayer(), position);

			if (detachedEditorByFreecam && !io.WantTextInput && PAD::IS_DISABLED_CONTROL_JUST_PRESSED(0, (Hash)NativeInputs::INPUT_FRONTEND_CANCEL))
			{
				SetState(false);
				return;
			}
		}

		virtual void OnDisable() override
		{
			RestoreFreecamEditor();

			if (camEntity)
			{
				CAM::SET_CAM_ACTIVE(camEntity, false);
				CAM::RENDER_SCRIPT_CAMS(false, true, 500, true, true, 0);
				CAM::DESTROY_CAM(camEntity, false);
				STREAMING::CLEAR_FOCUS();
			}

			Self::GetPed().SetFrozen(false);
			Self::GetPed().SetVisible(true);
			camEntity = 0;
		}
	};

	static Freecam _Freecam{"freecam", "Freecam", "Detaches your camera and allows you to go anywhere!"};
}
