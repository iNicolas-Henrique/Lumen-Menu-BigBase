#pragma once

#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace YimMenu
{
	// Safe integration point for a future asynchronous GeoIP provider. Tenebris does
	// not transmit player addresses to a third party without an explicitly
	// configured provider; unresolved and local addresses are cached as unknown.
	class GeoIPCountry
	{
	public:
		static std::string Get(std::string_view ip)
		{
			static std::mutex mutex;
			static std::unordered_map<std::string, std::string> cache;
			std::scoped_lock lock(mutex);
			auto [entry, inserted] = cache.try_emplace(std::string(ip), "Desconhecida");
			return entry->second;
		}
	};
}
