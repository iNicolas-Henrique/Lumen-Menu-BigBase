#pragma once

#include <string>
#include <string_view>

namespace YimMenu::Localization
{
	enum class Language
	{
		Portuguese,
		English
	};

	void SetLanguage(Language language);
	Language GetLanguage();
	bool IsPortuguese();
	const char* GetLanguageCode();
	Language FromLanguageCode(std::string_view code);

	// Translates visible menu text. Unknown strings are intentionally kept as-is
	// so new features never disappear just because a translation is missing.
	std::string Text(std::string_view text);
}
