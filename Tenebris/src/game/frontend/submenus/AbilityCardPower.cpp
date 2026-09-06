#include "AbilityCardPower.hpp"

#include "core/frontend/Localization.hpp"
#include "core/frontend/manager/Category.hpp"
#include "core/frontend/manager/Submenu.hpp"
#include "core/frontend/manager/UIItem.hpp"
#include "game/pointers/Pointers.hpp"
#include "game/rdr/Enums.hpp"
#include "game/rdr/Natives.hpp"
#include "game/rdr/Player.hpp"
#include "game/rdr/ScriptGlobal.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace YimMenu
{
	namespace
	{
		struct CardNameEntry
		{
			AbilityType Type;
			const char* Pt;
			const char* En;
		};

		constexpr std::array<CardNameEntry, 33> kCardNames{{
		    {AbilityType::A_MOMENT_TO_RECUPERATE, "Um Momento para se Recuperar", "A Moment to Recuperate"},
		    {AbilityType::FOCUS_FIRE, "Fogo Concentrado", "Focus Fire"},
		    {AbilityType::PAINT_IT_BLACK, "Pinte de Preto", "Paint It Black"},
		    {AbilityType::SLOW_AND_STEADY, "Lento e Firme", "Slow and Steady"},
		    {AbilityType::QUITE_AN_INSPIRATION, "Uma Grande Inspiração", "Quite an Inspiration"},
		    {AbilityType::SLIPPERY_BASTARD, "Bastardo Escorregadio", "Slippery Bastard"},
		    {AbilityType::GUNSLINGERS_CHOICE, "Escolha do Pistoleiro", "Gunslinger's Choice"},
		    {AbilityType::HORSEMAN, "Cavaleiro", "Horseman"},
		    {AbilityType::SHARPSHOOTER, "Atirador de Elite", "Sharpshooter"},
		    {AbilityType::NECESSITY_BREEDS, "A Necessidade Cria", "Necessity Breeds"},
		    {AbilityType::LANDONS_PATIENCE, "Paciência de Landon", "Landon's Patience"},
		    {AbilityType::THE_SHORT_GAME, "Jogo Curto", "The Short Game"},
		    {AbilityType::HANGMAN, "Carrasco", "Hangman"},
		    {AbilityType::WINNING_STREAK, "Sequência Vencedora", "Winning Streak"},
		    {AbilityType::IRON_LUNG, "Pulmão de Ferro", "Iron Lung"},
		    {AbilityType::KICK_IN_THE_BUTT, "Chute no Traseiro", "Kick in the Butt"},
		    {AbilityType::LIVE_FOR_THE_FIGHT, "Viva para a Luta", "Live for the Fight"},
		    {AbilityType::RIDE_LIKE_THE_WIND, "Cavalgue como o Vento", "Ride Like the Wind"},
		    {AbilityType::COME_BACK_STRONGER, "Volte Mais Forte", "Come Back Stronger"},
		    {AbilityType::PEAK_CONDITION, "Condição Máxima", "Peak Condition"},
		    {AbilityType::EYE_FOR_AN_EYE, "Olho por Olho", "Eye for an Eye"},
		    {AbilityType::THE_GIFT_OF_FOCUS, "Dom da Concentração", "The Gift of Focus"},
		    {AbilityType::STRANGE_MEDICINE, "Remédio Estranho", "Strange Medicine"},
		    {AbilityType::COLD_BLOODED, "Sangue Frio", "Cold Blooded"},
		    {AbilityType::FOOL_ME_ONCE, "Engane-me Uma Vez", "Fool Me Once"},
		    {AbilityType::FRIENDS_FOR_LIFE, "Amigos para a Vida", "Friends for Life"},
		    {AbilityType::STRENGTH_IN_NUMBERS, "Força em Números", "Strength in Numbers"},
		    {AbilityType::HUNKER_DOWN, "Entrincheirado", "Hunker Down"},
		    {AbilityType::TO_FIGHT_ANOTHER_DAY, "Para Lutar Outro Dia", "To Fight Another Day"},
		    {AbilityType::THE_UNBLINKING_EYE, "Olho Imperturbável", "The Unblinking Eye"},
		    {AbilityType::TAKE_THE_PAIN_AWAY, "Leve a Dor Embora", "Take the Pain Away"},
		    {AbilityType::OF_SINGLE_PURPOSE, "De Propósito Único", "Of Single Purpose"},
		    {AbilityType::NEVER_WITHOUT_ONE, "Cabeça Coberta", "Never Without One"},
		}};

		std::array<std::atomic_bool, 4> g_TierOverrideEnabled{};
		std::array<std::atomic_bool, 4> g_TierRestorePending{};
		std::array<std::atomic<int>, 4> g_OverrideTier{{2, 2, 2, 2}};
		std::array<std::atomic<int>, 4> g_SavedGameTier{{-1, -1, -1, -1}};
		std::array<std::atomic<int>, 4> g_ObservedTier{{-1, -1, -1, -1}};
		std::array<std::atomic<std::uint32_t>, 4> g_ObservedType{};

		const char* SlotName(std::size_t slot)
		{
			if (Localization::IsPortuguese())
			{
				static constexpr const char* labels[] = {"Olho da Morte", "Passiva 1", "Passiva 2", "Passiva 3"};
				return labels[std::min<std::size_t>(slot, 3)];
			}
			static constexpr const char* labels[] = {"Dead Eye", "Passive 1", "Passive 2", "Passive 3"};
			return labels[std::min<std::size_t>(slot, 3)];
		}

		std::string CardName(std::uint32_t hash)
		{
			for (const auto& card : kCardNames)
			{
				if (static_cast<std::uint32_t>(card.Type) == hash)
					return Localization::IsPortuguese() ? card.Pt : card.En;
			}
			return hash == 0 ? (Localization::IsPortuguese() ? "Nenhuma / lendo" : "None / reading") : std::format("0x{:08X}", hash);
		}

		const char* TierName(int tier)
		{
			switch (tier)
			{
			case 0: return "I";
			case 1: return "II";
			case 2: return "III";
			default: return "?";
			}
		}

		void EnableTierOverride(std::size_t slot, int tier)
		{
			if (slot >= 4)
				return;
			g_OverrideTier[slot].store(std::clamp(tier, 0, 2));
			g_TierRestorePending[slot].store(false);
			g_TierOverrideEnabled[slot].store(true);
		}

		void DisableTierOverride(std::size_t slot)
		{
			if (slot >= 4)
				return;
			g_TierOverrideEnabled[slot].store(false);
			g_TierRestorePending[slot].store(true);
		}

		class AbilityCardPowerItem final : public UIItem
		{
		public:
			void Draw() override
			{
				if (!Pointers.IsSessionStarted || !*Pointers.IsSessionStarted)
				{
					ImGui::TextWrapped("%s", Localization::IsPortuguese()
					    ? "Entre no Red Dead Online para ler e modificar o poder das cartas equipadas."
					    : "Enter Red Dead Online to read and modify the power of equipped cards.");
					return;
				}

				ImGui::TextWrapped("%s", Localization::IsPortuguese()
				    ? "O poder real das cartas é o Tier I/II/III. Esta opção sobrescreve o Tier em runtime somente nas quatro cartas que você está usando; não compra, evolui ou altera permanentemente a progressão."
				    : "A card's real power is its Tier I/II/III. This overrides the Tier at runtime only for the four cards you are using; it does not buy, upgrade or permanently alter progression.");
				ImGui::Separator();

				if (ImGui::Button(Localization::IsPortuguese() ? "MAXIMIZAR TODAS - TIER III" : "MAX ALL - TIER III"))
				{
					for (std::size_t slot = 0; slot < 4; ++slot)
						EnableTierOverride(slot, 2);
				}
				ImGui::SameLine();
				if (ImGui::Button(Localization::IsPortuguese() ? "Restaurar todas" : "Restore all"))
				{
					for (std::size_t slot = 0; slot < 4; ++slot)
						DisableTierOverride(slot);
				}

				for (std::size_t slot = 0; slot < 4; ++slot)
				{
					ImGui::PushID(static_cast<int>(slot));
					ImGui::SeparatorText(SlotName(slot));

					const auto type = g_ObservedType[slot].load();
					const int observedTier = g_ObservedTier[slot].load();
					const std::string name = CardName(type);
					ImGui::TextWrapped("%s: %s", Localization::IsPortuguese() ? "Carta equipada" : "Equipped card", name.c_str());
					ImGui::Text("%s: %s", Localization::IsPortuguese() ? "Tier efetivo" : "Effective tier", TierName(observedTier));

					bool enabled = g_TierOverrideEnabled[slot].load();
					if (ImGui::Checkbox(Localization::IsPortuguese() ? "Sobrescrever poder" : "Override power", &enabled))
					{
						if (enabled)
							EnableTierOverride(slot, g_OverrideTier[slot].load());
						else
							DisableTierOverride(slot);
					}

					int tier = std::clamp(g_OverrideTier[slot].load(), 0, 2);
					const char* tiers[] = {"Tier I", "Tier II", "Tier III (MAX)"};
					ImGui::SetNextItemWidth(220.0f);
					if (ImGui::Combo(Localization::IsPortuguese() ? "Poder" : "Power", &tier, tiers, 3))
						EnableTierOverride(slot, tier);

					if (ImGui::Button(Localization::IsPortuguese() ? "Maximizar esta carta" : "Max this card"))
						EnableTierOverride(slot, 2);
					ImGui::PopID();
				}
			}

			std::string_view GetMenuLabel() const override
			{
				return Localization::IsPortuguese() ? "Poder das cartas equipadas" : "Equipped card power";
			}

			std::string_view GetMenuDescription() const override
			{
				return Localization::IsPortuguese()
				    ? "Modifica em runtime o Tier I/II/III das quatro cartas que estão equipadas no momento."
				    : "Runtime Tier I/II/III control for the four cards currently equipped.";
			}

			bool RequiresImGuiEditor() const override { return true; }
			float GetPreferredEditorHeight() const override { return 760.0f; }
		};
	}

	namespace AbilityCardPower
	{
		void Tick()
		{
			if (!g_Running || !Pointers.IsSessionStarted || !*Pointers.IsSessionStarted)
				return;

			constexpr auto persona = ScriptGlobal(1155150);
			if (!persona.CanAccess(true))
				return;

			const int playerId = PLAYER::PLAYER_ID();
			if (playerId < 0 || playerId >= 32)
				return;

			auto* data = persona.As<PLAYER_PERSONA_DATA*>();
			if (!data)
				return;

			for (std::size_t slot = 0; slot < 4; ++slot)
			{
				auto& equipped = data->Entries[playerId].Abilities[static_cast<int>(slot)];
				g_ObservedType[slot].store(static_cast<std::uint32_t>(equipped.Type));

				if (g_TierRestorePending[slot].exchange(false))
				{
					const int saved = g_SavedGameTier[slot].exchange(-1);
					if (saved >= 0 && saved <= 2)
						equipped.Tier = saved;
					g_ObservedTier[slot].store(equipped.Tier);
					continue;
				}

				if (g_TierOverrideEnabled[slot].load())
				{
					const int current = equipped.Tier;
					if (g_SavedGameTier[slot].load() < 0 && current >= 0 && current <= 2)
						g_SavedGameTier[slot].store(current);

					const int desired = std::clamp(g_OverrideTier[slot].load(), 0, 2);
					equipped.Tier = desired;
					g_ObservedTier[slot].store(desired);
				}
				else
				{
					g_ObservedTier[slot].store(equipped.Tier);
					if (equipped.Tier >= 0 && equipped.Tier <= 2)
						g_SavedGameTier[slot].store(equipped.Tier);
				}
			}
		}
	}

	namespace Submenus
	{
		void InstallAbilityCardPower(const std::shared_ptr<Submenu>& selfSubmenu)
		{
			if (!selfSubmenu)
				return;

			for (auto& category : selfSubmenu->m_Categories)
			{
				if (category && category->m_Name == "Cartas de habilidade")
				{
					category->AddItem(std::make_shared<AbilityCardPowerItem>());
					return;
				}
			}

			auto category = std::make_shared<Category>("Cartas de habilidade");
			category->AddItem(std::make_shared<AbilityCardPowerItem>());
			selfSubmenu->AddCategory(std::move(category));
		}
	}
}
