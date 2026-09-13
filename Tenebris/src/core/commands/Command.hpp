#pragma once
#include "core/frontend/Localization.hpp"
#include "util/Joaat.hpp"

#include <nlohmann/json.hpp>

namespace YimMenu
{
	class Command
	{
	private:
		std::string m_Name;
		std::string m_Label;
		std::string m_Description;
		std::string m_EnglishDescription;
		joaat_t m_Hash;
		int m_NumArgs = 0;

	protected:
		virtual void OnCall() = 0;
		void MarkDirty();

	public:
		Command(std::string name, std::string label, std::string description, int num_args = 0);
		void Call();

		virtual void SaveState(nlohmann::json& value){}
		virtual void LoadState(nlohmann::json& value){}

		const std::string& GetName() { return m_Name; }
		const std::string& GetLabel() { return m_Label; }
		const std::string& GetDescription()
		{
			return Localization::IsPortuguese() ? m_Description : m_EnglishDescription;
		}
		joaat_t GetHash() { return m_Hash; }
	};
}
