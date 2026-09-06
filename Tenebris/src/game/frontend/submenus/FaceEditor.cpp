#include "FaceEditor.hpp"

#include "core/frontend/manager/Category.hpp"
#include "core/frontend/manager/Submenu.hpp"
#include "core/frontend/manager/UIItem.hpp"
#include "game/backend/FiberPool.hpp"
#include "game/pointers/Pointers.hpp"
#include "game/rdr/Natives.hpp"
#include "game/rdr/invoker/Crossmap.hpp"
#include "game/rdr/invoker/Invoker.hpp"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace YimMenu::Submenus
{
	namespace
	{
		// Somente natives do jogo base presentes no NativeDB/Crossmap do Tenebris.
		constexpr rage::scrNativeHash kSetFaceFeature = 0x5653AB26C82938CF;
		constexpr rage::scrNativeHash kGetFaceFeature = 0xFD1BA1EEF7985BB8;
		constexpr rage::scrNativeHash kFixPedOutfit = 0x704C908E9C405136;
		constexpr rage::scrNativeHash kUpdatePedVariation = 0xCC8CA3E88256E58F;
		constexpr rage::scrNativeHash kGetNumOutfitPresets = 0x10C70A515BC03707;
		constexpr rage::scrNativeHash kSetOutfitIndex = 0x77FF8D35EEC6BBC4;
		constexpr rage::scrNativeHash kGetNumComponents = 0x90403E8107B60E81;
		constexpr rage::scrNativeHash kGetComponentCategory = 0x9B90842304C938A7;
		constexpr rage::scrNativeHash kGetMetaPedAssetGuids = 0xA9C28516A6DC9D56;
		constexpr rage::scrNativeHash kGetMetaPedAssetTint = 0xE7998FEC53A33BBE;
		constexpr rage::scrNativeHash kSetMetaPedTag = 0xBC6DF00D7A4A6819;
		constexpr rage::scrNativeHash kRemoveMetaPedTag = 0xD710A5007C2AC539;
		constexpr rage::scrNativeHash kSetAmbientVoiceName = 0x6C8065A3B780185B;

		constexpr std::uint32_t kMpMaleModel = 0xF5C1611E;
		constexpr std::uint32_t kMpFemaleModel = 0xA7AF20C0;
		constexpr std::uint32_t kEyebrowCategory = 0x0EAF8BBE; // joaat("eyebrows")
		constexpr int kHeadBoneId = 21030;                    // SKEL_Head

		consteval bool NativeExistsInTenebris(rage::scrNativeHash hash)
		{
			for (const auto native : g_Crossmap)
				if (native == hash)
					return true;
			return false;
		}

		static_assert(NativeExistsInTenebris(kSetFaceFeature));
		static_assert(NativeExistsInTenebris(kGetFaceFeature));
		static_assert(NativeExistsInTenebris(kFixPedOutfit));
		static_assert(NativeExistsInTenebris(kUpdatePedVariation));
		static_assert(NativeExistsInTenebris(kGetNumOutfitPresets));
		static_assert(NativeExistsInTenebris(kSetOutfitIndex));
		static_assert(NativeExistsInTenebris(kGetNumComponents));
		static_assert(NativeExistsInTenebris(kGetComponentCategory));
		static_assert(NativeExistsInTenebris(kGetMetaPedAssetGuids));
		static_assert(NativeExistsInTenebris(kGetMetaPedAssetTint));
		static_assert(NativeExistsInTenebris(kSetMetaPedTag));
		static_assert(NativeExistsInTenebris(kRemoveMetaPedTag));
		static_assert(NativeExistsInTenebris(kSetAmbientVoiceName));

		template<typename... Args>
		bool InvokeRawVoid(rage::scrNativeHash hash, Args&&... args)
		{
			if (!Pointers.GetNativeHandler)
				return false;
			auto handler = Pointers.GetNativeHandler(hash);
			if (!handler)
				return false;
			NativeInvoker invoker{};
			invoker.BeginCall();
			(invoker.PushArg(std::forward<Args>(args)), ...);
			handler(&invoker.m_CallContext);
			return true;
		}

		template<typename Ret, typename... Args>
		std::optional<Ret> InvokeRaw(rage::scrNativeHash hash, Args&&... args)
		{
			if (!Pointers.GetNativeHandler)
				return std::nullopt;
			auto handler = Pointers.GetNativeHandler(hash);
			if (!handler)
				return std::nullopt;
			NativeInvoker invoker{};
			invoker.BeginCall();
			(invoker.PushArg(std::forward<Args>(args)), ...);
			handler(&invoker.m_CallContext);
			return invoker.GetReturnValue<Ret>();
		}

		struct FaceFeature
		{
			const char* Label;
			std::int32_t Index;
			const char* Group;
		};

		constexpr std::array<FaceFeature, 39> kFaceFeatures{{
		    {"Largura da cabeca", 0x84D6, "Cabeca e sobrancelhas"},
		    {"Altura da sobrancelha", 0x3303, "Cabeca e sobrancelhas"},
		    {"Largura da sobrancelha", 0x2FF9, "Cabeca e sobrancelhas"},
		    {"Profundidade da sobrancelha", 0x4AD1, "Cabeca e sobrancelhas"},
		    {"Largura da orelha", 0xC04F, "Orelhas"},
		    {"Angulo da orelha", 0xB6CE, "Orelhas"},
		    {"Altura da orelha", 0x2844, "Orelhas"},
		    {"Tamanho do lobulo", 0xED30, "Orelhas"},
		    {"Altura da maca do rosto", 0x6A0B, "Macas, maxilar e queixo"},
		    {"Largura da maca do rosto", 0xABCF, "Macas, maxilar e queixo"},
		    {"Profundidade da maca do rosto", 0x358D, "Macas, maxilar e queixo"},
		    {"Altura do maxilar", 0x8D0A, "Macas, maxilar e queixo"},
		    {"Largura do maxilar", 0xEBAE, "Macas, maxilar e queixo"},
		    {"Profundidade do maxilar", 0x1DF6, "Macas, maxilar e queixo"},
		    {"Altura do queixo", 0x3C0F, "Macas, maxilar e queixo"},
		    {"Largura do queixo", 0xC3B2, "Macas, maxilar e queixo"},
		    {"Profundidade do queixo", 0xE323, "Macas, maxilar e queixo"},
		    {"Altura da palpebra", 0x8B2B, "Olhos"},
		    {"Largura da palpebra", 0x1B6B, "Olhos"},
		    {"Profundidade dos olhos", 0xEE44, "Olhos"},
		    {"Angulo dos olhos", 0xD266, "Olhos"},
		    {"Distancia entre os olhos", 0xA54E, "Olhos"},
		    {"Altura dos olhos", 0xDDFB, "Olhos"},
		    {"Largura do nariz", 0x6E7F, "Nariz"},
		    {"Tamanho do nariz", 0x3471, "Nariz"},
		    {"Altura do nariz", 0x03F5, "Nariz"},
		    {"Angulo do nariz", 0x34B1, "Nariz"},
		    {"Curvatura do nariz", 0xF156, "Nariz"},
		    {"Distancia das narinas", 0x561E, "Nariz"},
		    {"Largura da boca", 0xF065, "Boca e labios"},
		    {"Profundidade da boca", 0xAA69, "Boca e labios"},
		    {"Posicao horizontal da boca", 0x7AC3, "Boca e labios"},
		    {"Posicao vertical da boca", 0x410D, "Boca e labios"},
		    {"Altura do labio superior", 0x1A00, "Boca e labios"},
		    {"Largura do labio superior", 0x91C1, "Boca e labios"},
		    {"Profundidade do labio superior", 0xC375, "Boca e labios"},
		    {"Altura do labio inferior", 0xBB4D, "Boca e labios"},
		    {"Largura do labio inferior", 0xB0B0, "Boca e labios"},
		    {"Profundidade do labio inferior", 0x5D16, "Boca e labios"},
		}};

		enum class CharacterSex : int
		{
			Unsupported = 0,
			Male,
			Female
		};

		enum class CameraFraming
		{
			Face,
			FullBody
		};

		struct MetaPedComponentSnapshot
		{
			std::uint32_t Category{};
			std::uint32_t Drawable{};
			std::uint32_t Albedo{};
			std::uint32_t Normal{};
			std::uint32_t Material{};
			std::uint32_t Palette{};
			int Tint0{};
			int Tint1{};
			int Tint2{};
		};

		struct CharacterAppearanceState
		{
			std::mutex Mutex;
			std::array<float, kFaceFeatures.size()> FaceValues{};
			std::array<float, kFaceFeatures.size()> OriginalFaceValues{};
			int OutfitIndex{};
			bool EyebrowRemoved{};
			int VoiceIndex{};
			std::optional<MetaPedComponentSnapshot> SavedEyebrow;
		};

		CharacterAppearanceState g_Appearance;
		std::atomic_bool g_LastSaveSucceeded{false};
		std::atomic_bool g_FaceLoaded{false};
		std::atomic_bool g_FaceNativeAvailable{true};
		std::atomic_bool g_FaceScrollRequested{false};
		std::atomic_bool g_WardrobeLoaded{false};
		std::atomic_bool g_WardrobeNativeAvailable{true};
		std::atomic_bool g_WardrobeScrollRequested{false};
		std::atomic<int> g_OutfitCount{0};
		std::atomic<CharacterSex> g_CharacterSex{CharacterSex::Unsupported};
		std::size_t g_SelectedFeature{};

		Cam g_EditorCamera{};
		std::atomic_bool g_FaceCameraOpen{false};
		std::atomic_bool g_WardrobeCameraOpen{false};
		std::atomic_uint64_t g_CameraGeneration{0};

		CharacterSex GetCharacterSex(Ped ped)
		{
			if (!ped || !ENTITY::DOES_ENTITY_EXIST(ped))
				return CharacterSex::Unsupported;
			const auto model = static_cast<std::uint32_t>(ENTITY::GET_ENTITY_MODEL(ped));
			if (model == kMpMaleModel)
				return CharacterSex::Male;
			if (model == kMpFemaleModel)
				return CharacterSex::Female;
			return CharacterSex::Unsupported;
		}

		const char* GetSexLabel(CharacterSex sex)
		{
			switch (sex)
			{
			case CharacterSex::Male: return "Masculino (mp_male)";
			case CharacterSex::Female: return "Feminino (mp_female)";
			default: return "Modelo nao suportado";
			}
		}

		std::filesystem::path GetAppearanceSavePath()
		{
			const char* appData = std::getenv("APPDATA");
			const auto base = appData ? std::filesystem::path(appData) : std::filesystem::current_path();
			return base / "Tenebris" / "character_appearance.cfg";
		}

		void SaveAppearanceState()
		{
			try
			{
				const auto path = GetAppearanceSavePath();
				std::filesystem::create_directories(path.parent_path());
				std::ofstream out(path, std::ios::trunc);
				if (!out)
				{
					g_LastSaveSucceeded.store(false);
					return;
				}

				std::scoped_lock lock(g_Appearance.Mutex);
				out << "version=2\n";
				out << "outfit=" << g_Appearance.OutfitIndex << '\n';
				out << "eyebrow_removed=" << (g_Appearance.EyebrowRemoved ? 1 : 0) << '\n';
				out << "voice=" << g_Appearance.VoiceIndex << '\n';
				for (std::size_t i = 0; i < g_Appearance.FaceValues.size(); ++i)
					out << "face_" << i << '=' << g_Appearance.FaceValues[i] << '\n';
				g_LastSaveSucceeded.store(static_cast<bool>(out));
			}
			catch (...)
			{
				g_LastSaveSucceeded.store(false);
			}
		}

		void StopEditorCameraOnFiber(bool releasePed)
		{
			if (g_EditorCamera && CAM::DOES_CAM_EXIST(g_EditorCamera))
			{
				CAM::RENDER_SCRIPT_CAMS(false, true, 250, true, true, 0);
				CAM::SET_CAM_ACTIVE(g_EditorCamera, false);
				CAM::DESTROY_CAM(g_EditorCamera, true);
			}
			g_EditorCamera = 0;

			if (releasePed)
			{
				const Ped ped = PLAYER::PLAYER_PED_ID();
				if (ped && ENTITY::DOES_ENTITY_EXIST(ped))
					ENTITY::FREEZE_ENTITY_POSITION(ped, false);
			}
		}

		void StartEditorCameraOnFiber(CameraFraming framing)
		{
			const bool shouldOpen = framing == CameraFraming::Face ? g_FaceCameraOpen.load() : g_WardrobeCameraOpen.load();
			if (!shouldOpen)
				return;

			const Ped ped = PLAYER::PLAYER_PED_ID();
			if (!ped || !ENTITY::DOES_ENTITY_EXIST(ped))
				return;

			StopEditorCameraOnFiber(false);
			TASK::CLEAR_PED_TASKS_IMMEDIATELY(ped, true, true);
			ENTITY::FREEZE_ENTITY_POSITION(ped, true);

			const auto head = PED::GET_PED_BONE_COORDS(ped, kHeadBoneId, 0.0f, 0.0f, 0.0f);
			const auto forward = ENTITY::GET_ENTITY_FORWARD_VECTOR(ped);
			const float rightX = forward.y;
			const float rightY = -forward.x;

			const float distance = framing == CameraFraming::Face ? 0.92f : 3.05f;
			const float cameraZOffset = framing == CameraFraming::Face ? 0.015f : -0.78f;
			const float targetZOffset = framing == CameraFraming::Face ? -0.015f : -0.78f;
			const float targetSideOffset = framing == CameraFraming::Face ? 0.16f : 0.25f;
			const float fov = framing == CameraFraming::Face ? 25.0f : 36.0f;

			const float cameraX = head.x + forward.x * distance;
			const float cameraY = head.y + forward.y * distance;
			const float cameraZ = head.z + cameraZOffset;
			const float targetX = head.x + rightX * targetSideOffset;
			const float targetY = head.y + rightY * targetSideOffset;
			const float targetZ = head.z + targetZOffset;

			g_EditorCamera = CAM::CREATE_CAM("DEFAULT_SCRIPTED_CAMERA", true);
			if (!g_EditorCamera || !CAM::DOES_CAM_EXIST(g_EditorCamera))
			{
				g_EditorCamera = 0;
				ENTITY::FREEZE_ENTITY_POSITION(ped, false);
				return;
			}

			CAM::SET_CAM_COORD(g_EditorCamera, cameraX, cameraY, cameraZ);
			CAM::POINT_CAM_AT_COORD(g_EditorCamera, targetX, targetY, targetZ);
			CAM::SET_CAM_FOV(g_EditorCamera, fov);
			CAM::SET_CAM_NEAR_CLIP(g_EditorCamera, 0.03f);
			CAM::SET_CAM_ACTIVE(g_EditorCamera, true);
			CAM::RENDER_SCRIPT_CAMS(true, true, 250, true, true, 0);
		}

		void RequestEditorCamera(CameraFraming framing)
		{
			const auto generation = ++g_CameraGeneration;
			FiberPool::Push([generation, framing] {
				if (generation != g_CameraGeneration.load())
					return;
				StartEditorCameraOnFiber(framing);
			});
		}

		void RequestStopEditorCamera()
		{
			const auto generation = ++g_CameraGeneration;
			FiberPool::Push([generation] {
				if (generation != g_CameraGeneration.load())
					return;
				StopEditorCameraOnFiber(true);
			});
		}

		bool UpdatePedVariation(Ped ped)
		{
			return InvokeRawVoid(kUpdatePedVariation, ped, false, true, true, true, false);
		}

		bool ApplyFaceFeatureOnFiber(Ped ped, std::int32_t featureIndex, float value, bool update)
		{
			if (!ped || !ENTITY::DOES_ENTITY_EXIST(ped))
				return false;
			if (!InvokeRawVoid(kSetFaceFeature, ped, featureIndex, std::clamp(value, -1.0f, 1.0f)))
				return false;
			return !update || UpdatePedVariation(ped);
		}

		std::optional<std::array<float, kFaceFeatures.size()>> ReadFaceSnapshotOnFiber(Ped ped)
		{
			if (!ped || !ENTITY::DOES_ENTITY_EXIST(ped))
				return std::nullopt;
			std::array<float, kFaceFeatures.size()> values{};
			for (std::size_t i = 0; i < kFaceFeatures.size(); ++i)
			{
				const auto current = InvokeRaw<float>(kGetFaceFeature, ped, kFaceFeatures[i].Index);
				if (!current)
					return std::nullopt;
				values[i] = std::clamp(*current, -1.0f, 1.0f);
			}
			return values;
		}

		bool RestoreFaceSnapshotOnFiber(Ped ped, const std::array<float, kFaceFeatures.size()>& values, bool update)
		{
			bool ok = true;
			for (std::size_t i = 0; i < kFaceFeatures.size(); ++i)
				ok = ApplyFaceFeatureOnFiber(ped, kFaceFeatures[i].Index, values[i], false) && ok;
			return ok && (!update || UpdatePedVariation(ped));
		}

		bool IsProtectedAppearanceCategory(std::uint32_t category)
		{
			// Cabeca/pele, olhos, dentes, cabelo, barba e sobrancelha nunca devem ser
			// substituidos por um preset de roupa.
			constexpr std::array<std::uint32_t, 7> categories{
			    0x378AD10C, // heads
			    0x40EBE4EB, // bodies
			    0x864B03AE, // hair
			    0x15D3C7F2, // beards_chin
			    0x0EAF8BBE, // eyebrows
			    0xEA24B45E, // eyes
			    0x96EDAE5C  // teeth
			};
			return std::find(categories.begin(), categories.end(), category) != categories.end();
		}

		std::vector<MetaPedComponentSnapshot> CaptureProtectedComponentsOnFiber(Ped ped)
		{
			std::vector<MetaPedComponentSnapshot> result;
			const auto countValue = InvokeRaw<int>(kGetNumComponents, ped);
			if (!countValue)
				return result;

			const int count = std::clamp(*countValue, 0, 256);
			for (int i = 0; i < count; ++i)
			{
				const auto category = InvokeRaw<std::uint32_t>(kGetComponentCategory, ped, i, 0);
				if (!category || !IsProtectedAppearanceCategory(*category))
					continue;

				MetaPedComponentSnapshot snapshot{};
				snapshot.Category = *category;
				const auto gotAssets = InvokeRaw<bool>(kGetMetaPedAssetGuids,
				    ped, i, &snapshot.Drawable, &snapshot.Albedo, &snapshot.Normal, &snapshot.Material);
				if (!gotAssets || !*gotAssets)
					continue;

				const auto gotTint = InvokeRaw<bool>(kGetMetaPedAssetTint,
				    ped, i, &snapshot.Palette, &snapshot.Tint0, &snapshot.Tint1, &snapshot.Tint2);
				if (!gotTint || !*gotTint)
				{
					snapshot.Palette = 0;
					snapshot.Tint0 = snapshot.Tint1 = snapshot.Tint2 = 0;
				}
				result.push_back(snapshot);
			}
			return result;
		}

		bool RestoreProtectedComponentsOnFiber(Ped ped, const std::vector<MetaPedComponentSnapshot>& components)
		{
			bool ok = true;
			for (const auto& component : components)
			{
				ok = InvokeRawVoid(kSetMetaPedTag,
				         ped,
				         component.Drawable,
				         component.Albedo,
				         component.Normal,
				         component.Material,
				         component.Palette,
				         component.Tint0,
				         component.Tint1,
				         component.Tint2)
				    && ok;
			}
			return ok;
		}

		void QueueReadCurrentFace()
		{
			g_FaceLoaded.store(false);
			g_FaceNativeAvailable.store(true);
			FiberPool::Push([] {
				const Ped ped = PLAYER::PLAYER_PED_ID();
				const auto values = ReadFaceSnapshotOnFiber(ped);
				if (!values)
				{
					g_FaceNativeAvailable.store(false);
					g_FaceLoaded.store(true);
					return;
				}
				{
					std::scoped_lock lock(g_Appearance.Mutex);
					g_Appearance.FaceValues = *values;
					g_Appearance.OriginalFaceValues = *values;
				}
				g_FaceLoaded.store(true);
			});
		}

		void QueueApplyFeature(std::size_t feature)
		{
			FiberPool::Push([feature] {
				if (feature >= kFaceFeatures.size())
					return;
				float value{};
				{
					std::scoped_lock lock(g_Appearance.Mutex);
					value = g_Appearance.FaceValues[feature];
				}
				const Ped ped = PLAYER::PLAYER_PED_ID();
				const bool ok = ApplyFaceFeatureOnFiber(ped, kFaceFeatures[feature].Index, value, true);
				g_FaceNativeAvailable.store(ok);
				if (ok)
					SaveAppearanceState();
			});
		}

		void AdjustSelectedFace(float delta)
		{
			if (!g_FaceLoaded.load() || !g_FaceNativeAvailable.load())
				return;
			{
				std::scoped_lock lock(g_Appearance.Mutex);
				g_Appearance.FaceValues[g_SelectedFeature] =
				    std::clamp(g_Appearance.FaceValues[g_SelectedFeature] + delta, -1.0f, 1.0f);
			}
			QueueApplyFeature(g_SelectedFeature);
		}

		void SelectPreviousFace()
		{
			g_SelectedFeature = g_SelectedFeature == 0 ? kFaceFeatures.size() - 1 : g_SelectedFeature - 1;
			g_FaceScrollRequested.store(true);
		}

		void SelectNextFace()
		{
			g_SelectedFeature = (g_SelectedFeature + 1) % kFaceFeatures.size();
			g_FaceScrollRequested.store(true);
		}

		void RestoreSelectedFaceValue()
		{
			if (!g_FaceLoaded.load())
				return;
			{
				std::scoped_lock lock(g_Appearance.Mutex);
				g_Appearance.FaceValues[g_SelectedFeature] = g_Appearance.OriginalFaceValues[g_SelectedFeature];
			}
			QueueApplyFeature(g_SelectedFeature);
		}

		void QueueRestoreOriginalFace()
		{
			if (!g_FaceLoaded.load())
				return;
			std::array<float, kFaceFeatures.size()> original{};
			{
				std::scoped_lock lock(g_Appearance.Mutex);
				original = g_Appearance.OriginalFaceValues;
				g_Appearance.FaceValues = original;
			}
			FiberPool::Push([original] {
				const Ped ped = PLAYER::PLAYER_PED_ID();
				const bool ok = RestoreFaceSnapshotOnFiber(ped, original, true);
				g_FaceNativeAvailable.store(ok);
				if (ok)
					SaveAppearanceState();
			});
		}

		void RenderSaveStatus()
		{
			ImGui::TextDisabled("Salvamento local automatico: %s", g_LastSaveSucceeded.load() ? "OK" : "aguardando/indisponivel");
		}

		void RenderFaceEditor()
		{
			if (!g_FaceLoaded.load())
			{
				ImGui::TextDisabled("Lendo as caracteristicas atuais...");
				return;
			}
			if (!g_FaceNativeAvailable.load())
			{
				ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.35f, 1.0f), "Falha ao acessar as natives faciais nesta execucao.");
				if (ImGui::Button("Tentar novamente"))
					QueueReadCurrentFace();
				return;
			}

			float currentValue{};
			{
				std::scoped_lock lock(g_Appearance.Mutex);
				currentValue = g_Appearance.FaceValues[g_SelectedFeature];
			}
			const auto& selected = kFaceFeatures[g_SelectedFeature];
			ImGui::Text("%s", selected.Label);
			ImGui::SameLine();
			ImGui::TextDisabled("[%zu/%zu]  %+.2f", g_SelectedFeature + 1, kFaceFeatures.size(), currentValue);
			ImGui::ProgressBar((currentValue + 1.0f) * 0.5f, ImVec2(-1.0f, 7.0f), "");
			ImGui::TextDisabled("Cima/Baixo: caracteristica | Q/E: diminuir/aumentar | BACK: sair");
			if (ImGui::Button("-##FaceValue"))
				AdjustSelectedFace(-0.05f);
			ImGui::SameLine();
			if (ImGui::Button("+##FaceValue"))
				AdjustSelectedFace(0.05f);
			ImGui::SameLine();
			if (ImGui::Button("Restaurar este"))
				RestoreSelectedFaceValue();
			ImGui::Separator();

			constexpr ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp;
			if (ImGui::BeginTable("##TenebrisFaceFeatures", 2, flags, ImVec2(0.0f, 370.0f)))
			{
				ImGui::TableSetupColumn("Caracteristica", ImGuiTableColumnFlags_WidthStretch, 0.78f);
				ImGui::TableSetupColumn("Valor", ImGuiTableColumnFlags_WidthFixed, 62.0f);
				ImGui::TableHeadersRow();
				const char* previousGroup = nullptr;
				for (std::size_t i = 0; i < kFaceFeatures.size(); ++i)
				{
					const auto& feature = kFaceFeatures[i];
					if (!previousGroup || std::string_view(previousGroup) != feature.Group)
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::TextDisabled("%s", feature.Group);
						previousGroup = feature.Group;
					}

					float value{};
					{
						std::scoped_lock lock(g_Appearance.Mutex);
						value = g_Appearance.FaceValues[i];
					}
					ImGui::PushID(static_cast<int>(i));
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					const bool selectedRow = g_SelectedFeature == i;
					if (ImGui::Selectable(feature.Label, selectedRow))
						g_SelectedFeature = i;
					if (selectedRow && g_FaceScrollRequested.exchange(false))
						ImGui::SetScrollHereY(0.5f);
					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%+.2f", value);
					ImGui::PopID();
				}
				ImGui::EndTable();
			}

			if (ImGui::Button("Restaurar rosto original"))
				QueueRestoreOriginalFace();
			ImGui::SameLine();
			if (ImGui::Button("Reler"))
				QueueReadCurrentFace();
			RenderSaveStatus();
		}

		class FaceEditorItem final : public UIItem
		{
		public:
			void Draw() override { RenderFaceEditor(); }
			std::string_view GetMenuLabel() const override { return "Editar caracteristicas faciais"; }
			std::string_view GetMenuDescription() const override
			{
				return "Ajusta cabeca, sobrancelhas, olhos, nariz, boca, maxilar, queixo e orelhas em tempo real.";
			}
			bool RequiresImGuiEditor() const override { return true; }
			float GetPreferredEditorHeight() const override { return 650.0f; }
			bool HandleEditorKey(int key) override
			{
				switch (key)
				{
				case VK_UP: SelectPreviousFace(); return true;
				case VK_DOWN: SelectNextFace(); return true;
				case 'Q': AdjustSelectedFace(-0.05f); return true;
				case 'E': AdjustSelectedFace(0.05f); return true;
				default: return false;
				}
			}
			void OnEditorOpened() override
			{
				g_FaceCameraOpen.store(true);
				g_SelectedFeature = 0;
				g_FaceScrollRequested.store(false);
				RequestEditorCamera(CameraFraming::Face);
				QueueReadCurrentFace();
			}
			void OnEditorClosed() override
			{
				g_FaceCameraOpen.store(false);
				RequestStopEditorCamera();
			}
		};

		// ------------------------------ ROUPAS ------------------------------
		void QueueLoadWardrobe()
		{
			g_WardrobeLoaded.store(false);
			g_WardrobeNativeAvailable.store(true);
			g_OutfitCount.store(0);
			FiberPool::Push([] {
				const Ped ped = PLAYER::PLAYER_PED_ID();
				const auto sex = GetCharacterSex(ped);
				g_CharacterSex.store(sex);
				if (sex == CharacterSex::Unsupported)
				{
					g_WardrobeLoaded.store(true);
					return;
				}
				const auto count = InvokeRaw<int>(kGetNumOutfitPresets, ped);
				if (!count)
				{
					g_WardrobeNativeAvailable.store(false);
					g_WardrobeLoaded.store(true);
					return;
				}
				g_OutfitCount.store(std::clamp(*count, 0, 512));
				{
					std::scoped_lock lock(g_Appearance.Mutex);
					g_Appearance.OutfitIndex = std::clamp(g_Appearance.OutfitIndex, 0, std::max(0, g_OutfitCount.load() - 1));
				}
				g_WardrobeLoaded.store(true);
			});
		}

		void ReapplyVoiceOnFiber(Ped ped)
		{
			int selectedVoice{};
			{
				std::scoped_lock lock(g_Appearance.Mutex);
				selectedVoice = g_Appearance.VoiceIndex;
			}
			if (selectedVoice <= 0)
				return;

			static constexpr std::array maleVoices{
			    "0791_A_M_M_MIDDLESDTOWNFOLK_01_WHITE_01",
			    "1046_A_M_O_WAPTOWNFOLK_01_NATIVE_01",
			    "0514_A_M_M_WAPWARRIORS_01_NATIVE_01",
			    "0515_A_M_M_WAPWARRIORS_01_NATIVE_02"};
			static constexpr std::array femaleVoices{
			    "0510_A_F_M_WAPTOWNFOLK_01_NATIVE_01",
			    "0511_A_F_M_WAPTOWNFOLK_01_NATIVE_02",
			    "1044_A_F_O_WAPTOWNFOLK_01_NATIVE_01",
			    "1045_A_F_O_WAPTOWNFOLK_01_NATIVE_02"};

			const auto sex = GetCharacterSex(ped);
			const int idx = std::clamp(selectedVoice - 1, 0, 3);
			if (sex == CharacterSex::Male)
				InvokeRawVoid(kSetAmbientVoiceName, ped, maleVoices[idx]);
			else if (sex == CharacterSex::Female)
				InvokeRawVoid(kSetAmbientVoiceName, ped, femaleVoices[idx]);
		}

		void QueueApplyOutfit(int index)
		{
			const int count = g_OutfitCount.load();
			if (index < 0 || index >= count || !g_WardrobeNativeAvailable.load())
				return;

			{
				std::scoped_lock lock(g_Appearance.Mutex);
				g_Appearance.OutfitIndex = index;
			}

			FiberPool::Push([index] {
				const Ped ped = PLAYER::PLAYER_PED_ID();
				if (!ped || !ENTITY::DOES_ENTITY_EXIST(ped))
					return;

				// O preset completo era a causa de rosto/pele mudarem: ele pode reconstruir
				// o MetaPed. Antes de aplica-lo preservamos cabeca/pele/cabelo/barba/olhos/
				// sobrancelha e todos os morphs faciais, e os restauramos antes do refresh.
				const auto faceBefore = ReadFaceSnapshotOnFiber(ped);
				const auto protectedBefore = CaptureProtectedComponentsOnFiber(ped);

				bool ok = InvokeRawVoid(kSetOutfitIndex, ped, index, true);
				if (ok)
					ok = InvokeRawVoid(kFixPedOutfit, ped) && ok;
				if (ok)
					ok = RestoreProtectedComponentsOnFiber(ped, protectedBefore) && ok;
				if (ok && faceBefore)
					ok = RestoreFaceSnapshotOnFiber(ped, *faceBefore, false) && ok;

				bool eyebrowRemoved{};
				{
					std::scoped_lock lock(g_Appearance.Mutex);
					eyebrowRemoved = g_Appearance.EyebrowRemoved;
				}
				if (ok && eyebrowRemoved)
					ok = InvokeRawVoid(kRemoveMetaPedTag, ped, kEyebrowCategory, 0) && ok;
				if (ok)
					ok = UpdatePedVariation(ped) && ok;
				if (ok)
					ReapplyVoiceOnFiber(ped);

				g_WardrobeNativeAvailable.store(ok);
				if (ok)
					SaveAppearanceState();
			});
		}

		int GetSelectedOutfit()
		{
			std::scoped_lock lock(g_Appearance.Mutex);
			return g_Appearance.OutfitIndex;
		}

		void MoveOutfitSelection(int delta, bool apply)
		{
			const int count = g_OutfitCount.load();
			if (count <= 0)
				return;
			int selected = GetSelectedOutfit();
			selected = (selected + delta) % count;
			if (selected < 0)
				selected += count;
			{
				std::scoped_lock lock(g_Appearance.Mutex);
				g_Appearance.OutfitIndex = selected;
			}
			g_WardrobeScrollRequested.store(true);
			if (apply)
				QueueApplyOutfit(selected);
		}

		std::string GetOutfitLabel(CharacterSex sex, int index)
		{
			const char* type = sex == CharacterSex::Male ? "Traje masculino" : "Traje feminino";
			return std::format("{} - indice {}", type, index);
		}

		void RenderWardrobeEditor()
		{
			if (!g_WardrobeLoaded.load())
			{
				ImGui::TextDisabled("Lendo o MetaPed e os trajes compativeis...");
				return;
			}
			const auto sex = g_CharacterSex.load();
			ImGui::Text("Personagem: %s", GetSexLabel(sex));
			if (sex == CharacterSex::Unsupported)
			{
				ImGui::TextWrapped("O editor de roupas aceita somente mp_male e mp_female para nao misturar modelos incompativeis.");
				return;
			}
			if (!g_WardrobeNativeAvailable.load())
			{
				ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.35f, 1.0f), "Falha ao aplicar ou consultar os trajes nesta execucao.");
				if (ImGui::Button("Recarregar roupas"))
					QueueLoadWardrobe();
				return;
			}

			const int count = g_OutfitCount.load();
			const int selected = std::clamp(GetSelectedOutfit(), 0, std::max(0, count - 1));
			ImGui::TextDisabled("%d trajes | selecionado: indice %d", count, selected);
			ImGui::TextDisabled("Q/E: traje anterior/seguinte e aplicar | Cima/Baixo: selecionar | Enter: aplicar");
			ImGui::TextWrapped("A protecao de aparencia restaura cabeca, pele, olhos, cabelo, barba, sobrancelha e morphs depois de cada preset.");
			ImGui::Separator();
			if (count <= 0)
				return;

			if (ImGui::BeginChild("##TenebrisWardrobeList", ImVec2(0.0f, 430.0f), true))
			{
				for (int i = 0; i < count; ++i)
				{
					const auto label = GetOutfitLabel(sex, i);
					ImGui::PushID(i);
					const bool selectedRow = selected == i;
					if (ImGui::Selectable(label.c_str(), selectedRow))
					{
						{
							std::scoped_lock lock(g_Appearance.Mutex);
							g_Appearance.OutfitIndex = i;
						}
						QueueApplyOutfit(i);
					}
					if (selectedRow && g_WardrobeScrollRequested.exchange(false))
						ImGui::SetScrollHereY(0.5f);
					ImGui::PopID();
				}
			}
			ImGui::EndChild();
			if (ImGui::Button("Aplicar selecionado"))
				QueueApplyOutfit(selected);
			ImGui::SameLine();
			if (ImGui::Button("Recarregar lista"))
				QueueLoadWardrobe();
			RenderSaveStatus();
		}

		class WardrobeEditorItem final : public UIItem
		{
		public:
			void Draw() override { RenderWardrobeEditor(); }
			std::string_view GetMenuLabel() const override { return "Editar roupas"; }
			std::string_view GetMenuDescription() const override
			{
				return "Troca trajes do modelo atual preservando rosto, pele, cabelo, barba, olhos e sobrancelha.";
			}
			bool RequiresImGuiEditor() const override { return true; }
			float GetPreferredEditorHeight() const override { return 650.0f; }
			bool HandleEditorKey(int key) override
			{
				switch (key)
				{
				case VK_UP: MoveOutfitSelection(-1, false); return true;
				case VK_DOWN: MoveOutfitSelection(1, false); return true;
				case 'Q': MoveOutfitSelection(-1, true); return true;
				case 'E': MoveOutfitSelection(1, true); return true;
				case VK_RETURN: QueueApplyOutfit(GetSelectedOutfit()); return true;
				default: return false;
				}
			}
			void OnEditorOpened() override
			{
				g_WardrobeCameraOpen.store(true);
				g_WardrobeScrollRequested.store(false);
				g_WardrobeNativeAvailable.store(true);
				RequestEditorCamera(CameraFraming::FullBody);
				QueueLoadWardrobe();
			}
			void OnEditorClosed() override
			{
				g_WardrobeCameraOpen.store(false);
				RequestStopEditorCamera();
			}
		};

		// --------------------------- SOBRANCELHA ---------------------------
		void QueueSetEyebrowRemoved(bool remove)
		{
			FiberPool::Push([remove] {
				const Ped ped = PLAYER::PLAYER_PED_ID();
				if (!ped || !ENTITY::DOES_ENTITY_EXIST(ped))
					return;

				bool ok = true;
				if (remove)
				{
					const auto components = CaptureProtectedComponentsOnFiber(ped);
					{
						std::scoped_lock lock(g_Appearance.Mutex);
						if (!g_Appearance.SavedEyebrow)
						{
							for (const auto& component : components)
								if (component.Category == kEyebrowCategory)
								{
									g_Appearance.SavedEyebrow = component;
									break;
								}
						}
					}
					ok = InvokeRawVoid(kRemoveMetaPedTag, ped, kEyebrowCategory, 0);
				}
				else
				{
					std::optional<MetaPedComponentSnapshot> eyebrow;
					{
						std::scoped_lock lock(g_Appearance.Mutex);
						eyebrow = g_Appearance.SavedEyebrow;
					}
					if (eyebrow)
						ok = RestoreProtectedComponentsOnFiber(ped, {*eyebrow});
					else
						ok = false;
				}

				if (ok)
					ok = UpdatePedVariation(ped);
				if (ok)
				{
					{
						std::scoped_lock lock(g_Appearance.Mutex);
						g_Appearance.EyebrowRemoved = remove;
					}
					SaveAppearanceState();
				}
			});
		}

		bool IsEyebrowRemoved()
		{
			std::scoped_lock lock(g_Appearance.Mutex);
			return g_Appearance.EyebrowRemoved;
		}

		void RenderEyebrowEditor()
		{
			const bool removed = IsEyebrowRemoved();
			ImGui::Text("Sobrancelha: %s", removed ? "Nenhuma" : "Atual");
			ImGui::TextDisabled("Q/E: alternar | BACK: sair");
			ImGui::TextWrapped("Nenhuma remove diretamente a categoria MetaPed eyebrows; Restaurar reaplica a sobrancelha capturada antes da remocao.");
			if (ImGui::Button("Atual / restaurar"))
				QueueSetEyebrowRemoved(false);
			ImGui::SameLine();
			if (ImGui::Button("Nenhuma"))
				QueueSetEyebrowRemoved(true);
			RenderSaveStatus();
		}

		class EyebrowEditorItem final : public UIItem
		{
		public:
			void Draw() override { RenderEyebrowEditor(); }
			std::string_view GetMenuLabel() const override { return "Sobrancelhas"; }
			std::string_view GetMenuDescription() const override { return "Permite remover completamente a categoria de sobrancelha e restaurar a anterior."; }
			bool RequiresImGuiEditor() const override { return true; }
			float GetPreferredEditorHeight() const override { return 260.0f; }
			bool HandleEditorKey(int key) override
			{
				if (key == 'Q' || key == 'E')
				{
					QueueSetEyebrowRemoved(!IsEyebrowRemoved());
					return true;
				}
				return false;
			}
			void OnEditorOpened() override
			{
				g_FaceCameraOpen.store(true);
				RequestEditorCamera(CameraFraming::Face);
			}
			void OnEditorClosed() override
			{
				g_FaceCameraOpen.store(false);
				RequestStopEditorCamera();
			}
		};

		// ------------------------------ VOZ ------------------------------
		int GetSelectedVoice()
		{
			std::scoped_lock lock(g_Appearance.Mutex);
			return g_Appearance.VoiceIndex;
		}

		void QueueApplyVoice(int voiceIndex)
		{
			voiceIndex = std::clamp(voiceIndex, 0, 4);
			{
				std::scoped_lock lock(g_Appearance.Mutex);
				g_Appearance.VoiceIndex = voiceIndex;
			}
			FiberPool::Push([voiceIndex] {
				const Ped ped = PLAYER::PLAYER_PED_ID();
				if (!ped || !ENTITY::DOES_ENTITY_EXIST(ped))
					return;
				if (voiceIndex > 0)
					ReapplyVoiceOnFiber(ped);
				SaveAppearanceState();
			});
		}

		const char* GetVoiceLabel(CharacterSex sex, int index)
		{
			if (index == 0)
				return "Atual (nao alterar)";
			static constexpr std::array maleLabels{"Middlesex Townfolk 01", "Wapiti Townfolk 01", "Wapiti Warrior 01", "Wapiti Warrior 02"};
			static constexpr std::array femaleLabels{"Wapiti Townfolk 01", "Wapiti Townfolk 02", "Wapiti Townfolk adulta 01", "Wapiti Townfolk adulta 02"};
			return sex == CharacterSex::Female ? femaleLabels[index - 1] : maleLabels[index - 1];
		}

		void MoveVoiceSelection(int delta)
		{
			int selected = GetSelectedVoice();
			selected = (selected + delta) % 5;
			if (selected < 0)
				selected += 5;
			QueueApplyVoice(selected);
		}

		void RenderVoiceEditor()
		{
			const Ped ped = PLAYER::PLAYER_PED_ID();
			const auto sex = GetCharacterSex(ped);
			const int selected = GetSelectedVoice();
			ImGui::Text("Voz: %s", GetVoiceLabel(sex, selected));
			ImGui::TextDisabled("Q/E: voz anterior/seguinte e aplicar | BACK: sair");
			ImGui::TextWrapped("Usa SET_AMBIENT_VOICE_NAME do jogo base. Ele troca o voice set ambiente do ped; alguns gritos de dano/morte podem usar eventos separados do banco de audio.");
			ImGui::Separator();
			for (int i = 0; i < 5; ++i)
			{
				ImGui::PushID(i);
				if (ImGui::Selectable(GetVoiceLabel(sex, i), selected == i))
					QueueApplyVoice(i);
				ImGui::PopID();
			}
			RenderSaveStatus();
		}

		class VoiceEditorItem final : public UIItem
		{
		public:
			void Draw() override { RenderVoiceEditor(); }
			std::string_view GetMenuLabel() const override { return "Voz do personagem"; }
			std::string_view GetMenuDescription() const override { return "Troca o voice set ambiente do proprio personagem usando uma native do jogo base."; }
			bool RequiresImGuiEditor() const override { return true; }
			float GetPreferredEditorHeight() const override { return 360.0f; }
			bool HandleEditorKey(int key) override
			{
				if (key == 'Q') { MoveVoiceSelection(-1); return true; }
				if (key == 'E') { MoveVoiceSelection(1); return true; }
				return false;
			}
		};
	}

	void InstallFaceEditor(const std::shared_ptr<Submenu>& selfSubmenu)
	{
		if (!selfSubmenu)
			return;

		auto clothesCategory = std::make_shared<Category>("Roupas");
		clothesCategory->AddItem(std::make_shared<WardrobeEditorItem>());
		selfSubmenu->AddCategory(std::move(clothesCategory));

		auto faceCategory = std::make_shared<Category>("Caracteristicas faciais");
		faceCategory->AddItem(std::make_shared<FaceEditorItem>());
		selfSubmenu->AddCategory(std::move(faceCategory));

		auto eyebrowCategory = std::make_shared<Category>("Sobrancelhas");
		eyebrowCategory->AddItem(std::make_shared<EyebrowEditorItem>());
		selfSubmenu->AddCategory(std::move(eyebrowCategory));

		auto voiceCategory = std::make_shared<Category>("Voz");
		voiceCategory->AddItem(std::make_shared<VoiceEditorItem>());
		selfSubmenu->AddCategory(std::move(voiceCategory));
	}
}
