#pragma once

namespace YimMenu
{
	class UIItem;

	class AdvancedEditor
	{
	public:
		static void Open(UIItem* item);
		static void Draw();
		static bool IsOpen();
	};
}
