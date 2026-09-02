#pragma once

#include "AbstractOption.hpp"

#include <memory>

namespace YimMenu::Classic
{
	class AbstractSubmenu
	{
	public:
		virtual ~AbstractSubmenu() = default;
		virtual std::string_view GetName() const = 0;
		virtual const std::vector<std::shared_ptr<AbstractOption>>& GetOptions() const = 0;
	};

	class BaseSubmenu : public AbstractSubmenu
	{
	public:
		explicit BaseSubmenu(std::string name) :
		    m_Name(std::move(name))
		{
		}
		std::string_view GetName() const override
		{
			return m_Name;
		}
		const std::vector<std::shared_ptr<AbstractOption>>& GetOptions() const override
		{
			return m_Options;
		}
		void AddOption(std::shared_ptr<AbstractOption> option)
		{
			m_Options.push_back(std::move(option));
		}

	protected:
		std::string m_Name;
		std::vector<std::shared_ptr<AbstractOption>> m_Options;
	};

	class RegularSubmenu final : public BaseSubmenu
	{
	public:
		using BaseSubmenu::BaseSubmenu;
	};
}
