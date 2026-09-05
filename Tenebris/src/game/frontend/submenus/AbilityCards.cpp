#include "AbilityCards.hpp"

#include "core/frontend/Localization.hpp"
#include "core/frontend/manager/Category.hpp"
#include "core/frontend/manager/Submenu.hpp"
#include "core/frontend/manager/UIItem.hpp"
#include "game/backend/Self.hpp"
#include "game/pointers/Pointers.hpp"
#include "game/rdr/Enums.hpp"
#include "game/rdr/Natives.hpp"
#include "game/rdr/Player.hpp"
#include "game/rdr/ScriptGlobal.hpp"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace YimMenu
{
	namespace
	{
		struct AbilityCardDefinition
		{
			AbilityType Type;
			bool DeadEye;
			const char* NamePt;
			const char* NameEn;
			const char* MechanicsPt;
			const char* MechanicsEn;
		};

		// Public RDO cards only. The internal OVERRIDE_REVENGE_SLOW_TIME entry is
		// intentionally not exposed as a selectable card.
		constexpr std::array<AbilityCardDefinition, 33> kCards{{
		    {AbilityType::A_MOMENT_TO_RECUPERATE, true, "Um Momento para se Recuperar", "A Moment to Recuperate",
		        "Regenera vida durante o Olho da Morte; normalmente receber dano interrompe o efeito.",
		        "Regenerates health during Dead Eye; taking damage normally interrupts the effect."},
		    {AbilityType::FOCUS_FIRE, true, "Fogo Concentrado", "Focus Fire",
		        "Aumenta o dano causado por você e aliados enquanto o Olho da Morte está ativo; o bônus não acumula com outra cópia.",
		        "Increases damage dealt by you and allies while Dead Eye is active; the bonus does not stack with another copy."},
		    {AbilityType::PAINT_IT_BLACK, true, "Pinte de Preto", "Paint It Black",
		        "Permite marcar alvos durante o Olho da Morte; os disparos marcados consomem Olho da Morte.",
		        "Allows targets to be painted during Dead Eye; marked shots consume Dead Eye."},
		    {AbilityType::SLOW_AND_STEADY, true, "Lento e Firme", "Slow and Steady",
		        "Reduz o dano recebido e impede morte imediata por tiro na cabeça durante o Olho da Morte, mas limita corrida e sprint.",
		        "Reduces incoming damage and prevents an outright headshot death during Dead Eye, but restricts running and sprinting."},
		    {AbilityType::QUITE_AN_INSPIRATION, true, "Uma Grande Inspiração", "Quite an Inspiration",
		        "Regenera a vida de você e aliados próximos enquanto o Olho da Morte está ativo; efeitos iguais não se acumulam.",
		        "Regenerates health for you and nearby allies while Dead Eye is active; identical effects do not stack."},
		    {AbilityType::SLIPPERY_BASTARD, true, "Bastardo Escorregadio", "Slippery Bastard",
		        "Altera fortemente precisão e travamento de mira enquanto o Olho da Morte está ativo e aumenta a drenagem da barra.",
		        "Strongly changes accuracy and lock-on behavior while Dead Eye is active and increases Dead Eye drain."},

		    {AbilityType::GUNSLINGERS_CHOICE, false, "Escolha do Pistoleiro", "Gunslinger's Choice",
		        "Melhora dano e precisão ao usar duas armas curtas simultaneamente.",
		        "Improves damage and accuracy while dual-wielding sidearms."},
		    {AbilityType::HORSEMAN, false, "Cavaleiro", "Horseman",
		        "Aumenta o dano causado enquanto você está montado.",
		        "Increases damage dealt while mounted."},
		    {AbilityType::SHARPSHOOTER, false, "Atirador de Elite", "Sharpshooter",
		        "Ao mirar por uma luneta, aumenta o dano causado e reduz o dano recebido.",
		        "While aiming through a scope, increases damage dealt and reduces damage received."},
		    {AbilityType::NECESSITY_BREEDS, false, "A Necessidade Cria", "Necessity Breeds",
		        "O dano aumenta progressivamente conforme sua vida se aproxima do mínimo.",
		        "Damage progressively increases as your health approaches its minimum."},
		    {AbilityType::LANDONS_PATIENCE, false, "Paciência de Landon", "Landon's Patience",
		        "Esperar entre disparos acumula um bônus de dano até o limite temporal da carta.",
		        "Waiting between shots builds a damage bonus up to the card's time limit."},
		    {AbilityType::THE_SHORT_GAME, false, "Jogo Curto", "The Short Game",
		        "Aumenta o dano em curta distância, com penalidade quando o alvo está mais longe.",
		        "Increases close-range damage, with a penalty against more distant targets."},
		    {AbilityType::HANGMAN, false, "Carrasco", "Hangman",
		        "O laço causa dano contínuo ao manter um inimigo preso.",
		        "The lasso deals continuous damage while an enemy remains restrained."},
		    {AbilityType::WINNING_STREAK, false, "Sequência Vencedora", "Winning Streak",
		        "Acertos consecutivos no mesmo alvo aumentam o dano; trocar ou perder a sequência encerra o acúmulo.",
		        "Consecutive hits on the same target increase damage; changing or losing the streak ends the buildup."},

		    {AbilityType::IRON_LUNG, false, "Pulmão de Ferro", "Iron Lung",
		        "Acelera a recuperação de vigor e fornece redução de dano relacionada ao nível de vigor.",
		        "Speeds stamina recovery and provides damage reduction related to current stamina."},
		    {AbilityType::KICK_IN_THE_BUTT, false, "Chute no Traseiro", "Kick in the Butt",
		        "Parte do dano recebido é convertida em recuperação de Olho da Morte.",
		        "A portion of damage received is converted into Dead Eye recovery."},
		    {AbilityType::LIVE_FOR_THE_FIGHT, false, "Viva para a Luta", "Live for the Fight",
		        "Regenera Olho da Morte passivamente ao longo do tempo.",
		        "Passively regenerates Dead Eye over time."},
		    {AbilityType::RIDE_LIKE_THE_WIND, false, "Cavalgue como o Vento", "Ride Like the Wind",
		        "Ao lutar montado, dano causado ou recebido recupera atributos do cavalo.",
		        "While mounted, damage dealt or received restores horse attributes."},
		    {AbilityType::COME_BACK_STRONGER, false, "Volte Mais Forte", "Come Back Stronger",
		        "Faz a regeneração natural de vida começar mais cedo depois de sofrer dano.",
		        "Makes natural health regeneration begin sooner after taking damage."},
		    {AbilityType::PEAK_CONDITION, false, "Condição Máxima", "Peak Condition",
		        "Concede bônus de dano quando o vigor está suficientemente alto.",
		        "Grants a damage bonus while stamina is sufficiently high."},
		    {AbilityType::EYE_FOR_AN_EYE, false, "Olho por Olho", "Eye for an Eye",
		        "Tiros na cabeça recuperam Olho da Morte.",
		        "Headshots restore Dead Eye."},
		    {AbilityType::THE_GIFT_OF_FOCUS, false, "Dom da Concentração", "The Gift of Focus",
		        "Fortalece efeitos que recuperam Olho da Morte, em troca de uma penalidade de dano.",
		        "Improves effects that restore Dead Eye in exchange for a damage penalty."},
		    {AbilityType::STRANGE_MEDICINE, false, "Remédio Estranho", "Strange Medicine",
		        "Causar dano recupera vida, mas a regeneração natural de vida fica mais lenta.",
		        "Dealing damage restores health, while natural health regeneration becomes slower."},
		    {AbilityType::COLD_BLOODED, false, "Sangue Frio", "Cold Blooded",
		        "Eliminar um inimigo recupera vida gradualmente durante alguns segundos.",
		        "Killing an enemy restores health gradually over the next few seconds."},

		    {AbilityType::FOOL_ME_ONCE, false, "Engane-me Uma Vez", "Fool Me Once",
		        "Acertos consecutivos recebidos reduzem progressivamente o dano; o efeito reinicia depois de um período sem ser atingido.",
		        "Consecutive hits received progressively reduce damage; the effect resets after a period without being hit."},
		    {AbilityType::FRIENDS_FOR_LIFE, false, "Amigos para a Vida", "Friends for Life",
		        "Reduz o dano recebido por você e sua montaria enquanto está montado.",
		        "Reduces damage received by you and your mount while mounted."},
		    {AbilityType::STRENGTH_IN_NUMBERS, false, "Força em Números", "Strength in Numbers",
		        "Reduz o dano recebido de acordo com a quantidade de aliados próximos, até o limite da carta.",
		        "Reduces incoming damage based on nearby allies, up to the card's limit."},
		    {AbilityType::HUNKER_DOWN, false, "Entrincheirado", "Hunker Down",
		        "Reduz o dano recebido enquanto você permanece em cobertura.",
		        "Reduces damage received while you remain in cover."},
		    {AbilityType::TO_FIGHT_ANOTHER_DAY, false, "Para Lutar Outro Dia", "To Fight Another Day",
		        "Reduz o dano de balas enquanto você está correndo em sprint.",
		        "Reduces bullet damage while you are sprinting."},
		    {AbilityType::THE_UNBLINKING_EYE, false, "Olho Imperturbável", "The Unblinking Eye",
		        "Faz as barras de Olho da Morte e Olho de Águia drenarem mais lentamente.",
		        "Makes Dead Eye and Eagle Eye drain more slowly."},
		    {AbilityType::TAKE_THE_PAIN_AWAY, false, "Leve a Dor Embora", "Take the Pain Away",
		        "Depois de reviver um aliado, ambos recebem redução temporária de dano.",
		        "After reviving an ally, both players receive temporary damage reduction."},
		    {AbilityType::OF_SINGLE_PURPOSE, false, "De Propósito Único", "Of Single Purpose",
		        "Reduz o dano de balas enquanto você está desarmado ou usando uma arma corpo a corpo.",
		        "Reduces bullet damage while unarmed or using a melee weapon."},
		    {AbilityType::NEVER_WITHOUT_ONE, false, "Cabeça Coberta", "Never Without One",
		        "O chapéu bloqueia um tiro na cabeça e cai. Enquanto estiver sem chapéu, o dano recebido aumenta; é um mecanismo de chapéu/cabeça, não um bônus percentual genérico.",
		        "Your hat blocks one headshot and falls off. While hatless, incoming damage increases; this is a hat/head mechanic, not a generic percentage bonus."},
		}};

		std::array<std::atomic<std::uint32_t>, 4> g_CurrentAbility{};
		std::array<std::atomic<int>, 4> g_CurrentTier{{-1, -1, -1, -1}};
		std::array<std::atomic<std::uint32_t>, 4> g_OverrideAbility{};
		std::array<std::atomic_bool, 4> g_OverrideEnabled{};

		std::atomic_bool g_PaintItBlackAutoTag{false};
		std::atomic_int g_PaintItBlackTargetMode{7}; // 6 enemies, 7 all, 8 animals
		std::atomic_bool g_MomentIgnoreDamageCancel{false};

		const AbilityCardDefinition* FindCard(std::uint32_t hash)
		{
			for (const auto& card : kCards)
				if (static_cast<std::uint32_t>(card.Type) == hash)
					return &card;
			return nullptr;
		}

		const char* CardName(const AbilityCardDefinition& card)
		{
			return Localization::IsPortuguese() ? card.NamePt : card.NameEn;
		}

		const char* CardMechanics(const AbilityCardDefinition& card)
		{
			return Localization::IsPortuguese() ? card.MechanicsPt : card.MechanicsEn;
		}

		std::string CardNameFromHash(std::uint32_t hash)
		{
			if (const auto* card = FindCard(hash))
				return CardName(*card);
			if (hash == 0)
				return Localization::IsPortuguese() ? "Nenhuma / lendo" : "None / reading";
			return std::format("0x{:08X}", hash);
		}

		bool IsCompatible(std::size_t slot, const AbilityCardDefinition& card)
		{
			return slot == 0 ? card.DeadEye : !card.DeadEye;
		}

		std::uint32_t EffectiveAbility(std::size_t slot)
		{
			if (slot >= g_CurrentAbility.size())
				return 0;
			if (g_OverrideEnabled[slot].load())
				return g_OverrideAbility[slot].load();
			return g_CurrentAbility[slot].load();
		}

		std::string TierLabel(int tier)
		{
			switch (tier)
			{
			case 0: return "I";
			case 1: return "II";
			case 2: return "III";
			default: return tier >= 0 ? std::to_string(tier + 1) : "?";
			}
		}

		void ApplyOverride(std::size_t slot, AbilityType type)
		{
			if (slot >= g_OverrideEnabled.size())
				return;
			const auto hash = static_cast<std::uint32_t>(type);
			const auto* card = FindCard(hash);
			if (!card || !IsCompatible(slot, *card))
				return;
			g_OverrideAbility[slot].store(hash);
			g_OverrideEnabled[slot].store(true);
			g_CurrentAbility[slot].store(hash);
		}

		void RestoreGameSlot(std::size_t slot)
		{
			if (slot >= g_OverrideEnabled.size())
				return;
			g_OverrideEnabled[slot].store(false);
		}

		void CycleCompatible(std::size_t slot, int delta)
		{
			std::array<const AbilityCardDefinition*, kCards.size()> compatible{};
			std::size_t count = 0;
			for (const auto& card : kCards)
				if (IsCompatible(slot, card))
					compatible[count++] = &card;
			if (count == 0)
				return;

			const auto current = EffectiveAbility(slot);
			std::size_t index = 0;
			for (std::size_t i = 0; i < count; ++i)
				if (static_cast<std::uint32_t>(compatible[i]->Type) == current)
				{
					index = i;
					break;
				}

			if (delta < 0)
				index = index == 0 ? count - 1 : index - 1;
			else
				index = (index + 1) % count;
			ApplyOverride(slot, compatible[index]->Type);
		}

		bool ContainsInsensitive(std::string_view text, std::string_view filter)
		{
			if (filter.empty())
				return true;
			std::string haystack(text);
			std::string needle(filter);
			std::transform(haystack.begin(), haystack.end(), haystack.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return haystack.find(needle) != std::string::npos;
		}

		void RenderVerifiedModifiers(std::uint32_t abilityHash)
		{
			ImGui::SeparatorText(Localization::IsPortuguese() ? "Parâmetros verificados" : "Verified parameters");

			if (abilityHash == static_cast<std::uint32_t>(AbilityType::PAINT_IT_BLACK))
			{
				bool autoTag = g_PaintItBlackAutoTag.load();
				if (ImGui::Checkbox(Localization::IsPortuguese() ? "Marcação automática" : "Automatic tagging", &autoTag))
					g_PaintItBlackAutoTag.store(autoTag);

				const char* modesPt[] = {"Inimigos", "Todos", "Animais"};
				const char* modesEn[] = {"Enemies", "All", "Animals"};
				int raw = g_PaintItBlackTargetMode.load();
				int index = raw == 6 ? 0 : raw == 8 ? 2 : 1;
				const char** modes = Localization::IsPortuguese() ? modesPt : modesEn;
				if (ImGui::Combo(Localization::IsPortuguese() ? "Alvos" : "Targets", &index, modes, 3))
					g_PaintItBlackTargetMode.store(index == 0 ? 6 : index == 2 ? 8 : 7);
				ImGui::TextWrapped("%s", Localization::IsPortuguese()
				    ? "Usa as natives de marcação de Dead Eye já presentes no Tenebris. Não altera o tier da carta."
				    : "Uses Dead Eye tagging natives already present in Tenebris. It does not alter the card tier.");
				return;
			}

			if (abilityHash == static_cast<std::uint32_t>(AbilityType::A_MOMENT_TO_RECUPERATE))
			{
				bool keepActive = g_MomentIgnoreDamageCancel.load();
				if (ImGui::Checkbox(Localization::IsPortuguese() ? "Não cancelar ao receber dano" : "Do not cancel when taking damage", &keepActive))
					g_MomentIgnoreDamageCancel.store(keepActive);
				ImGui::TextWrapped("%s", Localization::IsPortuguese()
				    ? "Controla diretamente a flag de persona usada pelo próprio jogo para sair do Dead Eye ao sofrer dano."
				    : "Directly controls the persona flag used by the game to exit Dead Eye after taking damage.");
				return;
			}

			if (abilityHash == static_cast<std::uint32_t>(AbilityType::NEVER_WITHOUT_ONE))
			{
				ImGui::TextWrapped("%s", Localization::IsPortuguese()
				    ? "Cabeça Coberta depende do estado do chapéu e do bloqueio do headshot. Não foi criado slider de porcentagem porque isso não representa o mecanismo real da carta."
				    : "Never Without One depends on hat state and headshot blocking. No percentage slider is exposed because that would not represent the card's real mechanism.");
				return;
			}

			ImGui::TextDisabled("%s", Localization::IsPortuguese()
			    ? "Nenhum parâmetro adicional foi exposto sem um mapeamento interno verificável."
			    : "No extra parameter is exposed without a verifiable internal mapping.");
		}

		class AbilitySlotItem final : public UIItem
		{
		public:
			explicit AbilitySlotItem(std::size_t slot) : m_Slot(slot)
			{
			}

			void Draw() override
			{
				const bool inSession = Pointers.IsSessionStarted && *Pointers.IsSessionStarted;
				if (!inSession)
				{
					ImGui::TextWrapped("%s", Localization::IsPortuguese()
					    ? "Entre no Red Dead Online para ler e substituir as cartas equipadas."
					    : "Enter Red Dead Online to read and replace equipped ability cards.");
					return;
				}

				const auto currentHash = EffectiveAbility(m_Slot);
				const int tier = g_CurrentTier[m_Slot].load();
				const std::string currentName = CardNameFromHash(currentHash);
				ImGui::Text("%s", SlotLabel());
				ImGui::TextWrapped("%s: %s  |  %s %s",
				    Localization::IsPortuguese() ? "Carta atual" : "Current card",
				    currentName.c_str(),
				    Localization::IsPortuguese() ? "Tier" : "Tier",
				    TierLabel(tier).c_str());

				if (g_OverrideEnabled[m_Slot].load())
				{
					ImGui::TextDisabled("%s", Localization::IsPortuguese()
					    ? "Substituição do Tenebris ativa. O valor de tier/progressão do jogo permanece intacto."
					    : "Tenebris runtime replacement is active. The game's tier/progression value remains untouched.");
					if (ImGui::Button(Localization::IsPortuguese() ? "Restaurar carta do jogo" : "Restore game card"))
						RestoreGameSlot(m_Slot);
				}
				else
				{
					ImGui::TextDisabled("%s", Localization::IsPortuguese()
					    ? "Usando o loadout real lido do jogador."
					    : "Using the player's real loadout value.");
				}

				ImGui::SeparatorText(Localization::IsPortuguese() ? "Escolher carta" : "Choose card");
				ImGui::InputTextWithHint("##AbilitySearch",
				    Localization::IsPortuguese() ? "Pesquisar carta..." : "Search card...",
				    m_Search,
				    sizeof(m_Search));
				ImGui::TextDisabled("%s", Localization::IsPortuguese()
				    ? "Q/E: carta anterior/seguinte e aplicar | BACK: sair"
				    : "Q/E: previous/next card and apply | BACK: exit");

				if (ImGui::BeginChild("##AbilityCardList", ImVec2(0.0f, 285.0f), true))
				{
					for (const auto& card : kCards)
					{
						if (!IsCompatible(m_Slot, card))
							continue;
						const std::string name = CardName(card);
						if (!ContainsInsensitive(name, m_Search))
							continue;
						const bool selected = static_cast<std::uint32_t>(card.Type) == currentHash;
						ImGui::PushID(static_cast<int>(static_cast<std::uint32_t>(card.Type)));
						if (ImGui::Selectable(name.c_str(), selected))
							ApplyOverride(m_Slot, card.Type);
						ImGui::PopID();
					}
				}
				ImGui::EndChild();

				const auto selectedHash = EffectiveAbility(m_Slot);
				if (const auto* selectedCard = FindCard(selectedHash))
				{
					ImGui::SeparatorText(Localization::IsPortuguese() ? "Mecânica da carta" : "Card mechanics");
					ImGui::TextWrapped("%s", CardMechanics(*selectedCard));
					RenderVerifiedModifiers(selectedHash);
				}
			}

			std::string_view GetMenuLabel() const override
			{
				return SlotLabel();
			}

			std::string GetMenuValue() const override
			{
				const auto hash = EffectiveAbility(m_Slot);
				const auto name = CardNameFromHash(hash);
				const int tier = g_CurrentTier[m_Slot].load();
				return tier >= 0 ? std::format("{} {}", name, TierLabel(tier)) : name;
			}

			std::string_view GetMenuDescription() const override
			{
				return Localization::IsPortuguese()
				    ? "Lê a carta equipada e permite uma substituição local em tempo de execução sem comprar, evoluir ou alterar o tier."
				    : "Reads the equipped card and allows a local runtime replacement without buying, upgrading, or altering its tier.";
			}

			bool RequiresImGuiEditor() const override
			{
				return true;
			}

			float GetPreferredEditorHeight() const override
			{
				return 690.0f;
			}

			bool HandleEditorKey(int key) override
			{
				if (key == 'Q')
				{
					CycleCompatible(m_Slot, -1);
					return true;
				}
				if (key == 'E')
				{
					CycleCompatible(m_Slot, 1);
					return true;
				}
				return false;
			}

		private:
			const char* SlotLabel() const
			{
				if (Localization::IsPortuguese())
				{
					static constexpr const char* labels[] = {"Olho da Morte", "Passiva 1", "Passiva 2", "Passiva 3"};
					return labels[std::min<std::size_t>(m_Slot, 3)];
				}
				static constexpr const char* labels[] = {"Dead Eye", "Passive 1", "Passive 2", "Passive 3"};
				return labels[std::min<std::size_t>(m_Slot, 3)];
			}

			std::size_t m_Slot{};
			char m_Search[96]{};
		};
	}

	namespace AbilityCards
	{
		void Tick()
		{
			static bool paintItBlackApplied = false;
			static bool momentFlagApplied = false;

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
				g_CurrentTier[slot].store(equipped.Tier);
				if (g_OverrideEnabled[slot].load())
				{
					const auto overrideHash = g_OverrideAbility[slot].load();
					if (FindCard(overrideHash))
						equipped.Type = static_cast<AbilityType>(overrideHash);
				}
				g_CurrentAbility[slot].store(static_cast<std::uint32_t>(equipped.Type));
			}

			const auto activeAbility = g_CurrentAbility[0].load();
			const bool deadEyeActive = PLAYER::_IS_SPECIAL_ABILITY_ACTIVE(playerId);

			const bool shouldAutoTag = deadEyeActive
			    && activeAbility == static_cast<std::uint32_t>(AbilityType::PAINT_IT_BLACK)
			    && g_PaintItBlackAutoTag.load();
			if (shouldAutoTag)
			{
				PLAYER::_SET_DEADEYE_TAGGING_CONFIG(playerId, g_PaintItBlackTargetMode.load());
				PLAYER::_SET_DEADEYE_TAGGING_ENABLED(playerId, true);
				paintItBlackApplied = true;
			}
			else if (paintItBlackApplied && activeAbility != static_cast<std::uint32_t>(AbilityType::PAINT_IT_BLACK))
			{
				PLAYER::_SET_DEADEYE_TAGGING_ENABLED(playerId, false);
				paintItBlackApplied = false;
			}

			const bool shouldIgnoreDamageCancel = deadEyeActive
			    && activeAbility == static_cast<std::uint32_t>(AbilityType::A_MOMENT_TO_RECUPERATE)
			    && g_MomentIgnoreDamageCancel.load();
			if (shouldIgnoreDamageCancel)
			{
				PLAYER::_SET_LOCAL_PLAYER_PERSONA_ABILITY_FLAG(ePersonaAbilityFlag::PERSONA_EXIT_DEADEYE_ON_TAKING_DAMAGE, false);
				momentFlagApplied = true;
			}
			else if (momentFlagApplied && activeAbility != static_cast<std::uint32_t>(AbilityType::A_MOMENT_TO_RECUPERATE))
			{
				PLAYER::_SET_LOCAL_PLAYER_PERSONA_ABILITY_FLAG(ePersonaAbilityFlag::PERSONA_EXIT_DEADEYE_ON_TAKING_DAMAGE, true);
				momentFlagApplied = false;
			}
		}
	}

	namespace Submenus
	{
		void InstallAbilityCards(const std::shared_ptr<Submenu>& selfSubmenu)
		{
			if (!selfSubmenu)
				return;

			auto category = std::make_shared<Category>("Cartas de habilidade");
			for (std::size_t slot = 0; slot < 4; ++slot)
				category->AddItem(std::make_shared<AbilitySlotItem>(slot));
			selfSubmenu->AddCategory(std::move(category));
		}
	}
}
