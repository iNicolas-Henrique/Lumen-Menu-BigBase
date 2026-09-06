#pragma once

#include <memory>

namespace YimMenu
{
	class Submenu;
}

namespace YimMenu::Submenus
{
	void InstallFaceEditor(const std::shared_ptr<Submenu>& selfSubmenu);
}
