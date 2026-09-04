#pragma once
#include "core/frontend/classic/AbstractOption.hpp"
#include "util/Joaat.hpp"

namespace YimMenu
{
	using MenuAction = Classic::OptionAction;

	// Preferably, all items should be saved in a global instance to make UI elements searchable
	class UIItem : public Classic::AbstractOption
	{
	public:
		virtual ~UIItem() = default;
		virtual void Draw() = 0;
		virtual std::string_view GetMenuLabel() const
		{
			return "Opcao avancada";
		}
		virtual std::string GetMenuValue() const
		{
			return {};
		}
		virtual std::string_view GetMenuDescription() const
		{
			return {};
		}
		virtual void HandleMenuAction(MenuAction)
		{
		}
		std::string_view GetLeftText() const final
		{
			return GetMenuLabel();
		}
		std::string GetRightText() const final
		{
			return GetMenuValue();
		}
		std::string_view GetDescription() const final
		{
			return GetMenuDescription();
		}
		void HandleAction(Classic::OptionAction action) final
		{
			HandleMenuAction(action);
		}
		virtual bool RequiresImGuiEditor() const
		{
			return false;
		}
		virtual float GetPreferredEditorHeight() const
		{
			return 240.0f;
		}
		virtual void OnEditorOpened()
		{
		}
		virtual void OnEditorClosed()
		{
		}
		virtual bool IsVisible() const
		{
			return true;
		}
		virtual void CollectMenuItems(std::vector<UIItem*>& items)
		{
			if (IsVisible())
				items.push_back(this);
		}
	};
}
