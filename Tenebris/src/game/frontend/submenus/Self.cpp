#include "Self.hpp"

#include "core/commands/BoolCommand.hpp"
#include "core/commands/Commands.hpp"
#include "core/commands/IntCommand.hpp"
#include "game/backend/AnimationDict.hpp"
#include "game/backend/FiberPool.hpp"
#include "game/backend/MusicDict.hpp"
#include "game/backend/Players.hpp"
#include "game/backend/ScriptMgr.hpp"
#include "game/backend/Self.hpp"
#include "game/features/Features.hpp"
#include "game/features/self/GiveAllFunctions.hpp"
#include "game/features/self/ScenarioPlayer.hpp"
#include "game/frontend/items/Items.hpp"
#include "game/rdr/Natives.hpp"
#include "game/rdr/data/Emotes.hpp"
#include "util/Rewards.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace YimMenu::Features
{
	BoolCommand _RecoveryEnabled("recoveryenabled", "Recovery Enabled", "Is the recovery feature enabled");
}

namespace YimMenu::Submenus
{
	static std::vector<std::pair<std::string, std::string>> lastPlayedAnimations;
	static std::vector<std::string> lastPlayedMusics;
	static float animationSpeed = 1.0f;
	static std::string currentPlayingDict = "";
	static std::string currentPlayingAnim = "";
	static bool loopMusic = false;
	static bool isMusicLooping = false;
	static std::string loopingMusicEvent = "";
	static std::set<std::pair<std::string, std::string>> favoriteAnimations;
	static std::set<std::string> favoriteMusics;

	static std::string GetTenebrisRoamingPath(const std::string& filename)
	{
		const char* appData = std::getenv("APPDATA");
		std::string base;
		if (appData)
			base = std::string(appData) + "\\Tenebris\\";
		else
			base = "Tenebris\\";
		std::filesystem::create_directories(base);
		return base + filename;
	}

	static void LoadAnimationHistory()
	{
		std::ifstream in(GetTenebrisRoamingPath("animation_history.txt"));
		if (!in.is_open())
			return;
		lastPlayedAnimations.clear();
		std::string dict, anim;
		while (std::getline(in, dict) && std::getline(in, anim))
		{
			lastPlayedAnimations.emplace_back(dict, anim);
			if (lastPlayedAnimations.size() >= 10)
				break;
		}
		in.close();
	}

	static void SaveAnimationHistory()
	{
		std::ofstream out(GetTenebrisRoamingPath("animation_history.txt"), std::ios::trunc);
		for (const auto& [dict, anim] : lastPlayedAnimations)
			out << dict << "\n" << anim << "\n";
		out.close();
	}

	static void LoadFavoriteAnimations()
	{
		std::ifstream in(GetTenebrisRoamingPath("animation_favorites.txt"));
		if (!in.is_open())
			return;
		favoriteAnimations.clear();
		std::string dict, anim;
		while (std::getline(in, dict) && std::getline(in, anim))
			favoriteAnimations.emplace(dict, anim);
		in.close();
	}

	static void SaveFavoriteAnimations()
	{
		std::ofstream out(GetTenebrisRoamingPath("animation_favorites.txt"), std::ios::trunc);
		for (const auto& [dict, anim] : favoriteAnimations)
			out << dict << "\n" << anim << "\n";
		out.close();
	}

	static void LoadMusicHistory()
	{
		std::ifstream in(GetTenebrisRoamingPath("music_history.txt"));
		if (!in.is_open())
			return;
		lastPlayedMusics.clear();
		std::string music;
		while (std::getline(in, music))
		{
			if (!music.empty())
				lastPlayedMusics.push_back(music);
			if (lastPlayedMusics.size() >= 10)
				break;
		}
		in.close();
	}

	static void SaveMusicHistory()
	{
		std::ofstream out(GetTenebrisRoamingPath("music_history.txt"), std::ios::trunc);
		for (const auto& music : lastPlayedMusics)
			out << music << "\n";
		out.close();
	}

	static void LoadFavoriteMusics()
	{
		std::ifstream in(GetTenebrisRoamingPath("music_favorites.txt"));
		if (!in.is_open())
			return;
		favoriteMusics.clear();
		std::string music;
		while (std::getline(in, music))
			if (!music.empty())
				favoriteMusics.insert(music);
		in.close();
	}

	static void SaveFavoriteMusics()
	{
		std::ofstream out(GetTenebrisRoamingPath("music_favorites.txt"), std::ios::trunc);
		for (const auto& music : favoriteMusics)
			out << music << "\n";
		out.close();
	}

	static void MusicLoopFiber(std::string music_event)
	{
		while (isMusicLooping && loopMusic && !music_event.empty())
		{
			AUDIO::PREPARE_MUSIC_EVENT(music_event.c_str());
			AUDIO::TRIGGER_MUSIC_EVENT(music_event.c_str());
			for (int i = 0; i < 100 && isMusicLooping && loopMusic; ++i)
				ScriptMgr::Yield(std::chrono::milliseconds(100));
		}
	}

	static bool shouldStopAnimation = false;
	static void AnimationFiber(std::string dict, std::string anim)
	{
		for (int i = 0; i < 250; i++)
		{
			if (dict.empty() || anim.empty())
				break;

			if (STREAMING::HAS_ANIM_DICT_LOADED(dict.c_str()))
				break;

			STREAMING::REQUEST_ANIM_DICT(dict.c_str());
			ScriptMgr::Yield();
		}

		TASK::TASK_PLAY_ANIM(YimMenu::Self::GetPed().GetHandle(), dict.c_str(), anim.c_str(), 8.0f, -8.0f, -1, 0, 0, false, false, false, "", 0);
	}

	void RenderAnimationsCategory()
	{
		static std::string anim, dict;
		static char dictBuf[256] = {};
		static char animBuf[256] = {};
		static char dictFilterBuf[128] = {};

		static bool aniDictToggleState = YimMenu::AnimationDict::AniDictToggle();
		static bool aniDictToggleChanged = false;

		ImGui::Checkbox("AniDict", &aniDictToggleState);
		if (ImGui::IsItemDeactivatedAfterEdit() || aniDictToggleChanged)
		{
			YimMenu::AnimationDict::SetAniDictToggle(aniDictToggleState);
			YimMenu::AnimationDict::SaveAniDictToggle();
			aniDictToggleChanged = false;
			if (aniDictToggleState)
			{
				YimMenu::AnimationDict::FetchAnimationDict();
			}
		}
		ImGui::SameLine();
		ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Toggle to enable/disable Animation Dictionary. When enabled, AniDict.json will be available for dictionary/animation selection and completion.");
		ImGui::Separator();

		std::strncpy(dictBuf, dict.c_str(), sizeof(dictBuf));
		dictBuf[sizeof(dictBuf) - 1] = '\0';
		std::strncpy(animBuf, anim.c_str(), sizeof(animBuf));
		animBuf[sizeof(animBuf) - 1] = '\0';

		std::string dictFilterStr;

		if (aniDictToggleState)
		{
			auto& allDicts = YimMenu::AnimationDict::GetAllAnimations();

			ImGui::Text("Dictionary");
			ImGui::SetNextItemWidth(250.0f);
			ImGui::InputTextWithHint("##DictionaryFilter", "Filter Dictionaries", dictFilterBuf, sizeof(dictFilterBuf));
			dictFilterStr = dictFilterBuf;

			std::vector<std::string> filteredDicts;
			if (!dictFilterStr.empty())
			{
				std::string filterLower = dictFilterStr;
				std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), [](unsigned char c) {
					return std::tolower(c);
				});
				for (const auto& [dictName, anims] : allDicts)
				{
					std::string dictNameLower = dictName;
					std::transform(dictNameLower.begin(), dictNameLower.end(), dictNameLower.begin(), [](unsigned char c) {
						return std::tolower(c);
					});
					if (dictNameLower.find(filterLower) != std::string::npos)
						filteredDicts.push_back(dictName);
				}
			}
			else
			{
				for (const auto& [dictName, anims] : allDicts)
					filteredDicts.push_back(dictName);
			}

			bool dictDropdownJustSelected = false;
			std::string dictComboPreviewValue = dict.empty() ? "Select Dictionary" : dict;
			if (ImGui::BeginCombo("##DictionaryCombo", dictComboPreviewValue.c_str()))
			{
				for (const auto& dictName : filteredDicts)
				{
					bool isSelected = (dict == dictName);
					if (ImGui::Selectable(dictName.c_str(), isSelected))
					{
						dict = dictName;
						std::strncpy(dictBuf, dict.c_str(), sizeof(dictBuf));
						dictBuf[sizeof(dictBuf) - 1] = '\0';
						anim.clear();
						dictDropdownJustSelected = true;
					}
					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			if (ImGui::InputText("##DictionaryInput", dictBuf, sizeof(dictBuf), ImGuiInputTextFlags_AutoSelectAll))
			{
				dict = dictBuf;
			}
			else if (dictDropdownJustSelected)
			{
				std::strncpy(dictBuf, dict.c_str(), sizeof(dictBuf));
				dictBuf[sizeof(dictBuf) - 1] = '\0';
			}

			ImGui::Text("Animation");
			ImGui::SetNextItemWidth(250.0f);
			std::vector<std::string> matchingAnims;
			if (!dict.empty() && allDicts.contains(dict))
			{
				matchingAnims = allDicts.at(dict);
			}

			bool animDropdownJustSelected = false;
			std::string animComboPreviewValue = anim.empty() ? "Select Animation" : anim;
			if (ImGui::BeginCombo("##AnimationCombo", animComboPreviewValue.c_str()))
			{
				for (const auto& animName : matchingAnims)
				{
					bool isSelected = (anim == animName);
					if (ImGui::Selectable(animName.c_str(), isSelected))
					{
						anim = animName;
						std::strncpy(animBuf, anim.c_str(), sizeof(animBuf));
						animBuf[sizeof(animBuf) - 1] = '\0';
						animDropdownJustSelected = true;
					}
					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			if (ImGui::InputText("##AnimationInput", animBuf, sizeof(animBuf), ImGuiInputTextFlags_AutoSelectAll))
			{
				anim = animBuf;
			}
			else if (animDropdownJustSelected)
			{
				std::strncpy(animBuf, anim.c_str(), sizeof(animBuf));
				animBuf[sizeof(animBuf) - 1] = '\0';
			}
		}
		else
		{
			ImGui::Text("Dictionary");
			ImGui::SetNextItemWidth(250.0f);
			ImGui::InputText("##DictionaryInput", dictBuf, sizeof(dictBuf), ImGuiInputTextFlags_AutoSelectAll);
			dict = dictBuf;

			ImGui::Text("Animation");
			ImGui::SetNextItemWidth(250.0f);
			ImGui::InputText("##AnimationInput", animBuf, sizeof(animBuf), ImGuiInputTextFlags_AutoSelectAll);
			anim = animBuf;
		}

		LoadFavoriteAnimations();
		auto p = std::make_pair(dict, anim);
		bool isFav = !dict.empty() && !anim.empty() && favoriteAnimations.count(p) > 0;
		if (!dict.empty() && !anim.empty())
		{
			if (!isFav)
			{
				if (ImGui::Button("Mark Favorite"))
				{
					favoriteAnimations.insert(p);
					SaveFavoriteAnimations();
				}
				ImGui::SameLine();
			}
			else
			{
				if (ImGui::Button("Unmark Favorite"))
				{
					favoriteAnimations.erase(p);
					SaveFavoriteAnimations();
				}
				ImGui::SameLine();
			}
			if (ImGui::Button("Clear Favorite List"))
			{
				favoriteAnimations.clear();
				SaveFavoriteAnimations();
			}
		}
		if (!favoriteAnimations.empty())
		{
			ImGui::Text("Favorite Animations:");
			static int favAnimIndex = -1;
			std::vector<std::pair<std::string, std::string>> favVec(favoriteAnimations.begin(), favoriteAnimations.end());
			std::vector<std::string> favLabels;
			for (const auto& fav : favVec)
				favLabels.push_back(fav.first + " / " + fav.second);
			const char* preview = (favAnimIndex >= 0 && favAnimIndex < (int)favLabels.size()) ? favLabels[favAnimIndex].c_str() : "Select Favorite";
			if (ImGui::BeginCombo("##AnimFavoritesCombo", preview))
			{
				for (int i = 0; i < (int)favVec.size(); ++i)
				{
					const auto& fav = favVec[i];
					std::string label = fav.first + " / " + fav.second;
					bool isSelected = (favAnimIndex == i);
					if (ImGui::Selectable(label.c_str(), isSelected))
					{
						favAnimIndex = i;
						dict = fav.first;
						anim = fav.second;
						std::strncpy(dictBuf, dict.c_str(), sizeof(dictBuf));
						dictBuf[sizeof(dictBuf) - 1] = '\0';
						std::strncpy(animBuf, anim.c_str(), sizeof(animBuf));
						animBuf[sizeof(animBuf) - 1] = '\0';
					}
					ImGui::SameLine();
					if (ImGui::SmallButton(("Remove##fav" + std::to_string(i)).c_str()))
					{
						favoriteAnimations.erase(fav);
						SaveFavoriteAnimations();
						if (favAnimIndex == i)
							favAnimIndex = -1;
						break;
					}
					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
		}

		if (!lastPlayedAnimations.empty())
		{
			ImGui::Text("Recent Animations:");
			if (ImGui::BeginCombo("##RecentAnims", "Select Animation"))
			{
				for (const auto& [recentDict, recentAnim] : lastPlayedAnimations)
				{
					std::string label = recentDict + " / " + recentAnim;
					if (ImGui::Selectable(label.c_str()))
					{
						dict = recentDict;
						anim = recentAnim;
						std::strncpy(dictBuf, dict.c_str(), sizeof(dictBuf));
						dictBuf[sizeof(dictBuf) - 1] = '\0';
						std::strncpy(animBuf, anim.c_str(), sizeof(animBuf));
						animBuf[sizeof(animBuf) - 1] = '\0';
					}
				}
				ImGui::EndCombo();
			}
			if (ImGui::Button("Clear Animation History"))
			{
				lastPlayedAnimations.clear();
				SaveAnimationHistory();
			}
		}

		ImGui::Spacing();

		if (ImGui::Button("Play Animation"))
		{
			auto pair = std::make_pair(dict, anim);
			if (!dict.empty() && !anim.empty())
			{
				auto it = std::find(lastPlayedAnimations.begin(), lastPlayedAnimations.end(), pair);
				if (it != lastPlayedAnimations.end())
					lastPlayedAnimations.erase(it);
				lastPlayedAnimations.insert(lastPlayedAnimations.begin(), pair);
				if (lastPlayedAnimations.size() > 10)
					lastPlayedAnimations.pop_back();
				SaveAnimationHistory();

				FiberPool::Push([=] {
					for (int i = 0; i < 250; i++)
					{
						if (dict.empty() || anim.empty())
							break;

						if (STREAMING::HAS_ANIM_DICT_LOADED(dict.c_str()))
							break;

						STREAMING::REQUEST_ANIM_DICT(dict.c_str());
						ScriptMgr::Yield();
					}

					TASK::TASK_PLAY_ANIM(YimMenu::Self::GetPed().GetHandle(), dict.c_str(), anim.c_str(), 8.0f, -8.0f, -1, 0, 0, false, false, false, "", 0);
				});
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Stop Animation"))
		{
			FiberPool::Push([=] {
				TASK::CLEAR_PED_TASKS(YimMenu::Self::GetPed().GetHandle(), true, false);
			});
		}

		ImGui::Separator();
		//ImGui::Text("Scenario Player");
		YimMenu::Features::Self::RenderScenarioPlayer();

		ImGui::Separator();

		ImGui::Text("Emote Category");
		if (ImGui::BeginCombo("##Emote Category", Emote::emoteCategories[Emote::selectedEmoteCategoryIndex]))
		{
			for (int i = 0; i < Emote::numCategories; i++)
			{
				bool isSelected = (i == Emote::selectedEmoteCategoryIndex);
				if (ImGui::Selectable(Emote::emoteCategories[i], isSelected))
				{
					Emote::selectedEmoteCategoryIndex = i;
					Emote::selectedEmoteMemberIndex = 0;
				}
				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		ImGui::Text("Emote");
		if (ImGui::BeginCombo("##Emote",
		        Emote::emoteCategoryMembers[Emote::selectedEmoteCategoryIndex][Emote::selectedEmoteMemberIndex].name))
		{
			for (int i = 0; i < Emote::maxEmotesPerCategory; i++)
			{
				const auto& emote = Emote::emoteCategoryMembers[Emote::selectedEmoteCategoryIndex][i];
				if (emote.name == nullptr)
					break;
				bool isSelected = (i == Emote::selectedEmoteMemberIndex);
				if (ImGui::Selectable(emote.name, isSelected))
					Emote::selectedEmoteMemberIndex = i;
				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		if (ImGui::Button("Play Emote"))
		{
			if (*Pointers.IsSessionStarted)
			{
				FiberPool::Push([=] {
					int selectedCategoryIndex = Emote::selectedEmoteCategoryIndex;
					int selectedEmoteIndex = Emote::selectedEmoteMemberIndex;
					const Emote::EmoteItemData& selectedEmote = Emote::emoteCategoryMembers[selectedCategoryIndex][selectedEmoteIndex];
					TASK::TASK_PLAY_EMOTE_WITH_HASH(YimMenu::Self::GetPed().GetHandle(),
					    static_cast<int>(selectedEmote.type),
					    EMOTE_PM_FULLBODY,
					    static_cast<Hash>(selectedEmote.hash),
					    false,
					    false,
					    false,
					    false,
					    false);
				});
			}
		}

		ImGui::Separator();
		ImGui::Text("Biblioteca de musicas");

		static char musicDictFilterBuf[128] = {};
		static std::string music_event;
		static char musicBuf[256] = {};
		static bool musicDropdownJustSelected = false;
		static bool musicLoopWasOn = false;
		static int favMusicIndex = -1;
		static const std::vector<std::string>& musicDict = YimMenu::MusicDict::GetAllMusicEvents();

		std::strncpy(musicBuf, music_event.c_str(), sizeof(musicBuf));
		musicBuf[sizeof(musicBuf) - 1] = '\0';

		ImGui::SetNextItemWidth(250.0f);
		ImGui::InputTextWithHint("##MusicDictFilter", "Filtrar musicas", musicDictFilterBuf, sizeof(musicDictFilterBuf));
		std::string musicDictFilterStr = musicDictFilterBuf;

		static std::vector<std::string> filteredMusicDict;
		static std::string lastMusicFilter;
		static bool musicFilterInitialized = false;
		if (!musicFilterInitialized || musicDictFilterStr != lastMusicFilter)
		{
			musicFilterInitialized = true;
			lastMusicFilter = musicDictFilterStr;
			filteredMusicDict.clear();
			if (musicDictFilterStr.empty())
				filteredMusicDict.assign(musicDict.begin(), musicDict.end());
			else
			{
				std::string filterLower = musicDictFilterStr;
				std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), [](unsigned char c) {
					return std::tolower(c);
				});
				for (const auto& entry : musicDict)
				{
					std::string entryLower = entry + " " + MusicDict::GetDisplayName(entry);
					std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), [](unsigned char c) {
						return std::tolower(c);
					});
					if (entryLower.find(filterLower) != std::string::npos)
						filteredMusicDict.push_back(entry);
				}
			}
		}

		std::string musicComboPreviewValue = music_event.empty() ? "Selecionar musica" : MusicDict::GetDisplayName(music_event);

		if (ImGui::BeginCombo("##MusicDictionaryCombo", musicComboPreviewValue.c_str()))
		{
			for (const auto& musicName : filteredMusicDict)
			{
				bool isSelected = (music_event == musicName);
				const std::string displayName = MusicDict::GetDisplayName(musicName);
				const std::string label = displayName + "##" + musicName;
				if (ImGui::Selectable(label.c_str(), isSelected))
				{
					music_event = musicName;
					std::strncpy(musicBuf, music_event.c_str(), sizeof(musicBuf));
					musicBuf[sizeof(musicBuf) - 1] = '\0';
					musicDropdownJustSelected = true;
				}
				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		if (ImGui::InputText("Evento da musica", musicBuf, sizeof(musicBuf), ImGuiInputTextFlags_AutoSelectAll))
		{
			music_event = musicBuf;
		}
		else if (musicDropdownJustSelected)
		{
			std::strncpy(musicBuf, music_event.c_str(), sizeof(musicBuf));
			musicBuf[sizeof(musicBuf) - 1] = '\0';
		}

		ImGui::Text("Reproducao");
		ImGui::Checkbox("Repetir musica", &loopMusic);
		if (!isMusicLooping && loopMusic && !music_event.empty())
		{
			isMusicLooping = true;
			loopingMusicEvent = music_event;
			FiberPool::Push([music_event = loopingMusicEvent]() {
				MusicLoopFiber(music_event);
				isMusicLooping = false;
			});
		}
		if (isMusicLooping && (!loopMusic || loopingMusicEvent != music_event))
		{
			isMusicLooping = false;
		}
		ImGui::SameLine();
		if (ImGui::Button("Tocar"))
		{
			if (!music_event.empty())
			{
				FiberPool::Push([music_event = music_event]() {
					AUDIO::PREPARE_MUSIC_EVENT(music_event.c_str());
					AUDIO::TRIGGER_MUSIC_EVENT(music_event.c_str());
				});
				auto it = std::find(lastPlayedMusics.begin(), lastPlayedMusics.end(), music_event);
				if (it != lastPlayedMusics.end())
					lastPlayedMusics.erase(it);
				lastPlayedMusics.insert(lastPlayedMusics.begin(), music_event);
				if (lastPlayedMusics.size() > 10)
					lastPlayedMusics.pop_back();
				SaveMusicHistory();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Parar"))
		{
			loopMusic = false;
			isMusicLooping = false;
			FiberPool::Push([] {
				AUDIO::TRIGGER_MUSIC_EVENT("MC_MUSIC_STOP");
			});
		}

		LoadFavoriteMusics();
		bool musicIsFav = !music_event.empty() && favoriteMusics.count(music_event) > 0;
		if (!music_event.empty())
		{
			if (!musicIsFav)
			{
				if (ImGui::Button("Adicionar aos favoritos"))
				{
					favoriteMusics.insert(music_event);
					SaveFavoriteMusics();
				}
				ImGui::SameLine();
			}
			else
			{
				if (ImGui::Button("Remover dos favoritos"))
				{
					favoriteMusics.erase(music_event);
					SaveFavoriteMusics();
					std::ofstream out(GetTenebrisRoamingPath("music_favorites.txt"), std::ios::trunc);
					for (const auto& music : favoriteMusics)
						out << music << "\n";
					out.close();
				}
				ImGui::SameLine();
			}
			if (ImGui::Button("Limpar favoritos"))
			{
				favoriteMusics.clear();
				SaveFavoriteMusics();
				std::ofstream out(GetTenebrisRoamingPath("music_favorites.txt"), std::ios::trunc);
				out.close();
			}
		}
		if (!favoriteMusics.empty())
		{
			ImGui::Text("Musicas favoritas:");
			static int favMusicIndex = -1;
			std::vector<std::string> favMusicLabels(favoriteMusics.begin(), favoriteMusics.end());
			const std::string preview =
			    (favMusicIndex >= 0 && favMusicIndex < (int)favMusicLabels.size()) ? MusicDict::GetDisplayName(favMusicLabels[favMusicIndex]) : "Selecionar favorita";
			if (ImGui::BeginCombo("##MusicFavoritesCombo", preview.c_str()))
			{
				for (int i = 0; i < (int)favMusicLabels.size(); ++i)
				{
					const std::string& fav = favMusicLabels[i];
					bool isSelected = (favMusicIndex == i);
					const std::string favoriteDisplay = MusicDict::GetDisplayName(fav);
					const std::string favoriteLabel = favoriteDisplay + "##favorite" + std::to_string(i);
					if (ImGui::Selectable(favoriteLabel.c_str(), isSelected))
					{
						favMusicIndex = i;
						music_event = fav;
						std::strncpy(musicBuf, music_event.c_str(), sizeof(musicBuf));
						musicBuf[sizeof(musicBuf) - 1] = '\0';
					}
					ImGui::SameLine();
					if (ImGui::SmallButton(("Remove##favmusic" + std::to_string(i)).c_str()))
					{
						favoriteMusics.erase(fav);
						SaveFavoriteMusics();
						std::ofstream out(GetTenebrisRoamingPath("music_favorites.txt"), std::ios::trunc);
						for (const auto& music : favoriteMusics)
							out << music << "\n";
						out.close();
						if (favMusicIndex == i)
							favMusicIndex = -1;
						break;
					}
					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
		}

		if (!lastPlayedMusics.empty())
		{
			ImGui::Text("Musicas recentes:");
			if (ImGui::BeginCombo("##RecentMusics", "Selecionar musica"))
			{
				for (const auto& recentMusic : lastPlayedMusics)
				{
					const std::string recentLabel = MusicDict::GetDisplayName(recentMusic) + "##recent" + recentMusic;
					if (ImGui::Selectable(recentLabel.c_str()))
					{
						music_event = recentMusic;
						std::strncpy(musicBuf, music_event.c_str(), sizeof(musicBuf));
						musicBuf[sizeof(musicBuf) - 1] = '\0';
					}
				}
				ImGui::EndCombo();
			}
			if (ImGui::Button("Limpar historico"))
			{
				lastPlayedMusics.clear();
				SaveMusicHistory();
				std::ofstream out(GetTenebrisRoamingPath("music_history.txt"), std::ios::trunc);
				out.close();
			}
		}
	}

	Self::Self() :
	    Submenu::Submenu("Personagem")
	{
		LoadAnimationHistory();
		LoadFavoriteAnimations();
		LoadFavoriteMusics();
		LoadMusicHistory();

		auto main = std::make_shared<Category>("Principal");
		auto globalsGroup = std::make_shared<Group>("Globals");
		auto movementGroup = std::make_shared<Group>("Movement");
		auto toolsGroup = std::make_shared<Group>("Tools");

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
			ImGui::Text("Player Scale");
			ImGui::SetNextItemWidth(100.0f);
			if (ImGui::InputFloat("##PlayerScale", &playerScale))
			{
				FiberPool::Push([] {
					YimMenu::Self::GetPed().SetScale(playerScale);
				});
			}
		}, "Escala do personagem", "Ajusta o tamanho visual do seu personagem.", 240.0f));

		movementGroup->AddItem(std::make_shared<BoolCommandItem>("climbsteepslopes"_J));
		movementGroup->AddItem(std::make_shared<BoolCommandItem>("superjump"_J));
		movementGroup->AddItem(std::make_shared<BoolCommandItem>("superrun"_J));
		movementGroup->AddItem(std::make_shared<BoolCommandItem>("noclip"_J));
		movementGroup->AddItem(std::make_shared<ConditionalItem>("noclip"_J, std::make_shared<FloatCommandItem>("noclipspeed"_J)));
		movementGroup->AddItem(std::make_shared<BoolCommandItem>("freecam"_J));
		movementGroup->AddItem(std::make_shared<ConditionalItem>("freecam"_J, std::make_shared<FloatCommandItem>("freecamspeed"_J)));

		main->AddItem(globalsGroup);
		main->AddItem(toolsGroup);
		main->AddItem(movementGroup);
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
		auto weaponsGlobalsGroup = std::make_shared<Group>("Globals");
		weaponsGlobalsGroup->AddItem(std::make_shared<BoolCommandItem>("infiniteammo"_J));
		weaponsGlobalsGroup->AddItem(std::make_shared<BoolCommandItem>("infiniteclip"_J));
		weaponsGlobalsGroup->AddItem(std::make_shared<BoolCommandItem>("nospread"_J));
		weaponsGlobalsGroup->AddItem(std::make_shared<BoolCommandItem>("autocock"_J));
		weaponsGlobalsGroup->AddItem(std::make_shared<BoolCommandItem>("keepgunsclean"_J));

		weaponsGlobalsGroup->AddItem(std::make_shared<ImGuiItem>([] {
			if (ImGui::Button("Give All Weapons"))
			{
				FiberPool::Push([] {
					YimMenu::Features::TriggerGiveAllWeapons();
				});
			}
			if (ImGui::Button("Give All Ammo"))
			{
				FiberPool::Push([] {
					YimMenu::Features::TriggerGiveAllAmmo();
				});
			}
		}, "Entregar armas e municao", "Entrega todas as armas ou recarrega toda a municao.", 260.0f));

		weapons->AddItem(weaponsGlobalsGroup);

		weapons->AddItem(std::make_shared<ImGuiItem>([] {
			ImGui::SeparatorText("Old Deadeye");
		}, "Dead Eye classico", "Identifica o conjunto de opcoes do modo Dead Eye classico.", 180.0f));
		weapons->AddItem(std::make_shared<BoolCommandItem>("olddeadeye"_J));
		weapons->AddItem(std::make_shared<ConditionalItem>("olddeadeye"_J, std::make_shared<BoolCommandItem>("unlockdeadeyeabilities"_J)));
		weapons->AddItem(std::make_shared<ConditionalItem>("olddeadeye"_J, std::make_shared<BoolCommandItem>("enhanceddeadeye"_J)));
		weapons->AddItem(std::make_shared<ConditionalItem>("olddeadeye"_J, std::make_shared<BoolCommandItem>("slowdeadeyedrain"_J)));
		weapons->AddItem(std::make_shared<ConditionalItem>("olddeadeye"_J, std::make_shared<BoolCommandItem>("slipperybastardaccuracy"_J)));
		weapons->AddItem(std::make_shared<ConditionalItem>("olddeadeye"_J, std::make_shared<BoolCommandItem>("momenttorecuperate"_J)));
		weapons->AddItem(std::make_shared<ConditionalItem>("olddeadeye"_J, std::make_shared<BoolCommandItem>("deadeyetagging"_J)));
		weapons->AddItem(std::make_shared<ConditionalItem>("deadeyetagging"_J, std::make_shared<ImGuiItem>([] {
			static int current_item_index = 0;
			const char* items[] = {"Enemies", "Animals", "All"};

			auto command = Commands::GetCommand<IntCommand>("deadeyetaggingconfig"_J);
			if (!command)
				return;

			// Note: The IntCommand methods might need to be implemented or accessed differently
			// For now, we'll use a static variable to track the state
			static int currentValue = 7;

			if (currentValue == 6)
				current_item_index = 0;
			else if (currentValue == 8)
				current_item_index = 1;
			else if (currentValue == 7)
				current_item_index = 2;

			ImGui::Text("Auto Tag");
			ImGui::SetNextItemWidth(150.f);

			if (ImGui::Combo("##deadeyetaggingdropdown", &current_item_index, items, IM_ARRAYSIZE(items)))
			{
				int newValue = 6; // Default to Enemies
				if (current_item_index == 0)
					newValue = 6; // Enemies
				else if (current_item_index == 1)
					newValue = 8; // Animals
				else if (current_item_index == 2)
					newValue = 7; // All

				currentValue = newValue;
				// TODO: Set the command value when the proper method is available
				// FiberPool::Push([=] {
				//     command->SetValue(newValue);
				// });
			}
		}, "Alvos automaticos do Dead Eye", "Escolhe se o Dead Eye marca inimigos, animais ou todos os alvos.", 260.0f)));

		AddCategory(std::move(weapons));

		auto horse = std::make_shared<Category>("Cavalo");
		auto horseGlobalsGroup = std::make_shared<Group>("Globals");
		horseGlobalsGroup->AddItem(std::make_shared<BoolCommandItem>("horsegodmode"_J));
		horseGlobalsGroup->AddItem(std::make_shared<BoolCommandItem>("horsenoragdoll"_J));
		horseGlobalsGroup->AddItem(std::make_shared<BoolCommandItem>("horsesuperrun"_J));
		horseGlobalsGroup->AddItem(std::make_shared<BoolCommandItem>("keephorseclean"_J));
		horseGlobalsGroup->AddItem(std::make_shared<BoolCommandItem>("horseclimbsteepslopes"_J));
		horseGlobalsGroup->AddItem(std::make_shared<BoolCommandItem>("keephorsebarsfilled"_J));
		horseGlobalsGroup->AddItem(std::make_shared<BoolCommandItem>("keephorsecoresfilled"_J));
		horseGlobalsGroup->AddItem(std::make_shared<BoolCommandItem>("keephorseagitationlow"_J));
		horseGlobalsGroup->AddItem(std::make_shared<BoolCommandItem>("flaminghooves"_J));
		horseGlobalsGroup->AddItem(std::make_shared<CommandItem>("tpmounttoself"_J));

		static float horseScale = 1.0f;
		horseGlobalsGroup->AddItem(std::make_shared<ImGuiItem>([] {
			ImGui::Text("Horse Scale");
			ImGui::SetNextItemWidth(100.0f);
			if (ImGui::InputFloat("##HorseScale", &horseScale))
			{
				FiberPool::Push([] {
					YimMenu::Self::GetMount().SetScale(horseScale);
				});
			}
		}, "Escala do cavalo", "Ajusta o tamanho visual do seu cavalo atual.", 240.0f));
		horse->AddItem(horseGlobalsGroup);
		AddCategory(std::move(horse));

		auto vehicle = std::make_shared<Category>("Veiculo");
		auto vehicleGlobalsGroup = std::make_shared<Group>("Globals");
		auto vehicleFunGroup = std::make_shared<Group>("Fun");
		vehicleGlobalsGroup->AddItem(std::make_shared<BoolCommandItem>("vehiclegodmode"_J));
		vehicleGlobalsGroup->AddItem(std::make_shared<BoolCommandItem>("vehiclenodetach"_J));
		vehicleGlobalsGroup->AddItem(std::make_shared<BoolCommandItem>("flaminghoovesdraft"_J));
		vehicleGlobalsGroup->AddItem(std::make_shared<CommandItem>("repairvehicle"_J));
		vehicleFunGroup->AddItem(std::make_shared<BoolCommandItem>("superdrive"_J));
		vehicleFunGroup->AddItem(std::make_shared<ConditionalItem>("superdrive"_J, std::make_shared<BoolCommandItem>("superdrivedirectional"_J, "Directional")));
		vehicleFunGroup->AddItem(std::make_shared<ConditionalItem>("superdrive"_J, std::make_shared<IntCommandItem>("superdriveforce"_J, "Force")));
		vehicleFunGroup->AddItem(std::make_shared<BoolCommandItem>("superbrake"_J));
		vehicle->AddItem(vehicleGlobalsGroup);
		vehicle->AddItem(vehicleFunGroup);
		AddCategory(std::move(vehicle));

		auto animations = std::make_shared<Category>("Animacoes");
		animations->AddItem(std::make_shared<ImGuiItem>([] {
			YimMenu::Submenus::RenderAnimationsCategory();
		}, "Animacoes e musicas", "Seleciona animacoes, emotes e musicas para reproduzir no personagem."));
		AddCategory(std::move(animations));
	}
}
