#pragma once

#include "AbstractOption.hpp"

namespace YimMenu::Classic
{
	class BaseOption : public AbstractOption
	{
	public:
		BaseOption(std::string label, std::string description = {}) :
		    m_Label(std::move(label)),
		    m_Description(std::move(description))
		{
		}

		std::string_view GetLeftText() const override
		{
			return m_Label;
		}
		std::string_view GetDescription() const override
		{
			return m_Description;
		}

	protected:
		std::string m_Label;
		std::string m_Description;
	};
}
