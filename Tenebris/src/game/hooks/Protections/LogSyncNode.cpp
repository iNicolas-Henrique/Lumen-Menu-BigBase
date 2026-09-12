#include "game/hooks/Hooks.hpp"
#include "game/backend/PlayerData.hpp"
#include "game/rdr/Nodes.hpp"

#include <network/netObject.hpp>

#include <chrono>
#include <cstdint>
#include <mutex>

namespace YimMenu::Hooks
{
    void Protections::LogSyncNode(CProjectBaseSyncDataNode* node, SyncNodeId& id, NetObjType type, rage::netObject* object, Player& player)
    {
        using Clock = std::chrono::steady_clock;
        using namespace std::chrono_literals;

        // Full field-by-field sync dumps can generate thousands of log lines per
        // second in a populated session and stall the game. Keep this diagnostic
        // useful without turning the logger into a hot-path bottleneck.
        static std::mutex logMutex;
        static Clock::time_point windowStart = Clock::now();
        static std::uint32_t emitted{};
        static std::uint32_t suppressed{};
        constexpr std::uint32_t kMaxEntriesPerSecond = 40;

        std::lock_guard guard(logMutex);
        const auto now = Clock::now();
        if (now - windowStart >= 1s)
        {
            if (suppressed > 0)
                LOG(INFO) << "[SyncLog] " << suppressed << " verbose sync entries suppressed in the previous second";

            windowStart = now;
            emitted = 0;
            suppressed = 0;
        }

        if (emitted >= kMaxEntriesPerSecond)
        {
            ++suppressed;
            return;
        }
        ++emitted;

        const int objectId = object ? object->m_ObjectId : -1;
        const auto objectType = static_cast<int>(type);
        if (player.IsValid())
            LOG(INFO) << "[SyncLog] " << player.GetName() << ": " << id.name << ", object=" << objectId << ", type=" << objectType;
        else
            LOG(INFO) << "[SyncLog] UNKNOWN: " << id.name << ", object=" << objectId << ", type=" << objectType;
    }
}
