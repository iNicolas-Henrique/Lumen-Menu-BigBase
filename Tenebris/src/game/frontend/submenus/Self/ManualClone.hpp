#pragma once

#include "game/rdr/Natives.hpp"

#include <cstdint>
#include <memory>

namespace YimMenu
{
	class UIItem;
}

// Compatibility overloads kept local to the manual-clone translation unit.
// They deliberately call the generated NativeDB wrappers instead of resolving
// hashes/handlers at runtime, keeping clone runtime calls typed and preventing
// invalid function-pointer dispatches from compatibility shims.
namespace PED
{
	inline void _SET_PED_COMPONENT_ENABLED(int ped, std::uint32_t componentHash, bool immediately, bool isMp, bool p4)
	{
		_APPLY_SHOP_ITEM_TO_PED(ped, componentHash, immediately, isMp, p4);
	}

	inline int SET_PED_TO_RAGDOLL(int ped, int timeMin, int timeMax, int ragdollType, bool abortIfInjured, bool abortIfDead, bool)
	{
		return PED::SET_PED_TO_RAGDOLL(
		    ped,
		    timeMin,
		    timeMax,
		    ragdollType,
		    abortIfInjured,
		    abortIfDead,
		    static_cast<const char*>(nullptr));
	}
}

namespace PLAYER
{
	// RDR2's generated native has a third BOOL parameter. Keep the legacy
	// two-argument clone call source-compatible, but dispatch through the exact
	// generated NativeDB wrapper rather than a hand-built hash invoker.
	inline bool IS_PLAYER_TARGETTING_ENTITY(int player, int entity)
	{
		return PLAYER::IS_PLAYER_TARGETTING_ENTITY(player, entity, false);
	}
}

namespace YimMenu::Submenus
{
	std::shared_ptr<UIItem> CreateManualCloneItem();
}
