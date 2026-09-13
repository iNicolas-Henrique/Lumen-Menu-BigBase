#include "core/hooking/DetourHook.hpp"
#include "game/hooks/Hooks.hpp"
#include "game/hooks/Protections/SyncBoundsGuard.hpp"
#include "game/pointers/Pointers.hpp"
#include "game/rdr/Nodes.hpp"
#include "game/backend/Protections.hpp"
#include "util/Joaat.hpp"

#include <network/netObject.hpp>

namespace YimMenu::Hooks
{
	bool Protections::CanApplyData(rage::netSyncTree* tree, rage::netObject* object)
	{
		if (!BaseHook::Get<Protections::CanApplyData, DetourHook<decltype(&Protections::CanApplyData)>>()->Original()(tree, object))
			return false;

		if (!tree || !object)
			return false;

		// Validate serialized array counts before the legacy visitor can index them.
		if (HasUnsafeSyncBounds(tree))
			return false;

		if (ShouldBlockSync(tree, (NetObjType)object->m_ObjectType, object))
			return false;

		return true;
	}
}