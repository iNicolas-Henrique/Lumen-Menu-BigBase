#pragma once
#include <atomic>
#include <rage/joaat.hpp>
#include <network/sync/netSyncTree.hpp>
#include "Enums.hpp"
#include "util/Joaat.hpp"

// Taken from https://github.com/YimMenu/YimMenu/blob/207e5457f9fca9e638168b1f26ec210116d7ad7d/src/util/sync_trees.hpp
// Credits to Aure7138

namespace YimMenu
{
	struct SyncNodeId
	{
		rage::joaat_t id;
		const char* name;

		constexpr SyncNodeId()
		{
			id   = 0;
			name = "INVALID NODE ID!";
		}

		template<size_t N>
		constexpr SyncNodeId(char const (&pp)[N])
		{
			id   = Joaat(pp);
			name = pp;
		}

		// implicit conversion
		constexpr operator rage::joaat_t() const
		{
			return id;
		}
	};

	// Sync Node Virtual Function Table address to all sync node identifiers that also have that vft address.
	using SyncNodeVftToIds = std::unordered_map<uint64_t, SyncNodeId>;

	// Sync Tree node array index to node identifier.
	using SyncTreeNodeArrayIndexToNodeId = std::vector<SyncNodeId>;

	struct SyncNodeFinder
	{
		static constexpr size_t sm_SyncTreeCount = size_t(NetObjType::Max);

		SyncNodeVftToIds m_SyncNodeMap;
		std::array<SyncTreeNodeArrayIndexToNodeId, sm_SyncTreeCount> m_SyncTreeNodeIdsMap;
		std::vector<SyncNodeId> m_GlobalNodeIds;

		SyncNodeFinder();
	};

	class Nodes
	{
	private:
		SyncNodeFinder m_Finder;
		std::atomic_bool m_Initialized{false};
		std::mutex m_InitMutex;

		SyncNodeId& FindImpl(uintptr_t addr);
		void InitImpl();

		static Nodes& GetInstance()
		{
			static Nodes Instance;
			return Instance;
		}

	public:
		static void Init()
		{
			auto& instance = GetInstance();
			if (instance.m_Initialized.load(std::memory_order_acquire))
				return;

			std::lock_guard guard(instance.m_InitMutex);
			if (!instance.m_Initialized.load(std::memory_order_relaxed))
				instance.InitImpl();
		}

		static void Reset()
		{
			auto& instance = GetInstance();
			std::lock_guard guard(instance.m_InitMutex);
			instance.m_Finder.m_SyncNodeMap = {};
			instance.m_Finder.m_GlobalNodeIds = {};
			instance.m_Initialized.store(false, std::memory_order_release);
		}

		static bool IsInitialized()
		{
			return GetInstance().m_Initialized.load(std::memory_order_acquire);
		}

		static SyncNodeId& Find(uintptr_t addr)
		{
			return GetInstance().FindImpl(addr);
		}

		static std::vector<SyncNodeId>& GetAllNodeIds()
		{
			return GetInstance().m_Finder.m_GlobalNodeIds;
		}
	};
}

namespace std
{
	template<>
	struct hash<YimMenu::SyncNodeId>
	{
		size_t operator()(const YimMenu::SyncNodeId& x) const
		{
			return x.id;
		}
	};
}