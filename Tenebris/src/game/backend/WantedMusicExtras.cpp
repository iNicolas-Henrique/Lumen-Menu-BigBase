#include "WantedMusicExtras.hpp"

#include "MusicDict.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace YimMenu
{
	namespace
	{
		std::filesystem::path GetMusicDictFilePath()
		{
			const char* appData = std::getenv("APPDATA");
			if (!appData)
				return std::filesystem::current_path() / "MusicDict.json";
			return std::filesystem::path(appData) / "Tenebris" / "MusicDict.json";
		}

		constexpr std::array<const char*, 25> kWantedMusicEvents{
		    "CABR01_WANTED",
		    "CLDN3_WANTED",
		    "NTS1_WITNESS_WANTED",
		    "ME_BUSTED_L3",
		    "ME_BUSTED_L4",
		    "ME_BUSTED_L5",
		    "ME_ESCAPED_WANTED_L3",
		    "ME_ESCAPED_WANTED_L4",
		    "ME_ESCAPED_WANTED_L5",
		    "ME_EVADING_WANTED_L3",
		    "ME_EVADING_WANTED_L4",
		    "ME_EVADING_WANTED_L5",
		    "ME_GAINED_WANTED",
		    "ME_GAINED_WANTED_L3",
		    "ME_GAINED_WANTED_L4",
		    "ME_GAINED_WANTED_L5",
		    "ME_REGAINED_WANTED_L3",
		    "ME_REGAINED_WANTED_L4",
		    "ME_REGAINED_WANTED_L5",
		    "ME_SEARCHING_WANTED_L3",
		    "ME_SEARCHING_WANTED_L4",
		    "ME_SEARCHING_WANTED_L5",
		    "ME_STOP_WANTED_MUSIC_MUTED",
		    "ME_WANTED_MUSIC_DISABLED",
		    "MC_MUSIC_STOP",
		};
	}

	void EnsureWantedMusicExtras()
	{
		static bool done = false;
		if (done)
			return;
		done = true;

		// Force MusicDict to create/load the normal user file first. We then merge
		// the curated wanted set into existing installations instead of only adding
		// them to the defaults used by fresh installs.
		(void)MusicDict::GetAllMusicEvents();

		const auto filePath = GetMusicDictFilePath();
		std::ifstream input(filePath);
		if (!input.is_open())
			return;

		nlohmann::json json;
		try
		{
			input >> json;
		}
		catch (...)
		{
			LOG(WARNING) << "[WantedMusicExtras] could not parse MusicDict.json";
			return;
		}
		input.close();

		if (!json.is_array())
			return;

		std::vector<std::string> events;
		events.reserve(json.size() + kWantedMusicEvents.size());
		for (const auto& entry : json)
		{
			if (entry.is_string())
				events.push_back(entry.get<std::string>());
		}

		std::size_t added{};
		for (const char* wantedEvent : kWantedMusicEvents)
		{
			if (std::find(events.begin(), events.end(), wantedEvent) != events.end())
				continue;
			events.emplace_back(wantedEvent);
			++added;
		}

		if (!added)
			return;

		std::sort(events.begin(), events.end());
		events.erase(std::unique(events.begin(), events.end()), events.end());

		std::ofstream output(filePath, std::ios::trunc);
		if (!output.is_open())
			return;
		output << nlohmann::json(events).dump(4);
		output.close();

		MusicDict::ReloadMusicDict();
		LOG(INFO) << "[WantedMusicExtras] merged " << added << " missing wanted music event(s)";
	}
}
