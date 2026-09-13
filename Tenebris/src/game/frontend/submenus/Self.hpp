#pragma once
#include "core/frontend/manager/UIManager.hpp"
#include "game/backend/AnimationDict.hpp"
#include "game/backend/MusicDict.hpp"
#include "game/backend/Self.hpp"

namespace YimMenu::Submenus
{
	class Self : public Submenu
	{
	public:
		Self();

		// Keep editor lambdas concise while forwarding to the actual local-player backend.
		static auto GetPed()
		{
			return YimMenu::Self::GetPed();
		}

		static auto GetMount()
		{
			return YimMenu::Self::GetMount();
		}
	};

	void RenderAnimationsCategory();
}
