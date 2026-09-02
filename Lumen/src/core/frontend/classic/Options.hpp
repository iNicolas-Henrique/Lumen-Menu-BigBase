#pragma once

#include "BaseOption.hpp"

#include <algorithm>

namespace YimMenu::Classic
{
	class RegularOption : public BaseOption
	{
	public:
		RegularOption(std::string label, std::function<void()> callback, std::string description = {}) :
		    BaseOption(std::move(label), std::move(description)),
		    m_Callback(std::move(callback))
		{
		}
		void HandleAction(OptionAction action) override
		{
			if (action == OptionAction::Enter && m_Callback)
				m_Callback();
		}

	private:
		std::function<void()> m_Callback;
	};

	class BoolOption : public BaseOption
	{
	public:
		BoolOption(std::string label, bool& value, std::string description = {}) :
		    BaseOption(std::move(label), std::move(description)),
		    m_Value(value)
		{
		}
		std::string GetRightText() const override
		{
			return m_Value ? "ATIVADO" : "DESATIVADO";
		}
		void HandleAction(OptionAction) override
		{
			m_Value = !m_Value;
		}

	private:
		bool& m_Value;
	};

	template<typename T>
	class NumberOption : public BaseOption
	{
	public:
		NumberOption(std::string label, T& value, T minimum, T maximum, T step, std::string description = {}) :
		    BaseOption(std::move(label), std::move(description)),
		    m_Value(value),
		    m_Minimum(minimum),
		    m_Maximum(maximum),
		    m_Step(step)
		{
		}
		std::string GetRightText() const override
		{
			return std::to_string(m_Value);
		}
		void HandleAction(OptionAction action) override
		{
			if (action == OptionAction::Left)
				m_Value = std::max(m_Minimum, static_cast<T>(m_Value - m_Step));
			if (action == OptionAction::Right)
				m_Value = std::min(m_Maximum, static_cast<T>(m_Value + m_Step));
		}

	private:
		T& m_Value;
		T m_Minimum, m_Maximum, m_Step;
	};

	class ChooseOption : public BaseOption
	{
	public:
		ChooseOption(std::string label, std::vector<std::string> values, std::size_t& selected, std::string description = {}) :
		    BaseOption(std::move(label), std::move(description)),
		    m_Values(std::move(values)),
		    m_Selected(selected)
		{
		}
		std::string GetRightText() const override
		{
			return m_Values.empty() ? std::string{} : m_Values[m_Selected % m_Values.size()];
		}
		void HandleAction(OptionAction action) override
		{
			if (m_Values.empty())
				return;
			m_Selected = action == OptionAction::Left && m_Selected == 0 ? m_Values.size() - 1 :
			    action == OptionAction::Left                             ? m_Selected - 1 :
			                                                               (m_Selected + 1) % m_Values.size();
		}

	private:
		std::vector<std::string> m_Values;
		std::size_t& m_Selected;
	};

	class SubOption : public RegularOption
	{
	public:
		using RegularOption::RegularOption;
		std::string GetRightText() const override
		{
			return ">";
		}
	};
}
