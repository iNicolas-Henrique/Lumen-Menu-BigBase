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
		// Todos os hashes abaixo pertencem a tabela nativa do RDR2/RDO incluida
		// no proprio Tenebris. Nao ha dependencia de CFX/RedM.
		constexpr rage::scrNativeHash kSetFaceFeature = 0x5653AB26C82938CF;
		constexpr rage::scrNativeHash kGetFaceFeature = 0xFD1BA1EEF7985BB8;
		constexpr rage::scrNativeHash kRefreshPedVariation = 0x704C908E9C405136;
		constexpr rage::scrNativeHash kUpdatePedVariation = 0xCC8CA3E88256E58F;

		consteval bool NativeExistsInTenebris(rage::scrNativeHash hash)
		{
			for (const auto native : g_Crossmap)
				if (native == hash)
					return true;
			return false;
		}

		static_assert(NativeExistsInTenebris(kSetFaceFeature));
		static_assert(NativeExistsInTenebris(kGetFaceFeature));
		static_assert(NativeExistsInTenebris(kRefreshPedVariation));
		static_assert(NativeExistsInTenebris(kUpdatePedVariation));

		Cam g_FaceCamera{};
		std::atomic_bool g_EditorOpen{false};
		std::atomic_bool g_FaceLoaded{false};
		std::atomic_bool g_NativeAvailable{true};
		std::mutex g_FaceStateMutex;

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

		std::array<float, kFaceFeatures.size()> g_FaceValues{};
		std::array<float, kFaceFeatures.size()> g_OriginalValues{};
		std::size_t g_SelectedFeature{};

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

		void StopFaceCameraOnFiber()
		{
			if (!g_FaceCamera || !CAM::DOES_CAM_EXIST(g_FaceCamera))
			{
				g_FaceCamera = 0;
				return;
			}

			CAM::RENDER_SCRIPT_CAMS(false, true, 350, true, true, 0);
			CAM::DETACH_CAM(g_FaceCamera);
			CAM::SET_CAM_ACTIVE(g_FaceCamera, false);
			CAM::DESTROY_CAM(g_FaceCamera, true);
			g_FaceCamera = 0;
		}

		void StartFaceCameraOnFiber()
		{
			if (!g_EditorOpen.load())
				return;

			const Ped ped = PLAYER::PLAYER_PED_ID();
			if (!ped || !ENTITY::DOES_ENTITY_EXIST(ped))
				return;

			StopFaceCameraOnFiber();
			g_FaceCamera = CAM::CREATE_CAM("DEFAULT_SCRIPTED_CAMERA", true);
			if (!g_FaceCamera || !CAM::DOES_CAM_EXIST(g_FaceCamera))
			{
				g_FaceCamera = 0;
				return;
			}

			// Posicao relativa ao proprio ped: bem em frente ao rosto, em vez de
			// uma camera distante presa ao tronco. Como e relativa, acompanha o ped.
			CAM::ATTACH_CAM_TO_ENTITY(g_FaceCamera, ped, 0.0f, 0.58f, 1.66f, true);
			CAM::POINT_CAM_AT_ENTITY(g_FaceCamera, ped, 0.0f, 0.0f, 1.61f, true);
			CAM::SET_CAM_FOV(g_FaceCamera, 22.0f);
			CAM::SET_CAM_NEAR_CLIP(g_FaceCamera, 0.03f);
			CAM::SET_CAM_ACTIVE(g_FaceCamera, true);
			CAM::RENDER_SCRIPT_CAMS(true, true, 350, true, true, 0);
		}

		bool RefreshPedVariation(Ped ped)
		{
			// Sequencia usada pelo proprio sistema MetaPed do jogo base apos alterar
			// expressoes faciais. Nao espera em loop e nao usa APIs de RP.
			if (!InvokeRawVoid(kRefreshPedVariation, ped))
				return false;
			return InvokeRawVoid(kUpdatePedVariation, ped, false, true, true, true, false);
		}

		bool ApplyFaceFeatureOnFiber(Ped ped, std::int32_t featureIndex, float value, bool refresh = true)
		{
			if (!ped || !ENTITY::DOES_ENTITY_EXIST(ped))
				return false;
			if (!InvokeRawVoid(kSetFaceFeature, ped, featureIndex, std::clamp(value, -1.0f, 1.0f)))
				return false;
			return !refresh || RefreshPedVariation(ped);
		}

		void QueueReadCurrentFace()
		{
			g_FaceLoaded.store(false);
			g_NativeAvailable.store(true);

			FiberPool::Push([] {
				const Ped ped = PLAYER::PLAYER_PED_ID();
				if (!ped || !ENTITY::DOES_ENTITY_EXIST(ped))
				{
					g_NativeAvailable.store(false);
					g_FaceLoaded.store(true);
					return;
				}

				std::array<float, kFaceFeatures.size()> loaded{};
				for (std::size_t i = 0; i < kFaceFeatures.size(); ++i)
				{
					const auto current = InvokeRaw<float>(kGetFaceFeature, ped, kFaceFeatures[i].index);
					if (!current)
					{
						g_NativeAvailable.store(false);
						g_FaceLoaded.store(true);
						return;
					}
					loaded[i] = std::clamp(*current, -1.0f, 1.0f);
				}

				{
					std::scoped_lock lock(g_FaceStateMutex);
					g_FaceValues = loaded;
					g_OriginalValues = loaded;
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
				g_NativeAvailable.store(ApplyFaceFeatureOnFiber(ped, kFaceFeatures[feature].index, value));
			});
		}

		void AdjustSelected(float delta)
		{
			if (!g_FaceLoaded.load() || !g_NativeAvailable.load())
				return;

			{
				std::scoped_lock lock(g_FaceStateMutex);
				g_FaceValues[g_SelectedFeature] = std::clamp(g_FaceValues[g_SelectedFeature] + delta, -1.0f, 1.0f);
			}
			QueueApplyFeature(g_SelectedFeature);
		}

		void ZeroSelected()
		{
			if (!g_FaceLoaded.load() || !g_NativeAvailable.load())
				return;
			{
				std::scoped_lock lock(g_FaceStateMutex);
				g_FaceValues[g_SelectedFeature] = 0.0f;
			}
			QueueApplyFeature(g_SelectedFeature);
		}

		void QueueRestoreOriginal()
		{
			if (!g_FaceLoaded.load())
				return;

			std::array<float, kFaceFeatures.size()> original{};
			{
				std::scoped_lock lock(g_FaceStateMutex);
				original = g_OriginalValues;
				g_FaceValues = original;
			}

			FiberPool::Push([original] {
				const Ped ped = PLAYER::PLAYER_PED_ID();
				if (!ped || !ENTITY::DOES_ENTITY_EXIST(ped))
					return;

				bool ok = true;
				for (std::size_t i = 0; i < kFaceFeatures.size(); ++i)
					ok = ApplyFaceFeatureOnFiber(ped, kFaceFeatures[i].index, original[i], false) && ok;
				if (ok)
					ok = RefreshPedVariation(ped);
				g_NativeAvailable.store(ok);
			});
		}

		void SelectPrevious()
		{
			g_SelectedFeature = g_SelectedFeature == 0 ? kFaceFeatures.size() - 1 : g_SelectedFeature - 1;
		}

		void SelectNext()
		{
			g_SelectedFeature = (g_SelectedFeature + 1) % kFaceFeatures.size();
		}

		void DrawFeatureList()
		{
			if (!ImGui::BeginChild("##FaceFeatureList", ImVec2(0.0f, 255.0f), true))
			{
				ImGui::EndChild();
				return;
			}

			const char* previousGroup = nullptr;
			for (std::size_t i = 0; i < kFaceFeatures.size(); ++i)
			{
				const auto& feature = kFaceFeatures[i];
				if (!previousGroup || std::string_view(previousGroup) != feature.group)
				{
					if (previousGroup)
						ImGui::Spacing();
					ImGui::TextDisabled("%s", feature.group);
					ImGui::Separator();
					previousGroup = feature.group;
				}

				ImGui::PushID(static_cast<int>(i));
				if (ImGui::Selectable(feature.label, g_SelectedFeature == i))
					g_SelectedFeature = i;
				ImGui::PopID();
			}

			ImGui::EndChild();
		}

		void RenderFaceEditor()
		{
			if (!g_FaceLoaded.load())
			{
				ImGui::TextDisabled("Lendo o rosto atual do personagem...");
				ImGui::TextWrapped("A camera e a leitura facial sao executadas pela thread de script do jogo para evitar chamadas nativas pelo renderizador.");
				return;
			}

			if (!g_NativeAvailable.load())
			{
				ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.35f, 1.0f), "Falha ao acessar as natives faciais nesta execucao.");
				if (ImGui::Button("Tentar ler novamente"))
					QueueReadCurrentFace();
				return;
			}

			float currentValue{};
			{
				std::scoped_lock lock(g_FaceStateMutex);
				currentValue = g_FaceValues[g_SelectedFeature];
			}

			const auto& selected = kFaceFeatures[g_SelectedFeature];
			ImGui::TextDisabled("%s", selected.group);
			ImGui::Text("Ajuste %zu / %zu", g_SelectedFeature + 1, kFaceFeatures.size());
			ImGui::Spacing();
			ImGui::TextWrapped("%s", selected.label);
			ImGui::Text("Valor: %+.2f", currentValue);
			ImGui::ProgressBar((currentValue + 1.0f) * 0.5f, ImVec2(-1.0f, 8.0f), "");
			ImGui::Spacing();

			if (ImGui::Button("- 0.05", ImVec2(86.0f, 0.0f)))
				AdjustSelected(-0.05f);
			ImGui::SameLine();
			if (ImGui::Button("+ 0.05", ImVec2(86.0f, 0.0f)))
				AdjustSelected(0.05f);
			ImGui::SameLine();
			if (ImGui::Button("Zerar atual"))
				ZeroSelected();

			ImGui::Spacing();
			ImGui::TextDisabled("Cima/Baixo: escolher parte");
			ImGui::TextDisabled("Esquerda/Direita ou Home/End: diminuir/aumentar");
			ImGui::Separator();

			DrawFeatureList();
			ImGui::Spacing();

			if (ImGui::Button("Reler rosto atual"))
				QueueReadCurrentFace();
			ImGui::SameLine();
			if (ImGui::Button("Restaurar ao abrir"))
				QueueRestoreOriginal();
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
				return "Editor de rosto";
			}

			std::string_view GetMenuDescription() const override
			{
				return "Altera sobrancelhas, olhos, nariz, boca, maxilar, queixo, orelhas e formato da cabeca do seu personagem.";
			}

			bool RequiresImGuiEditor() const override
			{
				return true;
			}

			float GetPreferredEditorHeight() const override
			{
				return 560.0f;
			}

			bool HandleEditorKey(int key) override
			{
				switch (key)
				{
				case VK_UP:
					SelectPrevious();
					return true;
				case VK_DOWN:
					SelectNext();
					return true;
				case VK_LEFT:
				case VK_HOME:
					AdjustSelected(-0.05f);
					return true;
				case VK_RIGHT:
				case VK_END:
					AdjustSelected(0.05f);
					return true;
				default:
					return false;
				}
			}

			void OnEditorOpened() override
			{
				g_EditorOpen.store(true);
				g_SelectedFeature = 0;
				g_FaceLoaded.store(false);
				g_NativeAvailable.store(true);

				// Camera e natives de MetaPed precisam rodar no contexto de script do jogo,
				// nao diretamente no callback de ImGui/WndProc.
				FiberPool::Push([] { StartFaceCameraOnFiber(); });
				QueueReadCurrentFace();
			}

			void OnEditorClosed() override
			{
				g_EditorOpen.store(false);
				FiberPool::Push([] { StopFaceCameraOnFiber(); });
			}
		};
	}

	void InstallFaceEditor(const std::shared_ptr<Submenu>& selfSubmenu)
	{
		if (!selfSubmenu)
			return;

		auto faceCategory = std::make_shared<Category>("Rosto e cabeca");
		faceCategory->AddItem(std::make_shared<FaceEditorItem>());
		selfSubmenu->AddCategory(std::move(faceCategory));
	}
}
