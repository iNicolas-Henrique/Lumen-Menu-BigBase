#pragma once

namespace YimMenu
{
	class UIItem;

	class AdvancedEditor
	{
	public:
		static void Open(UIItem* item);
		static void Close();
		static void CloseImmediate();
		static void Tick();
		static void Draw();
		static bool IsOpen();
		static bool HandleKey(int key);
		static float GetEditorAlpha();
		static float GetClassicMenuAlpha();
	};
}
