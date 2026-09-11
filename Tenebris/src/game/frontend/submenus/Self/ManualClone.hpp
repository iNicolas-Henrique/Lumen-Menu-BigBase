#pragma once

#include "game/rdr/invoker/Invoker.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

namespace YimMenu
{
	class UIItem;
}

namespace YimMenu::Submenus::CloneNativeCompat
{
	template <typename... Args>
	inline bool InvokeByHash(rage::scrNativeHash hash, Args&&... args)
	{
		std::size_t nativeIndex{};
		bool found{};
		for (; nativeIndex < g_Crossmap.size(); ++nativeIndex)
		{
			if (g_Crossmap[nativeIndex] == hash)
			{
				found = true;
				break;
			}
		}

		if (!found)
			return false;

		auto handler = NativeInvoker::GetNativeHandler(static_cast<NativeIndex>(nativeIndex));
		if (!handler)
			return false;

		NativeInvoker invoker{};
		invoker.BeginCall();
		(invoker.PushArg(std::forward<Args>(args)), ...);
		handler(&invoker.m_CallContext);
		return true;
	}
}

// Compatibility overloads used only by ManualClone.cpp. The generated NativeDB
// snapshot in this project does not expose _SET_PED_COMPONENT_ENABLED, while the
// RDR3 native _APPLY_SHOP_ITEM_TO_PED (0xD3A7B003ED343FD9) is the correct way to
// apply an MP shop clothing component such as the Halloween masks. The ragdoll
// overload also accepts the legacy bool final argument used by ManualClone and
// translates it to the native's actual final const-char* parameter (nullptr).
namespace PED
{
	inline void _SET_PED_COMPONENT_ENABLED(int ped, std::uint32_t componentHash, bool immediately, bool isMp, bool p4)
	{
		YimMenu::Submenus::CloneNativeCompat::InvokeByHash(
		    static_cast<rage::scrNativeHash>(0xD3A7B003ED343FD9ULL),
		    ped,
		    componentHash,
		    immediately,
		    isMp,
		    p4);
	}

	inline int SET_PED_TO_RAGDOLL(int ped, int timeMin, int timeMax, int ragdollType, bool abortIfInjured, bool abortIfDead, bool)
	{
		const char* taskMessage = nullptr;
		const bool invoked = YimMenu::Submenus::CloneNativeCompat::InvokeByHash(
		    static_cast<rage::scrNativeHash>(0xAE99FB955581844AULL),
		    ped,
		    timeMin,
		    timeMax,
		    ragdollType,
		    abortIfInjured,
		    abortIfDead,
		    taskMessage);
		return invoked ? 1 : 0;
	}
}

namespace YimMenu::Submenus
{
	std::shared_ptr<UIItem> CreateManualCloneItem();
}
