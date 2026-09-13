#include "PerformanceOptions.hpp"

namespace YimMenu::PerformanceOptions
{
	BoolCommand MenuAnimations{"perfmenuanimations", "Animações do menu", "", true};
	BoolCommand MenuShadows{"perfmenushadows", "Sombras do menu", "", true};
	BoolCommand EditorAnimations{"perfeditoranimations", "Animações do editor", "", true};
	BoolCommand NotificationAnimations{"perfnotificationanimations", "Animações de notificações", "", true};
	BoolCommand NotificationProgressBar{"perfnotificationprogress", "Barra de tempo das notificações", "", true};
}
