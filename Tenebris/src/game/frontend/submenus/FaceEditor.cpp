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
#include <mutex>
#include <optional>
#include <string>

namespace YimMenu::Submenus
{
	namespace
	{
		// Natives do jogo base presentes no NativeDB/Crossmap do proprio Tenebris.
		constexpr rage::scrNativeHash kSetFaceFeature = 0x5653AB26C82938CF;      // _SET_CHAR_EXPRESSION
		constexpr rage::scrNativeHash kGetFaceFeature = 0xFD1BA1EEF7985BB8;      // _GET_CHAR_EXPRESSION
		constexpr rage::scrNativeHash kFixPedOutfit = 0x704C908E9C405136;
		constexpr rage::scrNativeHash kUpdatePedVariation = 0xCC8CA3E88256E58F;
		constexpr rage::scrNativeHash kGetNumOutfitPresets = 0x10C70A515BC03707;
		constexpr rage::scrNativeHash kSetOutfitIndex = 0x77FF8D35EEC6BBC4;

		constexpr std::uint32_t kMpMaleModel = 0xF5C1611E;
		constexpr std::uint32_t kMpFemaleModel = 0xA7AF20C0;
		constexpr int kHeadBoneId = 21030; // SKEL_Head em mp_male e mp_female.

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
			const char* label;
			std::int32_t index;
			const char* group;
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

		enum class CameraFraming
		{
			Face,
			FullBody
		};

		enum class CharacterSex : int
		{
			Unsupported = 0,
			Male,
			Female
		};

		Cam g_EditorCamera{};
		std::atomic_bool g_FaceEditorOpen{false};
		std::atomic_bool g_WardrobeEditorOpen{false};

		void StopEditorCameraOnFiber(bool releasePed)
		{
			if (g_EditorCamera && CAM::DOES_CAM_EXIST(g_EditorCamera))
			{
				CAM::RENDER_SCRIPT_CAMS(false, true, 300, true, true, 0);
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
			const bool shouldOpen = framing == CameraFraming::Face ? g_FaceEditorOpen.load() : g_WardrobeEditorOpen.load();
			if (!shouldOpen)
				return;

			const Ped ped = PLAYER::PLAYER_PED_ID();
			if (!ped || !ENTITY::DOES_ENTITY_EXIST(ped))
				return;

			StopEditorCameraOnFiber(false);

			// O editor do jogo deixa o personagem parado e move a camera ao redor do
			// MetaPed. Aqui fazemos o mesmo sem depender de coordenadas de uma loja.
			TASK::CLEAR_PED_TASKS_IMMEDIATELY(ped, true, true);
			ENTITY::FREEZE_ENTITY_POSITION(ped, true);

			const auto head = PED::GET_PED_BONE_COORDS(ped, kHeadBoneId, 0.0f, 0.0f, 0.0f);
			const auto forward = ENTITY::GET_ENTITY_FORWARD_VECTOR(ped);

			float distance{};
			float cameraZOffset{};
			float targetZOffset{};
			float fov{};
			if (framing == CameraFraming::Face)
			{
				distance = 0.78f;
				cameraZOffset = 0.01f;
				targetZOffset = -0.025f;
				fov = 30.0f;
			}
			else
			{
				distance = 2.70f;
				cameraZOffset = -0.72f;
				targetZOffset = -0.72f;
				fov = 38.0f;
			}

			const float cameraX = head.x + forward.x * distance;
			const float cameraY = head.y + forward.y * distance;
			const float cameraZ = head.z + cameraZOffset;
			const float targetX = head.x;
			const float targetY = head.y;
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
			CAM::RENDER_SCRIPT_CAMS(true, true, 300, true, true, 0);
		}

		bool UpdatePedVariation(Ped ped)
		{
			return InvokeRawVoid(kUpdatePedVariation, ped, false, true, true, true, false);
		}

		bool ApplyFaceFeatureOnFiber(Ped ped, std::int32_t featureIndex, float value, bool update = true)
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
				const auto current = InvokeRaw<float>(kGetFaceFeature, ped, kFaceFeatures[i].index);
				if (!current)
					return std::nullopt;
				values[i] = std::clamp(*current, -1.0f, 1.0f);
			}
			return values;
		}

		bool RestoreFaceSnapshotOnFiber(Ped ped, const std::array<float, kFaceFeatures.size()>& values)
		{
			bool ok = true;
			for (std::size_t i = 0; i < kFaceFeatures.size(); ++i)
				ok = ApplyFaceFeatureOnFiber(ped, kFaceFeatures[i].index, values[i], false) && ok;
			return ok && UpdatePedVariation(ped);
		}

		// ------------------------------ ROSTO ------------------------------
		std::atomic_bool g_FaceLoaded{false};
		std::atomic_bool g_FaceNativeAvailable{true};
		std::atomic_bool g_FaceScrollRequested{false};
		std::mutex g_FaceStateMutex;
		std::array<float, kFaceFeatures.size()> g_FaceValues{};
		std::array<float, kFaceFeatures.size()> g_OriginalFaceValues{};
		std::size_t g_SelectedFeature{};

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
					std::scoped_lock lock(g_FaceStateMutex);
					g_FaceValues = *values;
					g_OriginalFaceValues = *values;
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
					std::scoped_lock lock(g_FaceStateMutex);
					value = g_FaceValues[feature];
				}

				const Ped ped = PLAYER::PLAYER_PED_ID();
				g_FaceNativeAvailable.store(ApplyFaceFeatureOnFiber(ped, kFaceFeatures[feature].index, value));
			});
		}

		void AdjustSelectedFace(float delta)
		{
			if (!g_FaceLoaded.load() || !g_FaceNativeAvailable.load())
				return;

			{
				std::scoped_lock lock(g_FaceStateMutex);
				g_FaceValues[g_SelectedFeature] = std::clamp(g_FaceValues[g_SelectedFeature] + delta, -1.0f, 1.0f);
			}
			QueueApplyFeature(g_SelectedFeature);
		}

		void RestoreSelectedFaceValue()
		{
			if (!g_FaceLoaded.load())
				return;
			{
				std::scoped_lock lock(g_FaceStateMutex);
				g_FaceValues[g_SelectedFeature] = g_OriginalFaceValues[g_SelectedFeature];
			}
			QueueApplyFeature(g_SelectedFeature);
		}

		void QueueRestoreOriginalFace()
		{
			if (!g_FaceLoaded.load())
				return;

			std::array<float, kFaceFeatures.size()> original{};
			{
				std::scoped_lock lock(g_FaceStateMutex);
				original = g_OriginalFaceValues;
				g_FaceValues = original;
			}

			FiberPool::Push([original] {
				const Ped ped = PLAYER::PLAYER_PED_ID();
				g_FaceNativeAvailable.store(RestoreFaceSnapshotOnFiber(ped, original));
			});
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
				std::scoped_lock lock(g_FaceStateMutex);
				currentValue = g_FaceValues[g_SelectedFeature];
			}

			const auto& selected = kFaceFeatures[g_SelectedFeature];
			ImGui::Text("%s", selected.label);
			ImGui::SameLine();
			ImGui::TextDisabled("%+.2f", currentValue);
			ImGui::ProgressBar((currentValue + 1.0f) * 0.5f, ImVec2(-1.0f, 7.0f), "");
			ImGui::TextDisabled("Cima/Baixo: selecionar | Esquerda/Direita ou Home/End: ajustar");
			ImGui::Separator();

			constexpr ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp;
			if (ImGui::BeginTable("##TenebrisFaceFeatures", 3, flags, ImVec2(0.0f, 350.0f)))
			{
				ImGui::TableSetupColumn("Grupo", ImGuiTableColumnFlags_WidthStretch, 0.34f);
				ImGui::TableSetupColumn("Caracteristica", ImGuiTableColumnFlags_WidthStretch, 0.48f);
				ImGui::TableSetupColumn("Valor", ImGuiTableColumnFlags_WidthFixed, 70.0f);
				ImGui::TableHeadersRow();

				const char* previousGroup = nullptr;
				for (std::size_t i = 0; i < kFaceFeatures.size(); ++i)
				{
					float value{};
					{
						std::scoped_lock lock(g_FaceStateMutex);
						value = g_FaceValues[i];
					}

					const auto& feature = kFaceFeatures[i];
					const bool selectedRow = g_SelectedFeature == i;
					ImGui::PushID(static_cast<int>(i));
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					if (!previousGroup || std::string_view(previousGroup) != feature.group)
					{
						ImGui::TextDisabled("%s", feature.group);
						previousGroup = feature.group;
					}

					ImGui::TableSetColumnIndex(1);
					if (ImGui::Selectable(feature.label, selectedRow))
						g_SelectedFeature = i;
					if (selectedRow && g_FaceScrollRequested.exchange(false))
						ImGui::SetScrollHereY(0.5f);

					ImGui::TableSetColumnIndex(2);
					ImGui::Text("%+.2f", value);
					ImGui::PopID();
				}
				ImGui::EndTable();
			}

			ImGui::Spacing();
			if (ImGui::Button("Restaurar selecionado"))
				RestoreSelectedFaceValue();
			ImGui::SameLine();
			if (ImGui::Button("Restaurar tudo"))
				QueueRestoreOriginalFace();
			ImGui::SameLine();
			if (ImGui::Button("Reler"))
				QueueReadCurrentFace();
		}

		class FaceEditorItem final : public UIItem
		{
		public:
			void Draw() override
			{
				RenderFaceEditor();
			}

			std::string_view GetMenuLabel() const override
			{
				return "Editar caracteristicas faciais";
			}

			std::string_view GetMenuDescription() const override
			{
				return "Ajusta formato da cabeca, sobrancelhas, olhos, nariz, boca, maxilar, queixo e orelhas do seu personagem.";
			}

			bool RequiresImGuiEditor() const override
			{
				return true;
			}

			float GetPreferredEditorHeight() const override
			{
				return 600.0f;
			}

			bool HandleEditorKey(int key) override
			{
				switch (key)
				{
				case VK_UP:
					SelectPreviousFace();
					return true;
				case VK_DOWN:
					SelectNextFace();
					return true;
				case VK_LEFT:
				case VK_HOME:
					AdjustSelectedFace(-0.05f);
					return true;
				case VK_RIGHT:
				case VK_END:
					AdjustSelectedFace(0.05f);
					return true;
				default:
					return false;
				}
			}

			void OnEditorOpened() override
			{
				g_FaceEditorOpen.store(true);
				g_SelectedFeature = 0;
				g_FaceScrollRequested.store(false);
				g_FaceLoaded.store(false);
				g_FaceNativeAvailable.store(true);
				FiberPool::Push([] { StartEditorCameraOnFiber(CameraFraming::Face); });
				QueueReadCurrentFace();
			}

			void OnEditorClosed() override
			{
				g_FaceEditorOpen.store(false);
				FiberPool::Push([] { StopEditorCameraOnFiber(true); });
			}
		};

		// ------------------------------ ROUPAS ------------------------------
		std::atomic_bool g_WardrobeLoaded{false};
		std::atomic_bool g_WardrobeNativeAvailable{true};
		std::atomic_bool g_WardrobeScrollRequested{false};
		std::atomic<int> g_OutfitCount{0};
		std::atomic<int> g_SelectedOutfit{0};
		std::atomic<CharacterSex> g_CharacterSex{CharacterSex::Unsupported};

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

		void QueueLoadWardrobe()
		{
			g_WardrobeLoaded.store(false);
			g_WardrobeNativeAvailable.store(true);
			g_OutfitCount.store(0);
			g_SelectedOutfit.store(0);
			g_CharacterSex.store(CharacterSex::Unsupported);

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
				g_WardrobeLoaded.store(true);
			});
		}

		void QueueApplyOutfit(int index)
		{
			const int count = g_OutfitCount.load();
			if (index < 0 || index >= count || !g_WardrobeNativeAvailable.load())
				return;

			FiberPool::Push([index] {
				const Ped ped = PLAYER::PLAYER_PED_ID();
				if (!ped || !ENTITY::DOES_ENTITY_EXIST(ped))
					return;

				// Alguns presets completos podem reconstruir o MetaPed. Guardamos os
				// morphs antes da troca e reaplicamos depois para roupa nao apagar o rosto.
				const auto faceBeforeOutfit = ReadFaceSnapshotOnFiber(ped);

				bool ok = InvokeRawVoid(kSetOutfitIndex, ped, index, true);
				if (ok)
					InvokeRawVoid(kFixPedOutfit, ped);

				if (ok && faceBeforeOutfit)
					ok = RestoreFaceSnapshotOnFiber(ped, *faceBeforeOutfit);
				else if (ok)
					ok = UpdatePedVariation(ped);

				g_WardrobeNativeAvailable.store(ok);
			});
		}

		void MoveOutfitSelection(int delta, bool apply)
		{
			const int count = g_OutfitCount.load();
			if (count <= 0)
				return;

			int selected = g_SelectedOutfit.load();
			selected = (selected + delta) % count;
			if (selected < 0)
				selected += count;
			g_SelectedOutfit.store(selected);
			g_WardrobeScrollRequested.store(true);
			if (apply)
				QueueApplyOutfit(selected);
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
				ImGui::TextWrapped("Este editor de roupas foi limitado aos modelos online mp_male e mp_female para evitar aplicar componentes de sexo/modelo incompatível.");
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
			const int selected = std::clamp(g_SelectedOutfit.load(), 0, std::max(0, count - 1));
			ImGui::SameLine();
			ImGui::TextDisabled("| %d trajes do modelo", count);
			ImGui::TextDisabled("Cima/Baixo: selecionar | Esquerda/Direita ou Home/End: trocar | Enter: aplicar");
			ImGui::Separator();

			if (count <= 0)
			{
				ImGui::TextDisabled("Nenhum preset de roupa foi informado pelo jogo para este MetaPed.");
				return;
			}

			const char* prefix = sex == CharacterSex::Male ? "Traje masculino" : "Traje feminino";
			if (ImGui::BeginChild("##TenebrisWardrobeList", ImVec2(0.0f, 405.0f), true))
			{
				for (int i = 0; i < count; ++i)
				{
					const std::string label = std::format("{} {:03d}", prefix, i + 1);
					ImGui::PushID(i);
					const bool selectedRow = selected == i;
					if (ImGui::Selectable(label.c_str(), selectedRow))
					{
						g_SelectedOutfit.store(i);
						QueueApplyOutfit(i);
					}
					if (selectedRow && g_WardrobeScrollRequested.exchange(false))
						ImGui::SetScrollHereY(0.5f);
					ImGui::PopID();
				}
			}
			ImGui::EndChild();

			ImGui::Spacing();
			if (ImGui::Button("Aplicar selecionado"))
				QueueApplyOutfit(g_SelectedOutfit.load());
			ImGui::SameLine();
			if (ImGui::Button("Recarregar lista"))
				QueueLoadWardrobe();
		}

		class WardrobeEditorItem final : public UIItem
		{
		public:
			void Draw() override
			{
				RenderWardrobeEditor();
			}

			std::string_view GetMenuLabel() const override
			{
				return "Editar roupas";
			}

			std::string_view GetMenuDescription() const override
			{
				return "Mostra somente os trajes compativeis com o modelo masculino ou feminino do seu personagem online.";
			}

			bool RequiresImGuiEditor() const override
			{
				return true;
			}

			float GetPreferredEditorHeight() const override
			{
				return 600.0f;
			}

			bool HandleEditorKey(int key) override
			{
				switch (key)
				{
				case VK_UP:
					MoveOutfitSelection(-1, false);
					return true;
				case VK_DOWN:
					MoveOutfitSelection(1, false);
					return true;
				case VK_LEFT:
				case VK_HOME:
					MoveOutfitSelection(-1, true);
					return true;
				case VK_RIGHT:
				case VK_END:
					MoveOutfitSelection(1, true);
					return true;
				case VK_RETURN:
					QueueApplyOutfit(g_SelectedOutfit.load());
					return true;
				default:
					return false;
				}
			}

			void OnEditorOpened() override
			{
				g_WardrobeEditorOpen.store(true);
				g_WardrobeScrollRequested.store(false);
				g_WardrobeNativeAvailable.store(true);
				FiberPool::Push([] { StartEditorCameraOnFiber(CameraFraming::FullBody); });
				QueueLoadWardrobe();
			}

			void OnEditorClosed() override
			{
				g_WardrobeEditorOpen.store(false);
				FiberPool::Push([] { StopEditorCameraOnFiber(true); });
			}
		};
	}

	void InstallFaceEditor(const std::shared_ptr<Submenu>& selfSubmenu)
	{
		if (!selfSubmenu)
			return;

		auto faceCategory = std::make_shared<Category>("Caracteristicas faciais");
		faceCategory->AddItem(std::make_shared<FaceEditorItem>());
		selfSubmenu->AddCategory(std::move(faceCategory));

		auto clothesCategory = std::make_shared<Category>("Roupas");
		clothesCategory->AddItem(std::make_shared<WardrobeEditorItem>());
		selfSubmenu->AddCategory(std::move(clothesCategory));
	}
}
