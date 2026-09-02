#pragma once
#include <vector>
#include <string>
#include <string_view>

namespace YimMenu
{
    class MusicDict
    {
    public:
        static const std::vector<std::string>& GetAllMusicEvents();
        static std::string GetDisplayName(std::string_view eventName);
        static void ReloadMusicDict();
    };
}
