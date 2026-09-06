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
#include "util/Joaat.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <format>
#include <string>
#include <string_view>
#include <utility>
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
			bool Story{};
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
			if (lower.starts_with("a_c_") || lower.starts_with("mp_a_c_") || lower.starts_with("p_c_horse_"))
				return true;

			// Alguns animais de cutscene usam CS_ em vez de A_C_.
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

		bool IsStoryCharacterModel(std::string_view model)
		{
			const std::string lower = Lower(model);
			return lower == "player_zero" || lower == "player_three" || lower.starts_with("cs_");
		}

		std::string HumanDisplayName(std::string_view model)
		{
			const std::string lower = Lower(model);

			// IDs confirmados na própria base de modelos/categorias do projeto.
			static constexpr std::array<std::pair<std::string_view, std::string_view>, 33> namedStoryPeds{{
			    {"player_zero", "Arthur Morgan"},
			    {"player_three", "John Marston"},
			    {"cs_johnmarston", "John Marston"},
			    {"cs_abigailroberts", "Abigail Roberts"},
			    {"cs_dutch", "Dutch van der Linde"},
			    {"cs_hoseamatthews", "Hosea Matthews"},
			    {"cs_javierescuella", "Javier Escuella"},
			    {"cs_micahbell", "Micah Bell"},
			    {"cs_billwilliamson", "Bill Williamson"},
			    {"cs_lenny", "Lenny Summers"},
			    {"cs_sean", "Sean MacGuire"},
			    {"cs_kieran", "Kieran Duffy"},
			    {"cs_uncle", "Uncle"},
			    {"cs_jackmarston", "Jack Marston"},
			    {"cs_jackmarston_teen", "Jack Marston (Teen)"},
			    {"cs_mollyoshea", "Molly O'Shea"},
			    {"cs_susangrimshaw", "Susan Grimshaw"},
			    {"cs_karen", "Karen Jones"},
			    {"cs_marybeth", "Mary-Beth Gaskill"},
			    {"cs_tilly", "Tilly Jackson"},
			    {"cs_revswanson", "Reverend Orville Swanson"},
			    {"cs_leostrauss", "Leopold Strauss"},
			    {"cs_josiahtrelawny", "Josiah Trelawny"},
			    {"cs_mrsadler", "Sadie Adler"},
			    {"cs_charlessmith", "Charles Smith"},
			    {"cs_mrpearson", "Simon Pearson"},
			    {"cs_eagleflies", "Eagle Flies"},
			    {"cs_rainsfall", "Rains Fall"},
			    {"cs_edgarross", "Edgar Ross"},
			    {"cs_miltonandrews", "Andrew Milton"},
			    {"cs_leviticuscornwall", "Leviticus Cornwall"},
			    {"cs_bronte", "Angelo Bronte"},
			    {"cs_sistercalderon", "Sister Calderón"},
			}};

			for (const auto& [modelName, displayName] : namedStoryPeds)
				if (lower == modelName)
					return std::string(displayName);

			// Para os demais humanos, remove prefixos técnicos e deixa o papel/nome legível.
			std::string_view clean = model;
			static constexpr std::array<std::string_view, 27> prefixes = {
			    "MP_U_M_M_", "MP_U_F_M_", "MP_A_M_M_", "MP_A_F_M_",
			    "CS_", "U_M_M_", "U_F_M_", "U_M_O_", "U_F_O_", "U_M_Y_", "U_F_Y_",
			    "A_M_M_", "A_F_M_", "A_M_O_", "A_F_O_", "A_M_Y_", "A_F_Y_",
			    "S_M_M_", "S_F_M_", "S_M_Y_", "S_F_Y_", "G_M_M_", "G_F_M_",
			    "P_U_M_M_", "P_U_F_M_", "P_M_M_", "P_F_M_"};
			for (const auto prefix : prefixes)
			{
				if (clean.size() >= prefix.size() && Lower(clean.substr(0, prefix.size())) == Lower(prefix))
				{
					clean.remove_prefix(prefix.size());
					break;
				}
			}

			std::string display = Data::GetPedDisplayName(clean);
			return display.empty() ? std::string(model) : display;
		}

		void ConfigureHumanKnifeCombat(Ped& ped)
		{
			if (!ped)
				return;

			const int pedHandle = ped.GetHandle();
			const int playerPed = Self::GetPed().GetHandle();
			if (!ENTITY::DOES_ENTITY_EXIST(pedHandle) || !ENTITY::DOES_ENTITY_EXIST(playerPed))
				return;

			const Hash knife = Joaat("WEAPON_MELEE_KNIFE");
			WEAPON::REMOVE_ALL_PED_WEAPONS(pedHandle, true, true);
			WEAPON::GIVE_WEAPON_TO_PED(pedHandle, knife, 1, true, true, 0, false, 0.5f, 1.0f, 1.0f, false, 0, false);
			WEAPON::SET_CURRENT_PED_WEAPON(pedHandle, knife, true, 0, false, false);

			PED::SET_PED_COMBAT_ABILITY(pedHandle, 3);
			PED::SET_PED_COMBAT_MOVEMENT(pedHandle, 3);
			PED::SET_PED_COMBAT_RANGE(pedHandle, 0);
			PED::SET_PED_COMBAT_ATTRIBUTES(pedHandle, 5, true);
			PED::SET_PED_COMBAT_ATTRIBUTES(pedHandle, 46, true);
			PED::SET_PED_FLEE_ATTRIBUTES(pedHandle, 0, false);
			PED::SET_PED_SEEING_RANGE(pedHandle, 500.0f);
			PED::SET_PED_HEARING_RANGE(pedHandle, 500.0f);
			PED::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(pedHandle, false);

			// Alvo exclusivo: personagem local. Não cria hostilidade genérica contra outros jogadores.
			TASK::TASK_COMBAT_PED(pedHandle, playerPed, 0, 16);
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
				entry.Story = !animal && IsStoryCharacterModel(model);
				entry.Display = animal ? Data::GetPedDisplayName(model) : HumanDisplayName(model);
				if (entry.Display.empty())
					entry.Display = entry.Model;
				entries.push_back(std::move(entry));
			}
			std::sort(entries.begin(), entries.end(), [](const PedEntry& a, const PedEntry& b) {
				if (a.Story != b.Story)
					return a.Story > b.Story;
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

			const bool human = !IsAnimalModel(entry.Model);
			FiberPool::Push([model, display = entry.Display, human] {
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

				if (human)
					ConfigureHumanKnifeCombat(ped);

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

				ImGui::TextDisabled("%s", Localization::IsPortuguese()
				        ? "Selecione um modelo e pressione Enter ou o botão Criar."
				        : "Select a model and press Enter or the Spawn button.");
				if (m_Kind == PedListKind::Human)
					ImGui::TextDisabled("%s", Localization::IsPortuguese()
					        ? "Personagens de história aparecem primeiro. Humanos criados atacam seu personagem automaticamente com uma faca."
					        : "Story characters appear first. Spawned humans automatically attack your character with a knife.");
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::InputTextWithHint("##ped_filter", Localization::IsPortuguese() ? "Pesquisar nome ou modelo" : "Search name or model", m_Filter, sizeof(m_Filter));

				const auto visibleIndices = BuildVisibleIndices(entries);
				ImGui::TextDisabled(Localization::IsPortuguese() ? "%zu de %zu modelos" : "%zu of %zu models", visibleIndices.size(), entries.size());
				ImGui::Separator();

				if (!visibleIndices.empty() && std::find(visibleIndices.begin(), visibleIndices.end(), m_Selected) == visibleIndices.end())
					m_Selected = visibleIndices.front();

				if (ImGui::BeginChild("##ped_list", ImVec2(0.0f, 430.0f), true))
				{
					if (visibleIndices.empty())
					{
						ImGui::TextDisabled("%s", Localization::IsPortuguese() ? "Nenhum resultado para este filtro." : "No results for this filter.");
					}
					else
					{
						for (const int index : visibleIndices)
						{
							const auto& entry = entries[static_cast<std::size_t>(index)];
							const std::string visible = entry.Story ? entry.Display + (Localization::IsPortuguese() ? "  [História]" : "  [Story]") : entry.Display;
							const std::string row = visible + "##" + entry.Model;
							if (ImGui::Selectable(row.c_str(), m_Selected == index))
								m_Selected = index;
							if (m_Selected == index && m_ScrollSelected)
							{
								ImGui::SetScrollHereY(0.5f);
								m_ScrollSelected = false;
							}
						}
					}
				}
				ImGui::EndChild();

				if (visibleIndices.empty())
					return;

				m_Selected = std::clamp(m_Selected, 0, static_cast<int>(entries.size()) - 1);
				ImGui::Text("%s: %s", Localization::IsPortuguese() ? "Nome" : "Name", entries[static_cast<std::size_t>(m_Selected)].Display.c_str());
				ImGui::TextDisabled("Model: %s", entries[static_cast<std::size_t>(m_Selected)].Model.c_str());
				if (ImGui::Button(Localization::IsPortuguese() ? "Criar" : "Spawn", ImVec2(-FLT_MIN, 0.0f)))
					SpawnPed(entries[static_cast<std::size_t>(m_Selected)]);
			}

			std::string_view GetMenuLabel() const override
			{
				return m_Kind == PedListKind::Human ? "Criar Ped Humanos" : "Criar PED Animais";
			}

			std::string_view GetMenuDescription() const override
			{
				return m_Kind == PedListKind::Human
				    ? "Lista humanos com nomes legíveis, incluindo personagens do modo história como Arthur e John."
				    : "Mostra os modelos de animais conhecidos do jogo para selecionar e criar rapidamente.";
			}

			bool RequiresImGuiEditor() const override { return true; }
			float GetPreferredEditorHeight() const override { return 660.0f; }

			bool HandleEditorKey(int key) override
			{
				const auto& entries = GetEntries(m_Kind);
				const auto visibleIndices = BuildVisibleIndices(entries);
				if (visibleIndices.empty())
					return false;

				auto current = std::find(visibleIndices.begin(), visibleIndices.end(), m_Selected);
				std::size_t position = current == visibleIndices.end() ? 0 : static_cast<std::size_t>(std::distance(visibleIndices.begin(), current));

				if (key == VK_UP)
				{
					position = position == 0 ? visibleIndices.size() - 1 : position - 1;
					m_Selected = visibleIndices[position];
					m_ScrollSelected = true;
					return true;
				}
				if (key == VK_DOWN)
				{
					position = (position + 1) % visibleIndices.size();
					m_Selected = visibleIndices[position];
					m_ScrollSelected = true;
					return true;
				}
				if (key == VK_RETURN)
				{
					m_Selected = visibleIndices[position];
					SpawnPed(entries[static_cast<std::size_t>(m_Selected)]);
					return true;
				}
				return false;
			}

		private:
			std::vector<int> BuildVisibleIndices(const std::vector<PedEntry>& entries) const
			{
				std::vector<int> visible;
				visible.reserve(entries.size());
				const std::string filter = Lower(m_Filter);
				for (std::size_t i = 0; i < entries.size(); ++i)
				{
					const auto& entry = entries[i];
					if (!filter.empty() && Lower(entry.Display + " " + entry.Model).find(filter) == std::string::npos)
						continue;
					visible.push_back(static_cast<int>(i));
				}
				return visible;
			}

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
