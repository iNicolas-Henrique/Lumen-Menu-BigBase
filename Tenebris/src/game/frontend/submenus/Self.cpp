#include "Self.hpp"

#include "core/commands/BoolCommand.hpp"
#include "core/commands/Commands.hpp"
#include "core/frontend/Localization.hpp"
#include "game/backend/AnimationDict.hpp"
#include "game/backend/FiberPool.hpp"
#include "game/backend/MusicDict.hpp"
#include "game/backend/ScriptMgr.hpp"
#include "game/backend/Self.hpp"
#include "game/features/Features.hpp"
#include "game/features/self/GiveAllFunctions.hpp"
#include "game/features/self/ScenarioPlayer.hpp"
#include "game/frontend/items/Items.hpp"
#include "game/pointers/Pointers.hpp"
#include "game/rdr/Natives.hpp"
#include "game/rdr/data/Emotes.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace YimMenu::Features
{
	BoolCommand _RecoveryEnabled("recoveryenabled", "Recovery Enabled", "Is the recovery feature enabled");
}

namespace YimMenu::Submenus
{
	namespace
	{
		std::vector<std::pair<std::string, std::string>> g_RecentAnimations;
		std::vector<std::string> g_RecentMusic;
		std::set<std::pair<std::string, std::string>> g_FavoriteAnimations;
		std::set<std::string> g_FavoriteMusic;
		bool g_LoopMusic{};
		bool g_MusicLoopRunning{};
		std::string g_LoopingMusicEvent;

		std::string DataPath(const std::string& filename)
		{
			const char* appData = std::getenv("APPDATA");
			std::filesystem::path base = appData ? std::filesystem::path(appData) / "Tenebris" : std::filesystem::path("Tenebris");
			std::filesystem::create_directories(base);
			return (base / filename).string();
		}

		void LoadAnimationHistory()
		{
			std::ifstream in(DataPath("animation_history.txt"));
			g_RecentAnimations.clear();
			std::string dict;
			std::string anim;
			while (std::getline(in, dict) && std::getline(in, anim))
			{
				if (!dict.empty() && !anim.empty())
					g_RecentAnimations.emplace_back(dict, anim);
				if (g_RecentAnimations.size() >= 10)
					break;
			}
		}

		void SaveAnimationHistory()
		{
			std::ofstream out(DataPath("animation_history.txt"), std::ios::trunc);
			for (const auto& [dict, anim] : g_RecentAnimations)
				out << dict << '\n' << anim << '\n';
		}

		void LoadAnimationFavorites()
		{
			std::ifstream in(DataPath("animation_favorites.txt"));
			g_FavoriteAnimations.clear();
			std::string dict;
			std::string anim;
			while (std::getline(in, dict) && std::getline(in, anim))
				if (!dict.empty() && !anim.empty())
					g_FavoriteAnimations.emplace(dict, anim);
		}

		void SaveAnimationFavorites()
		{
			std::ofstream out(DataPath("animation_favorites.txt"), std::ios::trunc);
			for (const auto& [dict, anim] : g_FavoriteAnimations)
				out << dict << '\n' << anim << '\n';
		}

		void LoadMusicHistory()
		{
			std::ifstream in(DataPath("music_history.txt"));
			g_RecentMusic.clear();
			std::string event;
			while (std::getline(in, event))
			{
				if (!event.empty())
					g_RecentMusic.push_back(event);
				if (g_RecentMusic.size() >= 10)
					break;
			}
		}

		void SaveMusicHistory()
		{
			std::ofstream out(DataPath("music_history.txt"), std::ios::trunc);
			for (const auto& event : g_RecentMusic)
				out << event << '\n';
		}

		void LoadMusicFavorites()
		{
			std::ifstream in(DataPath("music_favorites.txt"));
			g_FavoriteMusic.clear();
			std::string event;
			while (std::getline(in, event))
				if (!event.empty())
					g_FavoriteMusic.insert(event);
		}

		void SaveMusicFavorites()
		{
			std::ofstream out(DataPath("music_favorites.txt"), std::ios::trunc);
			for (const auto& event : g_FavoriteMusic)
				out << event << '\n';
		}

		void PushRecentAnimation(const std::string& dict, const std::string& anim)
		{
			const auto pair = std::make_pair(dict, anim);
			g_RecentAnimations.erase(std::remove(g_RecentAnimations.begin(), g_RecentAnimations.end(), pair), g_RecentAnimations.end());
			g_RecentAnimations.insert(g_RecentAnimations.begin(), pair);
			if (g_RecentAnimations.size() > 10)
				g_RecentAnimations.resize(10);
			SaveAnimationHistory();
		}

		void PushRecentMusic(const std::string& event)
		{
			g_RecentMusic.erase(std::remove(g_RecentMusic.begin(), g_RecentMusic.end(), event), g_RecentMusic.end());
			g_RecentMusic.insert(g_RecentMusic.begin(), event);
			if (g_RecentMusic.size() > 10)
				g_RecentMusic.resize(10);
			SaveMusicHistory();
		}

		void PlayAnimation(const std::string& dict, const std::string& anim)
		{
			if (dict.empty() || anim.empty())
				return;
			PushRecentAnimation(dict, anim);
			FiberPool::Push([dict, anim] {
				for (int i = 0; i < 250 && !STREAMING::HAS_ANIM_DICT_LOADED(dict.c_str()); ++i)
				{
					STREAMING::REQUEST_ANIM_DICT(dict.c_str());
					ScriptMgr::Yield();
				}
				TASK::TASK_PLAY_ANIM(Self::GetPed().GetHandle(), dict.c_str(), anim.c_str(), 8.0f, -8.0f, -1, 0, 0, false, false, false, "", 0);
			});
		}

		void StopAnimation()
		{
			FiberPool::Push([] { TASK::CLEAR_PED_TASKS(Self::GetPed().GetHandle(), true, false); });
		}

		void MusicLoopFiber(std::string event)
		{
			while (g_MusicLoopRunning && g_LoopMusic && !event.empty() && g_LoopingMusicEvent == event)
			{
				AUDIO::PREPARE_MUSIC_EVENT(event.c_str());
				AUDIO::TRIGGER_MUSIC_EVENT(event.c_str());
				for (int i = 0; i < 100 && g_MusicLoopRunning && g_LoopMusic; ++i)
					ScriptMgr::Yield(std::chrono::milliseconds(100));
			}
		}

		void StartMusic(const std::string& event)
		{
			if (event.empty())
				return;
			PushRecentMusic(event);
			FiberPool::Push([event] {
				AUDIO::PREPARE_MUSIC_EVENT(event.c_str());
				AUDIO::TRIGGER_MUSIC_EVENT(event.c_str());
			});
		}

		void StopMusic()
		{
			g_LoopMusic = false;
			g_MusicLoopRunning = false;
			g_LoopingMusicEvent.clear();
			FiberPool::Push([] { AUDIO::TRIGGER_MUSIC_EVENT("MC_MUSIC_STOP"); });
		}

		std::string Lower(std::string value)
		{
			std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return value;
		}

		void SectionTitle(const char* title)
		{
			ImGui::Spacing();
			ImGui::SeparatorText(title);
			ImGui::Spacing();
		}
	}

	void RenderAnimationsCategory()
	{
		static bool initialized = false;
		static std::string dict;
		static std::string anim;
		static std::string musicEvent;
		static char dictFilter[128]{};
		static char animFilter[128]{};
		static char musicFilter[128]{};

		if (!initialized)
		{
			initialized = true;
			AnimationDict::SetAniDictToggle(true);
			AnimationDict::SaveAniDictToggle();
			AnimationDict::FetchAnimationDict();
			LoadAnimationHistory();
			LoadAnimationFavorites();
			LoadMusicHistory();
			LoadMusicFavorites();
		}

		const float available = ImGui::GetContentRegionAvail().x;
		const float columnGap = 14.0f;
		const float columnWidth = std::max(260.0f, (available - columnGap) * 0.5f);

		SectionTitle(Localization::IsPortuguese() ? "ANIMAÇÕES" : "ANIMATIONS");
		if (ImGui::BeginTable("##AnimationModernLayout", available > 650.0f ? 2 : 1, ImGuiTableFlags_SizingStretchSame))
		{
			ImGui::TableNextColumn();
			ImGui::TextDisabled("%s", Localization::IsPortuguese() ? "Dicionário" : "Dictionary");
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::InputTextWithHint("##DictFilter", Localization::IsPortuguese() ? "Filtrar dicionários..." : "Filter dictionaries...", dictFilter, sizeof(dictFilter));

			auto& allDicts = AnimationDict::GetAllAnimations();
			std::vector<std::string> filteredDicts;
			const std::string dictNeedle = Lower(dictFilter);
			for (const auto& [name, animations] : allDicts)
			{
				(void)animations;
				if (dictNeedle.empty() || Lower(name).find(dictNeedle) != std::string::npos)
					filteredDicts.push_back(name);
			}
			std::sort(filteredDicts.begin(), filteredDicts.end());

			const std::string dictPreview = dict.empty() ? (Localization::IsPortuguese() ? "Selecionar dicionário" : "Select dictionary") : dict;
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::BeginCombo("##DictionaryModern", dictPreview.c_str()))
			{
				for (const auto& name : filteredDicts)
				{
					if (ImGui::Selectable(name.c_str(), name == dict))
					{
						dict = name;
						anim.clear();
					}
				}
				ImGui::EndCombo();
			}

			if (ImGui::TableGetColumnCount() > 1)
				ImGui::TableNextColumn();
			ImGui::TextDisabled("%s", Localization::IsPortuguese() ? "Animação" : "Animation");
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::InputTextWithHint("##AnimFilter", Localization::IsPortuguese() ? "Filtrar animações..." : "Filter animations...", animFilter, sizeof(animFilter));

			std::vector<std::string> filteredAnimations;
			if (!dict.empty() && allDicts.contains(dict))
			{
				const std::string animNeedle = Lower(animFilter);
				for (const auto& name : allDicts.at(dict))
					if (animNeedle.empty() || Lower(name).find(animNeedle) != std::string::npos)
						filteredAnimations.push_back(name);
			}
			const std::string animPreview = anim.empty() ? (Localization::IsPortuguese() ? "Selecionar animação" : "Select animation") : anim;
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::BeginCombo("##AnimationModern", animPreview.c_str()))
			{
				for (const auto& name : filteredAnimations)
					if (ImGui::Selectable(name.c_str(), name == anim))
						anim = name;
				ImGui::EndCombo();
			}
			ImGui::EndTable();
		}

		if (ImGui::Button(Localization::IsPortuguese() ? "Reproduzir animação" : "Play animation", ImVec2(190.0f, 32.0f)))
			PlayAnimation(dict, anim);
		ImGui::SameLine();
		if (ImGui::Button(Localization::IsPortuguese() ? "Parar animação" : "Stop animation", ImVec2(170.0f, 32.0f)))
			StopAnimation();
		ImGui::SameLine();
		if (!dict.empty() && !anim.empty())
		{
			const auto pair = std::make_pair(dict, anim);
			const bool favorite = g_FavoriteAnimations.contains(pair);
			if (ImGui::Button(favorite ? (Localization::IsPortuguese() ? "Remover favorita" : "Remove favorite") : (Localization::IsPortuguese() ? "Favoritar" : "Favorite"), ImVec2(150.0f, 32.0f)))
			{
				if (favorite)
					g_FavoriteAnimations.erase(pair);
				else
					g_FavoriteAnimations.insert(pair);
				SaveAnimationFavorites();
			}
		}

		if (!g_RecentAnimations.empty() || !g_FavoriteAnimations.empty())
		{
			ImGui::Spacing();
			if (!g_RecentAnimations.empty())
			{
				if (ImGui::BeginCombo("##RecentAnimations", Localization::IsPortuguese() ? "Animações recentes" : "Recent animations"))
				{
					for (const auto& [recentDict, recentAnim] : g_RecentAnimations)
					{
						const std::string label = recentDict + " / " + recentAnim;
						if (ImGui::Selectable(label.c_str()))
						{
							dict = recentDict;
							anim = recentAnim;
						}
					}
					ImGui::EndCombo();
				}
			}
			if (!g_FavoriteAnimations.empty())
			{
				ImGui::SameLine();
				if (ImGui::BeginCombo("##FavoriteAnimations", Localization::IsPortuguese() ? "Favoritas" : "Favorites"))
				{
					for (const auto& [favDict, favAnim] : g_FavoriteAnimations)
					{
						const std::string label = favDict + " / " + favAnim;
						if (ImGui::Selectable(label.c_str()))
						{
							dict = favDict;
							anim = favAnim;
						}
					}
					ImGui::EndCombo();
				}
			}
		}

		SectionTitle(Localization::IsPortuguese() ? "CENÁRIOS E EMOTES" : "SCENARIOS & EMOTES");
		Features::Self::RenderScenarioPlayer();
		ImGui::Spacing();
		const char* categoryPreview = Emote::emoteCategories[Emote::selectedEmoteCategoryIndex];
		ImGui::SetNextItemWidth(columnWidth);
		if (ImGui::BeginCombo("##EmoteCategoryModern", categoryPreview))
		{
			for (int i = 0; i < Emote::numCategories; ++i)
			{
				if (ImGui::Selectable(Emote::emoteCategories[i], i == Emote::selectedEmoteCategoryIndex))
				{
					Emote::selectedEmoteCategoryIndex = i;
					Emote::selectedEmoteMemberIndex = 0;
				}
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		const auto& selectedEmote = Emote::emoteCategoryMembers[Emote::selectedEmoteCategoryIndex][Emote::selectedEmoteMemberIndex];
		ImGui::SetNextItemWidth(columnWidth);
		if (ImGui::BeginCombo("##EmoteModern", selectedEmote.name ? selectedEmote.name : "-"))
		{
			for (int i = 0; i < Emote::maxEmotesPerCategory; ++i)
			{
				const auto& candidate = Emote::emoteCategoryMembers[Emote::selectedEmoteCategoryIndex][i];
				if (!candidate.name)
					break;
				if (ImGui::Selectable(candidate.name, i == Emote::selectedEmoteMemberIndex))
					Emote::selectedEmoteMemberIndex = i;
			}
			ImGui::EndCombo();
		}
		if (ImGui::Button(Localization::IsPortuguese() ? "Reproduzir emote" : "Play emote", ImVec2(170.0f, 32.0f)) && Pointers.IsSessionStarted && *Pointers.IsSessionStarted)
		{
			FiberPool::Push([] {
				const auto& chosen = Emote::emoteCategoryMembers[Emote::selectedEmoteCategoryIndex][Emote::selectedEmoteMemberIndex];
				if (!chosen.name)
					return;
				TASK::TASK_PLAY_EMOTE_WITH_HASH(Self::GetPed().GetHandle(), static_cast<int>(chosen.type), EMOTE_PM_FULLBODY, static_cast<Hash>(chosen.hash), false, false, false, false, false);
			});
		}

		SectionTitle(Localization::IsPortuguese() ? "BIBLIOTECA DE MÚSICAS" : "MUSIC LIBRARY");
		ImGui::TextDisabled("%s", Localization::IsPortuguese() ? "Pesquise pelo nome legível ou pelo evento interno do jogo." : "Search by readable name or the game's internal event name.");
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputTextWithHint("##MusicModernFilter", Localization::IsPortuguese() ? "Filtrar músicas..." : "Filter music...", musicFilter, sizeof(musicFilter));

		const auto& musicEvents = MusicDict::GetAllMusicEvents();
		std::vector<std::string> filteredMusic;
		const std::string musicNeedle = Lower(musicFilter);
		for (const auto& event : musicEvents)
		{
			const std::string searchable = Lower(event + " " + MusicDict::GetDisplayName(event));
			if (musicNeedle.empty() || searchable.find(musicNeedle) != std::string::npos)
				filteredMusic.push_back(event);
		}

		const std::string musicPreview = musicEvent.empty() ? (Localization::IsPortuguese() ? "Selecionar música" : "Select music") : MusicDict::GetDisplayName(musicEvent);
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::BeginCombo("##MusicModernCombo", musicPreview.c_str()))
		{
			for (const auto& event : filteredMusic)
			{
				const std::string label = MusicDict::GetDisplayName(event) + "##" + event;
				if (ImGui::Selectable(label.c_str(), event == musicEvent))
					musicEvent = event;
			}
			ImGui::EndCombo();
		}

		if (ImGui::Button(Localization::IsPortuguese() ? "Tocar" : "Play", ImVec2(110.0f, 32.0f)))
			StartMusic(musicEvent);
		ImGui::SameLine();
		if (ImGui::Button(Localization::IsPortuguese() ? "Parar" : "Stop", ImVec2(110.0f, 32.0f)))
			StopMusic();
		ImGui::SameLine();
		if (ImGui::Checkbox(Localization::IsPortuguese() ? "Repetir" : "Loop", &g_LoopMusic))
		{
			if (g_LoopMusic && !musicEvent.empty())
			{
				g_MusicLoopRunning = false;
				g_LoopingMusicEvent = musicEvent;
				g_MusicLoopRunning = true;
				FiberPool::Push([event = musicEvent] {
					MusicLoopFiber(event);
					if (g_LoopingMusicEvent == event)
						g_MusicLoopRunning = false;
				});
			}
			else
			{
				g_MusicLoopRunning = false;
				g_LoopingMusicEvent.clear();
			}
		}
		ImGui::SameLine();
		if (!musicEvent.empty())
		{
			const bool favorite = g_FavoriteMusic.contains(musicEvent);
			if (ImGui::Button(favorite ? (Localization::IsPortuguese() ? "Remover favorita" : "Remove favorite") : (Localization::IsPortuguese() ? "Favoritar" : "Favorite"), ImVec2(150.0f, 32.0f)))
			{
				if (favorite)
					g_FavoriteMusic.erase(musicEvent);
				else
					g_FavoriteMusic.insert(musicEvent);
				SaveMusicFavorites();
			}
		}

		if (!g_RecentMusic.empty())
		{
			ImGui::Spacing();
			if (ImGui::BeginCombo("##RecentMusicModern", Localization::IsPortuguese() ? "Músicas recentes" : "Recent music"))
			{
				for (const auto& event : g_RecentMusic)
				{
					const std::string label = MusicDict::GetDisplayName(event) + "##recent" + event;
					if (ImGui::Selectable(label.c_str()))
						musicEvent = event;
				}
				ImGui::EndCombo();
			}
		}
		if (!g_FavoriteMusic.empty())
		{
			ImGui::SameLine();
			if (ImGui::BeginCombo("##FavoriteMusicModern", Localization::IsPortuguese() ? "Músicas favoritas" : "Favorite music"))
			{
				for (const auto& event : g_FavoriteMusic)
				{
					const std::string label = MusicDict::GetDisplayName(event) + "##fav" + event;
					if (ImGui::Selectable(label.c_str()))
						musicEvent = event;
				}
				ImGui::EndCombo();
			}
		}
	}

	Self::Self() :
	    Submenu::Submenu("Personagem")
	{
		LoadAnimationHistory();
		LoadAnimationFavorites();
		LoadMusicHistory();
		LoadMusicFavorites();

		auto main = std::make_shared<Category>("Principal");
		auto globalsGroup = std::make_shared<Group>("Gerais");
		auto movementGroup = std::make_shared<Group>("Movimento");
		auto toolsGroup = std::make_shared<Group>("Ferramentas");

		globalsGroup->AddItem(std::make_shared<BoolCommandItem>("godmode"_J));
		globalsGroup->AddItem(std::make_shared<BoolCommandItem>("neverwanted"_J));
		globalsGroup->AddItem(std::make_shared<BoolCommandItem>("invis"_J));
		globalsGroup->AddItem(std::make_shared<BoolCommandItem>("offtheradar"_J));
		globalsGroup->AddItem(std::make_shared<BoolCommandItem>("noragdoll"_J));
		globalsGroup->AddItem(std::make_shared<BoolCommandItem>("antiafk"_J));
		globalsGroup->AddItem(std::make_shared<BoolCommandItem>("keepbarsfilled"_J));
		globalsGroup->AddItem(std::make_shared<ConditionalItem>("keepbarsfilled"_J, std::make_shared<BoolCommandItem>("keepdeadeyefilled"_J)));
		globalsGroup->AddItem(std::make_shared<ConditionalItem>("keepbarsfilled"_J, std::make_shared<BoolCommandItem>("keepstaminafilled"_J)));
		globalsGroup->AddItem(std::make_shared<ConditionalItem>("keepbarsfilled"_J, std::make_shared<BoolCommandItem>("keephealthfilled"_J)));
		globalsGroup->AddItem(std::make_shared<BoolCommandItem>("keepcoresfilled"_J));
		globalsGroup->AddItem(std::make_shared<ConditionalItem>("keepcoresfilled"_J, std::make_shared<BoolCommandItem>("keepdeadeyecorefilled"_J)));
		globalsGroup->AddItem(std::make_shared<ConditionalItem>("keepcoresfilled"_J, std::make_shared<BoolCommandItem>("keepstaminacorefilled"_J)));
		globalsGroup->AddItem(std::make_shared<ConditionalItem>("keepcoresfilled"_J, std::make_shared<BoolCommandItem>("keephealthcorefilled"_J)));
		globalsGroup->AddItem(std::make_shared<BoolCommandItem>("keepclean"_J));
		globalsGroup->AddItem(std::make_shared<BoolCommandItem>("antilasso"_J));
		globalsGroup->AddItem(std::make_shared<BoolCommandItem>("antihogtie"_J));
		globalsGroup->AddItem(std::make_shared<BoolCommandItem>("antimelee"_J));
		globalsGroup->AddItem(std::make_shared<BoolCommandItem>("drunk"_J));
		globalsGroup->AddItem(std::make_shared<BoolCommandItem>("superpunch"_J));
		globalsGroup->AddItem(std::make_shared<BoolCommandItem>("quickskin"_J));

		toolsGroup->AddItem(std::make_shared<CommandItem>("suicide"_J));
		toolsGroup->AddItem(std::make_shared<CommandItem>("clearcrimes"_J));
		toolsGroup->AddItem(std::make_shared<BoolCommandItem>("npcignore"_J));
		toolsGroup->AddItem(std::make_shared<BoolCommandItem>("eagleeye"_J));
		toolsGroup->AddItem(std::make_shared<BoolCommandItem>("overridewhistle"_J));
		toolsGroup->AddItem(std::make_shared<ConditionalItem>("overridewhistle"_J, std::make_shared<FloatCommandItem>("whistlepitch"_J, "Pitch")));
		toolsGroup->AddItem(std::make_shared<ConditionalItem>("overridewhistle"_J, std::make_shared<FloatCommandItem>("whistleclarity"_J, "Clarity")));
		toolsGroup->AddItem(std::make_shared<ConditionalItem>("overridewhistle"_J, std::make_shared<FloatCommandItem>("whistleshape"_J, "Shape")));

		static float playerScale = 1.0f;
		toolsGroup->AddItem(std::make_shared<ImGuiItem>([] {
			ImGui::Text("%s", Localization::IsPortuguese() ? "Escala do personagem" : "Player Scale");
			ImGui::SetNextItemWidth(180.0f);
			if (ImGui::InputFloat("##PlayerScale", &playerScale))
				FiberPool::Push([] { Self::GetPed().SetScale(playerScale); });
		}, "Escala do personagem", "Ajusta o tamanho visual do seu personagem.", 520.0f));

		movementGroup->AddItem(std::make_shared<BoolCommandItem>("climbsteepslopes"_J));
		movementGroup->AddItem(std::make_shared<BoolCommandItem>("superjump"_J));
		movementGroup->AddItem(std::make_shared<BoolCommandItem>("superrun"_J));
		movementGroup->AddItem(std::make_shared<BoolCommandItem>("noclip"_J));
		movementGroup->AddItem(std::make_shared<ConditionalItem>("noclip"_J, std::make_shared<FloatCommandItem>("noclipspeed"_J)));
		movementGroup->AddItem(std::make_shared<BoolCommandItem>("freecam"_J));
		movementGroup->AddItem(std::make_shared<ConditionalItem>("freecam"_J, std::make_shared<FloatCommandItem>("freecamspeed"_J)));

		main->AddItem(std::move(globalsGroup));
		main->AddItem(std::move(toolsGroup));
		main->AddItem(std::move(movementGroup));
		AddCategory(std::move(main));

		auto nativeUtilities = std::make_shared<Category>("Utilidades");
		auto recoveryUtilities = std::make_shared<Group>("Recuperação");
		auto actionUtilities = std::make_shared<Group>("Ações");
		auto lawUtilities = std::make_shared<Group>("Lei e recompensa");
		recoveryUtilities->AddItem(std::make_shared<CommandItem>("restoreplayer"_J));
		recoveryUtilities->AddItem(std::make_shared<CommandItem>("cleanplayernow"_J));
		recoveryUtilities->AddItem(std::make_shared<CommandItem>("refillcoresnow"_J));
		recoveryUtilities->AddItem(std::make_shared<CommandItem>("refilldeadeyenow"_J));
		actionUtilities->AddItem(std::make_shared<CommandItem>("cleartasks"_J));
		actionUtilities->AddItem(std::make_shared<CommandItem>("ragdollplayer"_J));
		actionUtilities->AddItem(std::make_shared<CommandItem>("removeallweapons"_J));
		lawUtilities->AddItem(std::make_shared<CommandItem>("maximumhostility"_J));
		lawUtilities->AddItem(std::make_shared<CommandItem>("clearlawstate"_J));
		nativeUtilities->AddItem(std::move(recoveryUtilities));
		nativeUtilities->AddItem(std::move(actionUtilities));
		nativeUtilities->AddItem(std::move(lawUtilities));
		AddCategory(std::move(nativeUtilities));

		auto weapons = std::make_shared<Category>("Armas");
		auto weaponsGlobals = std::make_shared<Group>("Gerais");
		weaponsGlobals->AddItem(std::make_shared<BoolCommandItem>("infiniteammo"_J));
		weaponsGlobals->AddItem(std::make_shared<BoolCommandItem>("infiniteclip"_J));
		weaponsGlobals->AddItem(std::make_shared<BoolCommandItem>("nospread"_J));
		weaponsGlobals->AddItem(std::make_shared<BoolCommandItem>("autocock"_J));
		weaponsGlobals->AddItem(std::make_shared<BoolCommandItem>("keepgunsclean"_J));
		weaponsGlobals->AddItem(std::make_shared<ImGuiItem>([] {
			if (ImGui::Button(Localization::IsPortuguese() ? "Entregar todas as armas" : "Give All Weapons"))
				FiberPool::Push([] { Features::TriggerGiveAllWeapons(); });
			if (ImGui::Button(Localization::IsPortuguese() ? "Entregar toda a munição" : "Give All Ammo"))
				FiberPool::Push([] { Features::TriggerGiveAllAmmo(); });
		}, "Entregar armas e munição", "Entrega todas as armas ou recarrega toda a munição.", 520.0f));
		weapons->AddItem(std::move(weaponsGlobals));

		// Old Dead Eye remains, but the obsolete classic heading, ability-unlock/
		// weak-spot toggle and auto-target configuration editor are gone.
		weapons->AddItem(std::make_shared<BoolCommandItem>("olddeadeye"_J));
		weapons->AddItem(std::make_shared<ConditionalItem>("olddeadeye"_J, std::make_shared<BoolCommandItem>("enhanceddeadeye"_J)));
		weapons->AddItem(std::make_shared<ConditionalItem>("olddeadeye"_J, std::make_shared<BoolCommandItem>("slowdeadeyedrain"_J)));
		weapons->AddItem(std::make_shared<ConditionalItem>("olddeadeye"_J, std::make_shared<BoolCommandItem>("slipperybastardaccuracy"_J)));
		weapons->AddItem(std::make_shared<ConditionalItem>("olddeadeye"_J, std::make_shared<BoolCommandItem>("momenttorecuperate"_J)));
		AddCategory(std::move(weapons));

		auto horse = std::make_shared<Category>("Cavalo");
		auto horseGlobals = std::make_shared<Group>("Gerais");
		horseGlobals->AddItem(std::make_shared<BoolCommandItem>("horsegodmode"_J));
		horseGlobals->AddItem(std::make_shared<BoolCommandItem>("horsenoragdoll"_J));
		horseGlobals->AddItem(std::make_shared<BoolCommandItem>("horsesuperrun"_J));
		horseGlobals->AddItem(std::make_shared<BoolCommandItem>("keephorseclean"_J));
		horseGlobals->AddItem(std::make_shared<BoolCommandItem>("horseclimbsteepslopes"_J));
		horseGlobals->AddItem(std::make_shared<BoolCommandItem>("keephorsebarsfilled"_J));
		horseGlobals->AddItem(std::make_shared<BoolCommandItem>("keephorsecoresfilled"_J));
		horseGlobals->AddItem(std::make_shared<BoolCommandItem>("keephorseagitationlow"_J));
		horseGlobals->AddItem(std::make_shared<BoolCommandItem>("flaminghooves"_J));
		horseGlobals->AddItem(std::make_shared<CommandItem>("tpmounttoself"_J));
		static float horseScale = 1.0f;
		horseGlobals->AddItem(std::make_shared<ImGuiItem>([] {
			ImGui::Text("%s", Localization::IsPortuguese() ? "Escala do cavalo" : "Horse Scale");
			if (ImGui::InputFloat("##HorseScale", &horseScale))
				FiberPool::Push([] { Self::GetMount().SetScale(horseScale); });
		}, "Escala do cavalo", "Ajusta o tamanho visual do seu cavalo atual.", 520.0f));
		horse->AddItem(std::move(horseGlobals));
		AddCategory(std::move(horse));

		auto vehicle = std::make_shared<Category>("Veículo");
		auto vehicleGlobals = std::make_shared<Group>("Gerais");
		auto vehicleFun = std::make_shared<Group>("Diversão");
		vehicleGlobals->AddItem(std::make_shared<BoolCommandItem>("vehiclegodmode"_J));
		vehicleGlobals->AddItem(std::make_shared<BoolCommandItem>("vehiclenodetach"_J));
		vehicleGlobals->AddItem(std::make_shared<BoolCommandItem>("flaminghoovesdraft"_J));
		vehicleGlobals->AddItem(std::make_shared<CommandItem>("repairvehicle"_J));
		vehicleFun->AddItem(std::make_shared<BoolCommandItem>("superdrive"_J));
		vehicleFun->AddItem(std::make_shared<ConditionalItem>("superdrive"_J, std::make_shared<BoolCommandItem>("superdrivedirectional"_J, "Directional")));
		vehicleFun->AddItem(std::make_shared<ConditionalItem>("superdrive"_J, std::make_shared<IntCommandItem>("superdriveforce"_J, "Force")));
		vehicleFun->AddItem(std::make_shared<BoolCommandItem>("superbrake"_J));
		vehicle->AddItem(std::move(vehicleGlobals));
		vehicle->AddItem(std::move(vehicleFun));
		AddCategory(std::move(vehicle));

		auto animations = std::make_shared<Category>("Animações");
		animations->AddItem(std::make_shared<ImGuiItem>([] { RenderAnimationsCategory(); },
		    "Animações e Músicas",
		    "Biblioteca modernizada de animações, cenários, emotes e músicas.",
		    620.0f));
		AddCategory(std::move(animations));
	}
}
