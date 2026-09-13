#include "Recovery.hpp"
#include "core/commands/BoolCommand.hpp"
#include "core/commands/Commands.hpp"
#include "core/frontend/Notifications.hpp"
#include "core/frontend/Localization.hpp"
#include "game/backend/FiberPool.hpp"
#include "game/frontend/items/Items.hpp"
#include "game/rdr/data/Stats.hpp"
#include "game/rdr/Natives.hpp"
#include "game/rdr/ScriptFunction.hpp"
#include "game/rdr/Scripts.hpp"
#include "util/Rewards.hpp"

namespace YimMenu::Submenus
{
	bool IsValid(joaat_t BaseId, joaat_t PermutationId)
	{
		StatId statid{};
		statid.BaseId = BaseId;
		statid.PermutationId = PermutationId;
		return STATS::STAT_ID_IS_VALID(&statid);
	}

	void SetInt(joaat_t BaseId, joaat_t PermutationId, int value)
	{
		StatId statid{};
		statid.BaseId = BaseId;
		statid.PermutationId = PermutationId;
		STATS::STAT_ID_SET_INT(&statid, value, TRUE);
	}

	void IncrementInt(joaat_t BaseId, joaat_t PermutationId, int value)
	{
		StatId statid{};
		statid.BaseId = BaseId;
		statid.PermutationId = PermutationId;
		STATS::_STAT_ID_INCREMENT_INT(&statid, value);
	}

	void SetBool(joaat_t BaseId, joaat_t PermutationId, bool value)
	{
		StatId statid{};
		statid.BaseId = BaseId;
		statid.PermutationId = PermutationId;
		STATS::STAT_ID_SET_BOOL(&statid, value, TRUE);
	}

	void SetFloat(joaat_t BaseId, joaat_t PermutationId, float value)
	{
		StatId statid{};
		statid.BaseId = BaseId;
		statid.PermutationId = PermutationId;
		STATS::STAT_ID_SET_FLOAT(&statid, value, TRUE);
	}

	void IncrementFloat(joaat_t BaseId, joaat_t PermutationId, float value)
	{
		StatId statid{};
		statid.BaseId = BaseId;
		statid.PermutationId = PermutationId;
		STATS::_STAT_ID_INCREMENT_FLOAT(&statid, value);
	}

	void SetDate(joaat_t BaseId, joaat_t PermutationId, Date* value)
	{
		StatId statid{};
		statid.BaseId = BaseId;
		statid.PermutationId = PermutationId;
		STATS::STAT_ID_SET_DATE(&statid, value, TRUE);
	}

	int GetInt(joaat_t BaseId, joaat_t PermutationId)
	{
		int value{};
		StatId statid{};
		statid.BaseId = BaseId;
		statid.PermutationId = PermutationId;
		STATS::STAT_ID_GET_INT(&statid, &value);
		return value;
	}

	bool GetBool(joaat_t BaseId, joaat_t PermutationId)
	{
		BOOL value{};
		StatId statid{};
		statid.BaseId = BaseId;
		statid.PermutationId = PermutationId;
		STATS::STAT_ID_GET_BOOL(&statid, &value);
		return value;
	}

	float GetFloat(joaat_t BaseId, joaat_t PermutationId)
	{
		float value{};
		StatId statid{};
		statid.BaseId = BaseId;
		statid.PermutationId = PermutationId;
		STATS::STAT_ID_GET_FLOAT(&statid, &value);
		return value;
	}

	Date GetDate(joaat_t BaseId, joaat_t PermutationId)
	{
		Date value{};
		StatId statid{};
		statid.BaseId = BaseId;
		statid.PermutationId = PermutationId;
		STATS::STAT_ID_GET_DATE(&statid, &value);
		return value;
	}

	Recovery::Recovery() :
	    Submenu::Submenu("Recuperação")
	{
		auto recovery               = std::make_shared<Category>("Recuperação");
		auto spawnCollectiblesGroup = std::make_shared<Group>("Gerar colecionáveis");
		auto spawnHerbsGroup        = std::make_shared<Group>("Gerar ervas");
		auto recoveryOptions        = std::make_shared<Group>("Opções");

		static auto recoveryCommand = Commands::GetCommand<BoolCommand>("recoveryenabled"_J);
		spawnCollectiblesGroup->AddItem(std::make_shared<ImGuiItem>([=] {
			if (recoveryCommand->GetState())
			{
				static Rewards::eRewardType selected{};
				std::map<Rewards::eRewardType, std::string> reward_translations = {
				    {Rewards::eRewardType::HEIRLOOMS, "Heirlooms"},
				    {Rewards::eRewardType::COINS, "Coins"},
				    {Rewards::eRewardType::ALCBOTTLES, "Alcohol Bottles"},
				    {Rewards::eRewardType::ARROWHEADS, "Arrowheads"},
				    {Rewards::eRewardType::BRACELETS, "Bracelets"},
				    {Rewards::eRewardType::EARRINGS, "Earrings"},
				    {Rewards::eRewardType::NECKLACES, "Necklaces"},
				    {Rewards::eRewardType::RINGS, "Rings"},
				    {Rewards::eRewardType::TAROTCARDS_CUPS, "Tarot Cards - Cups"},
				    {Rewards::eRewardType::TAROTCARDS_PENTACLES, "Tarot Cards - Pentacles"},
				    {Rewards::eRewardType::TAROTCARDS_SWORDS, "Tarot Cards - Swords"},
				    {Rewards::eRewardType::TAROTCARDS_WANDS, "Tarot Cards - Wands"},
				    {Rewards::eRewardType::FOSSILS, "Fossils"},
				    {Rewards::eRewardType::EGGS, "Eggs"},
				    {Rewards::eRewardType::FLOWERS, "Wild Flower Collection"},
				    {Rewards::eRewardType::TREASURE, "Treasure Reward"},
				    {Rewards::eRewardType::CAPITALE, "Capitale"},
				    {Rewards::eRewardType::XP, "25K XP"},
				    {Rewards::eRewardType::MOONSHINERXP, "200 Moonshiner XP"},
				    {Rewards::eRewardType::TRADERXP, "200 Trader XP"},
				    {Rewards::eRewardType::COLLECTORXP, "200 Collector XP"},
				    {Rewards::eRewardType::NATURALISTXP, "300 Naturalist XP"},
				    {Rewards::eRewardType::BOUNTYHUNTERXP, "200 Bounty Hunter XP"},
				    {Rewards::eRewardType::TRADERGOODS, "Max Trader Goods"},
				};

				if (ImGui::BeginCombo(Localization::IsPortuguese() ? "Recompensas" : "Rewards", reward_translations[selected].c_str()))
				{
					for (auto& [type, translation] : reward_translations)
					{
						if (ImGui::Selectable(std::string(translation).c_str(), type == selected, ImGuiSelectableFlags_AllowDoubleClick))
						{
							selected = type;
						}
						if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
						{
							FiberPool::Push([] {
								Rewards::GiveRequestedRewards({selected});
							});
						}
					}
					ImGui::EndCombo();
				}

				if (ImGui::Button(Localization::IsPortuguese() ? "Adicionar selecionado" : "Add Selected"))
				{
					FiberPool::Push([] {
						Rewards::GiveRequestedRewards({selected});
					});
				}
			}
			else
			{
				ImGui::Text("%s", Localization::IsPortuguese() ? "Recurso de recuperação restrito" : "Recovery Feature Restricted");
				ImGui::TextWrapped("%s", Localization::IsPortuguese()
				        ? "O recurso de recuperação e colecionáveis envolve risco de punição. Você é responsável pelo uso e por eventuais danos à sua conta."
				        : "The recovery and collectibles feature carries a risk of account penalties. You are responsible for its use and for any resulting account damage.");
				if (ImGui::Button(Localization::IsPortuguese() ? "Ativar recuperação" : "Enable Recovery"))
				{
					recoveryCommand->SetState(true);
				}
			}
		}, "Gerar colecionaveis", "Seleciona e entrega colecionaveis da categoria escolhida."));
		spawnHerbsGroup->AddItem(std::make_shared<ImGuiItem>([=] {
			if (recoveryCommand->GetState())
			{
				static joaat_t selectedHerb;
				std::map<joaat_t, std::string> herb_translations = {{"HERB_LOOT_ALASKAN_GINSENG"_J, "Alaskan Ginseng"},{"HERB_LOOT_AMERICAN_GINSENG"_J, "American Ginseng"},{"HERB_LOOT_BAY_BOLETE"_J, "Bay Bolete"},{"HERB_LOOT_BLACK_BERRY"_J, "Black Berry"},{"HERB_LOOT_BLACK_CURRANT"_J, "Black Currant"},{"HERB_LOOT_BURDOCK_ROOT"_J, "Burdock Root"},{"HERB_LOOT_CHANTERELLES"_J, "Chanterelles"},{"HERB_LOOT_COMMON_BULRUSH"_J, "Common Bulrush"},{"HERB_LOOT_CREEPING_THYME"_J, "Creeping Thyme"},{"HERB_LOOT_DESERT_SAGE"_J, "Desert Sage"},{"HERB_LOOT_ENGLISH_MACE"_J, "English Mace"},{"HERB_LOOT_EVERGREEN_HUCKLEBERRY"_J, "Evergreen Huckleberry"},{"HERB_LOOT_GOLDEN_CURRANT"_J, "Golden Currant"},{"HERB_LOOT_HUMMINGBIRD_SAGE"_J, "Hummingbird Sage"},{"HERB_LOOT_INDIAN_TOBACCO"_J, "Indian Tobacco"},{"HERB_LOOT_MILKWEED"_J, "Milkweed"},{"HERB_LOOT_OLEANDER_SAGE"_J, "Oleander Sage"},{"HERB_LOOT_OREGANO"_J, "Oregano"},{"HERB_LOOT_PARASOL_MUSHROOM"_J, "Parasol Mushroom"},{"HERB_LOOT_PRAIRIE_POPPY"_J, "Prairie Poppy"},{"HERB_LOOT_RAMS_HEAD"_J, "Rams Head"},{"HERB_LOOT_RED_RASPBERRY"_J, "Red Raspberry"},{"HERB_LOOT_RED_SAGE"_J, "Red Sage"},{"HERB_LOOT_VANILLA_FLOWER"_J, "Vanilla Flower"},{"HERB_LOOT_VIOLET_SNOWDROP"_J, "Violet Snowdrop"},{"HERB_LOOT_WILD_CARROTS"_J, "Wild Carrots"},{"HERB_LOOT_WILD_FEVERFEW"_J, "Wild Feverfew"},{"HERB_LOOT_WILD_MINT"_J, "Wild Mint"},{"HERB_LOOT_WINTERGREEN_BERRY"_J, "Wintergreen Berry"},{"HERB_LOOT_YARROW"_J, "Yarrow"},{"HERB_LOOT_AGARITA"_J, "Agarita"},{"HERB_LOOT_BITTERWEED"_J, "Bitterweed"},{"HERB_LOOT_BLUE_BONNET"_J, "Blue Bonnet"},{"HERB_LOOT_BLOOD_FLOWER"_J, "Blood Flower"},{"HERB_LOOT_CARDINAL_FLOWER"_J, "Cardinal Flower"},{"HERB_LOOT_CHOCOLATE_DAISY"_J, "Chocolate Daisy"},{"HERB_LOOT_CREEK_PLUM"_J, "Creek Plum"},{"HERB_LOOT_RHUBARB"_J, "Rhubarb"},{"HERB_LOOT_WISTERIA"_J, "Wisteria"},{"HERB_LOOT_HARRIETUM"_J, "Harrietum"},};
				if (ImGui::BeginCombo(Localization::IsPortuguese() ? "Ervas" : "Herbs", herb_translations[selectedHerb].c_str()))
				{
					for (auto& [herb, translation] : herb_translations)
					{
						if (ImGui::Selectable(std::string(translation).c_str(), herb == selectedHerb))
						{
							selectedHerb = herb;
						}
					}
					ImGui::EndCombo();
				}

				static int amount = 1;
				ImGui::SliderInt(Localization::IsPortuguese() ? "Quantidade" : "Amount", &amount, 1, 10);

				if (ImGui::Button(Localization::IsPortuguese() ? "Entregar selecionado" : "Give Selected"))
				{
					FiberPool::Push([] {
						if (!Scripts::RequestScript("interactive_campfire"_J))
							return;

						for (int i = 0; i < amount; i++)
							ScriptFunctions::GiveLootTableAward.StaticCall(selectedHerb, 0);
					});
				}
			}
		}, "Gerar ervas", "Seleciona e entrega ervas ao personagem."));
		recoveryOptions->AddItem(std::make_shared<BoolCommandItem>("unlimiteditems"_J));
		recovery->AddItem(spawnCollectiblesGroup);
		recovery->AddItem(spawnHerbsGroup);
		recovery->AddItem(recoveryOptions);

		auto dailiesGroup = std::make_shared<Group>("Desafios diários");
		dailiesGroup->AddItem(std::make_shared<CommandItem>("completedailies"_J));
		recovery->AddItem(dailiesGroup);

		AddCategory(std::move(recovery));
	}
}
