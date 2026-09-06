#pragma once

#include <memory>

namespace YimMenu
{
	class Submenu;

	namespace AbilityCardPower
	{
		void Tick();
	}

	namespace Submenus
	{
		void InstallAbilityCardPower(const std::shared_ptr<Submenu>& selfSubmenu);
	}
}
