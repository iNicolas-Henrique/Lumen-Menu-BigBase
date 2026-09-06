#pragma once

#include <memory>

namespace YimMenu
{
	class Submenu;

	namespace AbilityCards
	{
		// Called from the game script fiber. Runtime overrides only replace the
		// equipped ability type; the game's tier/progression value is never changed.
		void Tick();
	}

	namespace Submenus
	{
		void InstallAbilityCards(const std::shared_ptr<Submenu>& selfSubmenu);
	}
}
