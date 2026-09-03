#include "FaceEditor.hpp"

#include "core/frontend/manager/Category.hpp"
#include "core/frontend/manager/Submenu.hpp"
#include "game/frontend/items/Items.hpp"
#include "game/pointers/Pointers.hpp"
#include "game/rdr/Natives.hpp"
#include "game/rdr/invoker/Invoker.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace YimMenu::Submenus
{
	namespace
	{
		constexpr rage::scrNativeHash kSetFaceFeature = 0x5653AB26C82938CF;
		constexpr rage::scrNativeHash kGetFaceFeature = 0xFD1BA1EEF7985BB8;
		constexpr rage::scrNativeHash kUpdatePedVariation = 0xCC8CA3E88256E58F;

		struct FaceFeature
		{
			const char* label;
			std::uint32_t hash;
		};

		constexpr std::array<FaceFeature, 39> kFaceFeatures{{
		    {"Largura da cabeca", 0x84D6},
		    {"Altura da sobrancelha", 0x3303},
		    {"Largura da sobrancelha", 0x2FF9},
		    {"Profundidade da sobrancelha", 0x4AD1},
		    {"Largura da orelha", 0xC04F},
		    {"Angulo da orelha", 0xB6CE},
		    {"Altura da orelha", 0x2844},
		    {"Tamanho do lobulo", 0xED30},
		    {"Altura da maca do rosto", 0x6A0B},
		    {"Largura da maca do rosto", 0xABCF},
		    {"Profundidade da maca do rosto", 0x358D},
		    {"Altura do maxilar", 0x8D0A},
		    {"Largura do maxilar", 0xEBAE},
		    {"Profundidade do maxilar", 0x1DF6},
		    {"Altura do queixo", 0x3C0F},
		    {"Largura do queixo", 0xC3B2},
		    {"Profundidade do queixo", 0xE323},
		    {"Altura da palpebra", 0x8B2B},
		    {"Largura da palpebra", 0x1B6B},
		    {"Profundidade dos olhos", 0xEE44},
		    {"Angulo dos olhos", 0xD266},
		    {"Distancia entre os olhos", 0xA54E},
		    {"Altura dos olhos", 0xDDFB},
		    {"Largura do nariz", 0x6E7F},
		    {"Tamanho do nariz", 0x3471},
		    {"Altura do nariz", 0x03F5},
		    {"Angulo do nariz", 0x34B1},
		    {"Curvatura do nariz", 0xF156},
		    {"Distancia das narinas", 0x561E},
		    {"Largura da boca", 0xF065},
		    {"Profundidade da boca", 0xAA69},
		    {"Posicao horizontal da boca", 0x7AC3},
		    {"Posicao vertical da boca", 0x410D},
		    {"Altura do labio superior", 0x1A00},
		    {"Largura do labio superior", 0x91C1},
		    {"Profundidade do labio superior", 0xC375},
		    {"Altura do labio inferior", 0xBB4D},
		    {"Largura do labio inferior", 0xB0B0},
		    {"Profundidade do labio inferior", 0x5D16},
		}};

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

		void UpdateFace(Ped ped)
		{
			InvokeRawVoid(kUpdatePedVariation, ped, false, true, true, true, false);
		}

		bool ApplyFaceFeature(Ped ped, std::uint32_t featureHash, float value)
		{
			if (!InvokeRawVoid(kSetFaceFeature, ped, featureHash, value))
				return false;

			UpdateFace(ped);
			return true;
		}

		void DrawFeatureRange(Ped ped, std::array<float, kFaceFeatures.size()>& values, std::size_t first, std::size_t last, bool& nativeAvailable)
		{
			for (std::size_t i = first; i < last; ++i)
			{
				const auto& feature = kFaceFeatures[i];
				const std::string id = std::string(feature.label) + "##face_" + std::to_string(i);
				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::SliderFloat(id.c_str(), &values[i], -1.0f, 1.0f, "%.2f"))
					nativeAvailable = ApplyFaceFeature(ped, feature.hash, values[i]);
			}
		}

		void RenderFaceEditor()
		{
			static std::array<float, kFaceFeatures.size()> values{};
			static Ped loadedPed{};
			static bool initialized = false;
			static bool nativeAvailable = true;

			const Ped ped = PLAYER::PLAYER_PED_ID();

			auto readCurrentFace = [&] {
				nativeAvailable = true;
				for (std::size_t i = 0; i < kFaceFeatures.size(); ++i)
				{
					const auto current = InvokeRaw<float>(kGetFaceFeature, ped, kFaceFeatures[i].hash);
					if (!current)
					{
						nativeAvailable = false;
						break;
					}
					values[i] = std::clamp(*current, -1.0f, 1.0f);
				}
				loadedPed = ped;
				initialized = true;
			};

			if (!initialized || loadedPed != ped)
				readCurrentFace();

			ImGui::TextWrapped("Ajusta somente o rosto e a cabeca do seu personagem atual. As mudancas podem ser reaplicadas pelo proprio jogo ao trocar de aparencia ou sessao.");
			ImGui::Spacing();

			if (ImGui::Button("Ler rosto atual"))
				readCurrentFace();
			ImGui::SameLine();
			if (ImGui::Button("Centralizar todos"))
			{
				values.fill(0.0f);
				nativeAvailable = true;
				for (std::size_t i = 0; i < kFaceFeatures.size(); ++i)
				{
					if (!InvokeRawVoid(kSetFaceFeature, ped, kFaceFeatures[i].hash, values[i]))
					{
						nativeAvailable = false;
						break;
					}
				}
				if (nativeAvailable)
					UpdateFace(ped);
			}

			if (!nativeAvailable)
			{
				ImGui::Spacing();
				ImGui::TextDisabled("Native de edicao facial indisponivel nesta versao do jogo.");
			}

			ImGui::Separator();
			if (ImGui::CollapsingHeader("Cabeca e sobrancelhas", ImGuiTreeNodeFlags_DefaultOpen))
				DrawFeatureRange(ped, values, 0, 4, nativeAvailable);
			if (ImGui::CollapsingHeader("Orelhas", ImGuiTreeNodeFlags_DefaultOpen))
				DrawFeatureRange(ped, values, 4, 8, nativeAvailable);
			if (ImGui::CollapsingHeader("Macas, maxilar e queixo", ImGuiTreeNodeFlags_DefaultOpen))
				DrawFeatureRange(ped, values, 8, 17, nativeAvailable);
			if (ImGui::CollapsingHeader("Olhos", ImGuiTreeNodeFlags_DefaultOpen))
				DrawFeatureRange(ped, values, 17, 23, nativeAvailable);
			if (ImGui::CollapsingHeader("Nariz", ImGuiTreeNodeFlags_DefaultOpen))
				DrawFeatureRange(ped, values, 23, 29, nativeAvailable);
			if (ImGui::CollapsingHeader("Boca e labios", ImGuiTreeNodeFlags_DefaultOpen))
				DrawFeatureRange(ped, values, 29, 39, nativeAvailable);
		}
	}

	void InstallFaceEditor(const std::shared_ptr<Submenu>& selfSubmenu)
	{
		if (!selfSubmenu)
			return;

		auto faceCategory = std::make_shared<Category>("Rosto e cabeca");
		faceCategory->AddItem(std::make_shared<ImGuiItem>(
		    [] {
			    RenderFaceEditor();
		    },
		    "Editor de rosto",
		    "Altera sobrancelhas, olhos, nariz, boca, maxilar, queixo, orelhas e formato da cabeca do seu personagem.",
		    560.0f));
		selfSubmenu->AddCategory(std::move(faceCategory));
	}
}
