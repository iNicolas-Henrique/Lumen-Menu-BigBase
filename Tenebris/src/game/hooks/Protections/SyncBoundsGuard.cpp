#include "SyncBoundsGuard.hpp"

#include "game/rdr/Nodes.hpp"
#include "util/Joaat.hpp"

#include <network/sync/CProjectBaseSyncDataNode.hpp>
#include <network/sync/netSyncTree.hpp>
#include <network/sync/ped/CPedTaskTreeData.hpp>

#include <cstdint>

namespace YimMenu::Hooks::Protections
{
	namespace
	{
		constexpr int kMaxVisitedSyncNodes = 160;
		constexpr int kMaxTaskTrees = 5;
		constexpr std::uint32_t kMaxTasksPerTree = 12;
		constexpr int kMaxWantedEntries = 16;

		bool ValidateNode(CProjectBaseSyncDataNode* node, int& visited)
		{
			if (!node)
				return false;

			// A normal RDR2 sync tree is much smaller than this. The guard also
			// prevents a corrupted sibling/child chain from recursing forever.
			if (++visited > kMaxVisitedSyncNodes)
			{
				LOG(WARNING) << "[SyncBoundsGuard] rejecting malformed sync tree: excessive node count";
				return true;
			}

			if (node->IsParentNode())
			{
				for (auto child = node->m_FirstChild; child; child = child->m_NextSibling)
				{
					if (ValidateNode(reinterpret_cast<CProjectBaseSyncDataNode*>(child), visited))
						return true;
				}
				return false;
			}

			if (!node->IsDataNode() || !node->IsActive())
				return false;

			const SyncNodeId id = Nodes::Find(reinterpret_cast<std::uintptr_t>(node));
			switch (id)
			{
			case "CPedTaskTreeNode"_J:
			{
				auto& data = node->GetData<CPedTaskTreeData>();
				const int treeCount = data.GetNumTaskTrees();
				if (treeCount < 0 || treeCount > kMaxTaskTrees)
				{
					LOG(WARNING) << "[SyncBoundsGuard] rejecting CPedTaskTreeNode with invalid tree count: " << treeCount;
					return true;
				}

				for (int i = 0; i < treeCount; ++i)
				{
					const std::uint32_t taskCount = data.m_Trees[i].m_NumTasks;
					if (taskCount > kMaxTasksPerTree)
					{
						LOG(WARNING) << "[SyncBoundsGuard] rejecting CPedTaskTreeNode with invalid task count: " << taskCount;
						return true;
					}
				}
				break;
			}
			case "Node_14359d660"_J:
			{
				const auto base = reinterpret_cast<std::uintptr_t>(&node->GetData<char>());
				const int count = *reinterpret_cast<const int*>(base + 36);
				if (count < 0 || count > kMaxWantedEntries)
				{
					LOG(WARNING) << "[SyncBoundsGuard] rejecting Node_14359d660 with invalid entry count: " << count;
					return true;
				}

				// The logger already documents sixteen fixed entry slots for this node.
				// Validate only the serialized count, never an attacker-controlled range.
				for (int i = 0; i < count; ++i)
				{
					const int lower = *reinterpret_cast<const int*>(base + 36ULL * i + 64ULL);
					const int upper = *reinterpret_cast<const int*>(base + 36ULL * i + 72ULL);
					if (upper < lower)
					{
						LOG(WARNING) << "[SyncBoundsGuard] rejecting Node_14359d660 with inverted range";
						return true;
					}
				}
				break;
			}
			default:
				break;
			}

			return false;
		}
	}

	bool HasUnsafeSyncBounds(rage::netSyncTree* tree)
	{
		if (!tree || !tree->m_NextSyncNode)
			return false;

		Nodes::Init();
		int visited{};
		return ValidateNode(reinterpret_cast<CProjectBaseSyncDataNode*>(tree->m_NextSyncNode), visited);
	}
}
