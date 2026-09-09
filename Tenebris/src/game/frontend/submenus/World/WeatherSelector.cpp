#include "WeatherSelector.hpp"

#include "core/frontend/Localization.hpp"
#include "core/frontend/Notifications.hpp"
#include "core/frontend/manager/UIItem.hpp"
#include "game/backend/FiberPool.hpp"
#include "game/frontend/items/Items.hpp"
#include "game/pointers/Pointers.hpp"
#include "game/rdr/Natives.hpp"
#include "game/rdr/invoker/Invoker.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <utility>

namespace YimMenu::Submenus
{
	namespace
	{
		struct WeatherEntry
		{
			const char* Id;
			const char* Pt;
			const char* En;
		};

		struct WeatherVariantEntry
		{
			const char* Type;
			const char* Variant;
			bool SinglePlayerOnly;
		};

		constexpr std::array kWeatherEntries = {
		    WeatherEntry{"BLIZZARD", "Nevasca", "Blizzard"},
		    WeatherEntry{"CLOUDS", "Nublado", "Clouds"},
		    WeatherEntry{"DRIZZLE", "Garoa", "Drizzle"},
		    WeatherEntry{"FOG", "Nevoeiro", "Fog"},
		    WeatherEntry{"GROUNDBLIZZARD", "Nevasca baixa", "Ground Blizzard"},
		    WeatherEntry{"HAIL", "Granizo", "Hail"},
		    WeatherEntry{"HIGHPRESSURE", "Alta pressão", "High Pressure"},
		    WeatherEntry{"HURRICANE", "Furacão", "Hurricane"},
		    WeatherEntry{"MISTY", "Neblina", "Misty"},
		    WeatherEntry{"OVERCAST", "Encoberto", "Overcast"},
		    WeatherEntry{"OVERCASTDARK", "Encoberto escuro", "Dark Overcast"},
		    WeatherEntry{"RAIN", "Chuva", "Rain"},
		    WeatherEntry{"SANDSTORM", "Tempestade de areia", "Sandstorm"},
		    WeatherEntry{"SHOWER", "Pancadas de chuva", "Showers"},
		    WeatherEntry{"SLEET", "Chuva congelada", "Sleet"},
		    WeatherEntry{"SNOW", "Neve", "Snow"},
		    WeatherEntry{"SNOWLIGHT", "Neve leve", "Light Snow"},
		    WeatherEntry{"SUNNY", "Ensolarado", "Sunny"},
		    WeatherEntry{"THUNDER", "Trovões", "Thunder"},
		    WeatherEntry{"THUNDERSTORM", "Tempestade com trovões", "Thunderstorm"},
		    WeatherEntry{"WHITEOUT", "Branco total", "Whiteout"},
		};

		constexpr std::array kWeatherVariants = {
		    WeatherVariantEntry{"BLIZZARD", "BLIZZARD_winter2", false},
		    WeatherVariantEntry{"CLOUDS", "CLOUDS_mudtown3B", false},
		    WeatherVariantEntry{"DRIZZLE", "DRIZZLE_finale1", false},
		    WeatherVariantEntry{"DRIZZLE", "DRIZZLE_finale1B", false},
		    WeatherVariantEntry{"FOG", "FOG_guama", false},
		    WeatherVariantEntry{"FOG", "Fog_MP_Pred", false},
		    WeatherVariantEntry{"GROUNDBLIZZARD", "GROUNDBLIZZARD_odriscols", false},
		    WeatherVariantEntry{"GROUNDBLIZZARD", "GROUNDBLIZZARD_winter2", false},
		    WeatherVariantEntry{"HIGHPRESSURE", "HIGHPRESSURE_guama", false},
		    WeatherVariantEntry{"HURRICANE", "HURRICANE_guama", false},
		    WeatherVariantEntry{"MISTY", "MISTY_braithwaites3", false},
		    WeatherVariantEntry{"MISTY", "MISTY_finale1", false},
		    WeatherVariantEntry{"MISTY", "MISTY_finale1B", false},
		    WeatherVariantEntry{"MISTY", "MISTY_finale2", false},
		    WeatherVariantEntry{"MISTY", "MISTY_guama", false},
		    WeatherVariantEntry{"MISTY", "MISTY_MP_intro", false},
		    WeatherVariantEntry{"MISTY", "MISTY_MP_Pred", false},
		    WeatherVariantEntry{"MISTY", "MISTY_train1", false},
		    WeatherVariantEntry{"OVERCASTDARK", "OVERCASTDARK_finale2", false},
		    WeatherVariantEntry{"OVERCASTDARK", "OVERCASTDARK_Gang2", false},
		    WeatherVariantEntry{"OVERCASTDARK", "OVERCASTDARK_native3", false},
		    WeatherVariantEntry{"OVERCASTDARK", "OVERCASTDARK_STD1", false},
		    WeatherVariantEntry{"SHOWER", "SHOWER_finale2", false},
		    WeatherVariantEntry{"SHOWER", "SHOWER_guama", false},
		    WeatherVariantEntry{"SHOWER", "shower_MP_Pred", false},
		    WeatherVariantEntry{"SNOW", "SNOW_Odriscolls1", false},
		    WeatherVariantEntry{"SNOW", "SNOW_Pearson1", false},
		    WeatherVariantEntry{"SNOWLIGHT", "SNOWLIGHT_finale2", false},
		    WeatherVariantEntry{"SNOWLIGHT", "SNOWLIGHT_Odriscolls1", false},
		    WeatherVariantEntry{"SNOWLIGHT", "SNOWLIGHT_Pearson1", false},
		    WeatherVariantEntry{"SUNNY", "Sunny_odriscols4", false},
		    WeatherVariantEntry{"THUNDERSTORM", "THUNDERSTORM_MP_Pred", false},
		    WeatherVariantEntry{"THUNDERSTORM", "THUNDERSTORM_nativeSon3", false},
		    WeatherVariantEntry{"WHITEOUT", "WHITEOUT_winter1", false},
		    WeatherVariantEntry{"SNOWCLEARING", "SNOWCLEARING_mud1", true},
		    WeatherVariantEntry{"SNOWCLEARING", "SNOWCLEARING_winter4", true},
		};

		constexpr std::uint64_t kSetWeatherType = 0x59174F1AFE095B5AULL;
		constexpr std::uint64_t kSetWeatherVariation = 0x3373779BAF7CAF48ULL;
		constexpr std::uint64_t kClearWeatherVariation = 0x0E71C80FA4EC8147ULL;
		std::string g_ActiveVariantType;

		const char* WeatherLabel(const WeatherEntry& entry)
		{
			return Localization::IsPortuguese() ? entry.Pt : entry.En;
		}

		template<typename... Args>
		bool CallNative(std::uint64_t hash, Args&&... args)
		{
			if (!Pointers.GetNativeHandler)
				return false;

			auto handler = Pointers.GetNativeHandler(static_cast<rage::scrNativeHash>(hash));
			if (!handler)
				return false;

			CustomCallContext context{};
			context.reset();
			(context.PushArg(std::forward<Args>(args)), ...);
			handler(&context);
			return true;
		}

		void ClearActiveVariant()
		{
			if (g_ActiveVariantType.empty())
				return;
			CallNative(kClearWeatherVariation, g_ActiveVariantType.c_str(), true);
			g_ActiveVariantType.clear();
		}

		void ApplyWeather(const char* id)
		{
			const std::string weather = id;
			FiberPool::Push([weather] {
				ClearActiveVariant();
				MISC::CLEAR_OVERRIDE_WEATHER();
				MISC::_SET_OVERRIDE_WEATHER(MISC::GET_HASH_KEY(weather.c_str()));
				Notifications::Show("Tenebris",
				    Localization::IsPortuguese() ? "Clima aplicado." : "Weather applied.",
				    NotificationType::Success,
				    2200);
			});
		}

		void ApplyVariant(const WeatherVariantEntry& entry)
		{
			const std::string type = entry.Type;
			const std::string variant = entry.Variant;
			FiberPool::Push([type, variant] {
				ClearActiveVariant();
				MISC::CLEAR_OVERRIDE_WEATHER();

				const bool variationOk = CallNative(kSetWeatherVariation, type.c_str(), variant.c_str());
				const bool typeOk = CallNative(kSetWeatherType,
				    MISC::GET_HASH_KEY(type.c_str()), true, true, true, 3.0f, false);

				if (variationOk && typeOk)
				{
					g_ActiveVariantType = type;
					Notifications::Show("Tenebris",
					    Localization::IsPortuguese() ? "Variante de clima aplicada." : "Weather variation applied.",
					    NotificationType::Success,
					    2200);
				}
				else
				{
					Notifications::Show("Tenebris",
					    Localization::IsPortuguese() ? "Não foi possível aplicar esta variante." : "Could not apply this weather variation.",
					    NotificationType::Warning,
					    2600);
				}
			});
		}

		void RestoreWeather()
		{
			FiberPool::Push([] {
				ClearActiveVariant();
				MISC::CLEAR_OVERRIDE_WEATHER();
				Notifications::Show("Tenebris",
				    Localization::IsPortuguese() ? "Clima restaurado ao controle do jogo." : "Weather restored to game control.",
				    NotificationType::Success,
				    2200);
			});
		}

		class WeatherItem final : public UIItem
		{
		public:
			explicit WeatherItem(const WeatherEntry& entry) : m_Entry(entry)
			{
			}

			std::string_view GetMenuLabel() const override
			{
				return WeatherLabel(m_Entry);
			}

			std::string GetMenuValue() const override
			{
				return m_Entry.Id;
			}

			std::string_view GetMenuDescription() const override
			{
				return Localization::IsPortuguese() ? "Aplica este clima imediatamente." : "Applies this weather immediately.";
			}

			void HandleMenuAction(MenuAction action) override
			{
				if (action == MenuAction::Enter)
					ApplyWeather(m_Entry.Id);
			}

		private:
			WeatherEntry m_Entry;
		};

		class WeatherVariantItem final : public UIItem
		{
		public:
			explicit WeatherVariantItem(const WeatherVariantEntry& entry) : m_Entry(entry)
			{
			}

			std::string_view GetMenuLabel() const override
			{
				return m_Entry.Variant;
			}

			std::string GetMenuValue() const override
			{
				if (m_Entry.SinglePlayerOnly)
					return Localization::IsPortuguese() ? "HISTÓRIA" : "STORY";
				return {};
			}

			std::string_view GetMenuDescription() const override
			{
				return m_Entry.SinglePlayerOnly
				    ? (Localization::IsPortuguese() ? "Variante documentada como exclusiva do modo História." : "Variation documented as Story Mode only.")
				    : (Localization::IsPortuguese() ? "Aplica esta variante de clima." : "Applies this weather variation.");
			}

			void HandleMenuAction(MenuAction action) override
			{
				if (action == MenuAction::Enter)
					ApplyVariant(m_Entry);
			}

		private:
			WeatherVariantEntry m_Entry;
		};

		class WeatherSectionItem final : public UIItem
		{
		public:
			std::string_view GetMenuLabel() const override
			{
				return Localization::IsPortuguese() ? "--- Variantes de clima ---" : "--- Weather variants ---";
			}

			std::string_view GetMenuDescription() const override
			{
				return Localization::IsPortuguese() ? "Lista completa de variantes documentadas." : "Complete list of documented weather variations.";
			}
		};

		class RestoreWeatherItem final : public UIItem
		{
		public:
			std::string_view GetMenuLabel() const override
			{
				return Localization::IsPortuguese() ? "Restaurar clima automático" : "Restore automatic weather";
			}

			void HandleMenuAction(MenuAction action) override
			{
				if (action == MenuAction::Enter)
					RestoreWeather();
			}
		};
	}

	std::shared_ptr<UIItem> CreateWeatherSelectorItem()
	{
		auto list = std::make_shared<Group>("", 1);
		for (const auto& weather : kWeatherEntries)
			list->AddItem(std::make_shared<WeatherItem>(weather));
		list->AddItem(std::make_shared<RestoreWeatherItem>());
		list->AddItem(std::make_shared<WeatherSectionItem>());
		for (const auto& variant : kWeatherVariants)
			list->AddItem(std::make_shared<WeatherVariantItem>(variant));
		return list;
	}
}
