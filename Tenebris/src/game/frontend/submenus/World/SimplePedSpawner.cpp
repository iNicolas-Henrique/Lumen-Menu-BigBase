#include "SimplePedSpawner.hpp"

#include "core/frontend/Localization.hpp"
#include "core/frontend/Notifications.hpp"
#include "core/frontend/manager/UIItem.hpp"
#include "game/backend/FiberPool.hpp"
#include "game/backend/ScriptMgr.hpp"
#include "game/backend/Self.hpp"
#include "game/rdr/Natives.hpp"
#include "game/rdr/Ped.hpp"
#include "game/rdr/data/PedModels.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace YimMenu::Submenus
{
	namespace
	{
		enum class PedListKind
		{
			Human,
			Animal
		};

		struct PedEntry
		{
			Hash HashValue{};
			std::string Model;
			std::string Display;
		};

		std::string Lower(std::string_view value)
		{
			std::string result(value);
			std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
			return result;
		}

		bool IsAnimalModel(std::string_view model)
		{
			const std::string lower = Lower(model);
			if (lower.starts_with("a_c_") || lower.starts_with("mp_a_c_"))
				return true;

			// A small number of story/cutscene animal models use CS_ instead of A_C_.
			if (!lower.starts_with("cs_"))
				return false;
			static constexpr std::array animalTokens = {
			    "horse", "dog", "cat", "bear", "wolf", "cougar", "panther", "deer", "buck", "elk", "moose", "boar",
			    "alligator", "buffalo", "bull", "cow", "sheep", "goat", "donkey", "mule", "eagle", "owl", "raven", "coyote"};
			for (const auto token : animalTokens)
				if (lower.find(token) != std::string::npos)
					return true;
			return false;
		}

		std::vector<PedEntry> BuildEntries(PedListKind kind)
		{
			std::vector<PedEntry> entries;
			entries.reserve(Data::g_PedModels.size());
			for (const auto& [hash, model] : Data::g_PedModels)
			{
				if (!model || !*model)
					continue;
				const bool animal = IsAnimalModel(model);
				if ((kind == PedListKind::Animal) != animal)
					continue;
				PedEntry entry;
				entry.HashValue = static_cast<Hash>(hash);
				entry.Model = model;
				entry.Display = Data::GetPedDisplayName(model);
				if (entry.Display.empty())
					entry.Display = entry.Model;
				entries.push_back(std::move(entry));
			}
			std::sort(entries.begin(), entries.end(), [](const PedEntry& a, const PedEntry& b) {
				return Lower(a.Display) < Lower(b.Display);
			});
			return entries;
		}

		const std::vector<PedEntry>& GetEntries(PedListKind kind)
		{
			static const std::vector<PedEntry> humans = BuildEntries(PedListKind::Human);
			static const std::vector<PedEntry> animals = BuildEntries(PedListKind::Animal);
			return kind == PedListKind::Human ? humans : animals;
		}

		void SpawnPed(const PedEntry& entry)
		{
			const Hash model = entry.HashValue;
			if (!STREAMING::IS_MODEL_VALID(model))
			{
				Notifications::Show("Tenebris", Localization::IsPortuguese() ? "Modelo de PED inválido." : "Invalid PED model.", NotificationType::Error);
				return;
			}

			FiberPool::Push([model, display = entry.Display] {
				STREAMING::REQUEST_MODEL(model, false);
				for (int attempt = 0; attempt < 240 && !STREAMING::HAS_MODEL_LOADED(model); ++attempt)
					ScriptMgr::Yield();
				if (!STREAMING::HAS_MODEL_LOADED(model))
				{
					STREAMING::SET_MODEL_AS_NO_LONGER_NEEDED(model);
					Notifications::Show("Tenebris", Localization::IsPortuguese() ? "O modelo do PED não carregou a tempo." : "The PED model did not load in time.", NotificationType::Error);
					return;
				}

				auto coords = Self::GetPed().GetPosition();
				coords.x += 2.0f;
				auto ped = Ped::Create(model, coords);
				STREAMING::SET_MODEL_AS_NO_LONGER_NEEDED(model);
				if (!ped)
				{
					Notifications::Show("Tenebris", Localization::IsPortuguese() ? "Não foi possível criar o PED selecionado." : "Could not spawn the selected PED.", NotificationType::Error);
					return;
				}
				Notifications::Show("Tenebris",
				    Localization::IsPortuguese() ? std::format("PED criado: {}", display) : std::format("PED spawned: {}", display),
				    NotificationType::Success,
				    2600);
			});
		}

		class PedListSpawnerItem final : public UIItem
		{
		public:
			explicit PedListSpawnerItem(PedListKind kind) : m_Kind(kind)
			{
			}

			void Draw() override
			{
				const auto& entries = GetEntries(m_Kind);
				if (entries.empty())
				{
					ImGui::TextDisabled("%s", Localization::IsPortuguese() ? "Nenhum modelo encontrado." : "No models found.");
					return;
				}
				m_Selected = std::clamp(m_Selected, 0, static_cast<int>(entries.size()) - 1);

				ImGui::TextDisabled("%s", Localization::IsPortuguese()
				        ? "Selecione um modelo e pressione Enter ou o botão Criar."
				        : "Select a model and press Enter or the Spawn button.");
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::InputTextWithHint("##ped_filter", Localization::IsPortuguese() ? "Pesquisar (opcional)" : "Search (optional)", m_Filter, sizeof(m_Filter));
				ImGui::TextDisabled(Localization::IsPortuguese() ? "%zu modelos conhecidos" : "%zu known models", entries.size());
				ImGui::Separator();

				const std::string filter = Lower(m_Filter);
				if (ImGui::BeginChild("##ped_list", ImVec2(0.0f, 430.0f), true))
				{
					for (std::size_t i = 0; i < entries.size(); ++i)
					{
						const auto& entry = entries[i];
						if (!filter.empty() && Lower(entry.Display + " " + entry.Model).find(filter) == std::string::npos)
							continue;
						const std::string row = entry.Display + "##" + entry.Model;
						if (ImGui::Selectable(row.c_str(), m_Selected == static_cast<int>(i)))
							m_Selected = static_cast<int>(i);
						if (m_Selected == static_cast<int>(i) && m_ScrollSelected)
						{
							ImGui::SetScrollHereY(0.5f);
							m_ScrollSelected = false;
						}
					}
				}
				ImGui::EndChild();

				ImGui::TextDisabled("%s", entries[m_Selected].Model.c_str());
				if (ImGui::Button(Localization::IsPortuguese() ? "Criar" : "Spawn", ImVec2(-FLT_MIN, 0.0f)))
					SpawnPed(entries[m_Selected]);
			}

			std::string_view GetMenuLabel() const override
			{
				return m_Kind == PedListKind::Human ? "Criar Ped Humanos" : "Criar PED Animais";
			}

			std::string_view GetMenuDescription() const override
			{
				return m_Kind == PedListKind::Human
				    ? "Mostra os modelos humanos conhecidos do jogo para selecionar e criar rapidamente."
				    : "Mostra os modelos de animais conhecidos do jogo para selecionar e criar rapidamente.";
			}

			bool RequiresImGuiEditor() const override { return true; }
			float GetPreferredEditorHeight() const override { return 620.0f; }

			bool HandleEditorKey(int key) override
			{
				const auto& entries = GetEntries(m_Kind);
				if (entries.empty())
					return false;
				if (key == VK_UP)
				{
					m_Selected = m_Selected <= 0 ? static_cast<int>(entries.size()) - 1 : m_Selected - 1;
					m_ScrollSelected = true;
					return true;
				}
				if (key == VK_DOWN)
				{
					m_Selected = (m_Selected + 1) % static_cast<int>(entries.size());
					m_ScrollSelected = true;
					return true;
				}
				if (key == VK_RETURN)
				{
					SpawnPed(entries[std::clamp(m_Selected, 0, static_cast<int>(entries.size()) - 1)]);
					return true;
				}
				return false;
			}

		private:
			PedListKind m_Kind;
			int m_Selected = 0;
			bool m_ScrollSelected = false;
			char m_Filter[96]{};
		};
	}

	std::shared_ptr<UIItem> CreateHumanPedSpawnerItem()
	{
		return std::make_shared<PedListSpawnerItem>(PedListKind::Human);
	}

	std::shared_ptr<UIItem> CreateAnimalPedSpawnerItem()
	{
		return std::make_shared<PedListSpawnerItem>(PedListKind::Animal);
	}
}
