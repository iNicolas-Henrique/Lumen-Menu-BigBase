#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace YimMenu::Classic
{
	enum class OptionAction
	{
		Enter,
		Left,
		Right,
	};

	class AbstractOption
	{
	public:
		virtual ~AbstractOption() = default;
		virtual std::string_view GetLeftText() const = 0;
		virtual std::string GetRightText() const
		{
			return {};
		}
		virtual std::string_view GetDescription() const
		{
			return {};
		}
		virtual void HandleAction(OptionAction)
		{
		}
	};
}
