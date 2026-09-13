#pragma once

#include <memory>

namespace YimMenu
{
	class UIItem;
}

namespace YimMenu::Submenus
{
	std::shared_ptr<UIItem> CreateHumanPedSpawnerItem();
	std::shared_ptr<UIItem> CreateAnimalPedSpawnerItem();
}
