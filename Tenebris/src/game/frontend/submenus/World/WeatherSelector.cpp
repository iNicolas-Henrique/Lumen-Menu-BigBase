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
			const char* Pt;
			const char* En;
			bool SinglePlayerOnly;
		};

		constexpr std::array kWeatherEntries = {
		    WeatherEntry{"BLIZZARD", "Nevasca", "Blizzard"}, WeatherEntry{"CLOUDS", "Nublado", "Clouds"},
		    WeatherEntry{"DRIZZLE", "Garoa", "Drizzle"}, WeatherEntry{"FOG", "Nevoeiro", "Fog"},
		    WeatherEntry{"GROUNDBLIZZARD", "Nevasca baixa", "Ground Blizzard"}, WeatherEntry{"HAIL", "Granizo", "Hail"},
		    WeatherEntry{"HIGHPRESSURE", "Alta pressão", "High Pressure"}, WeatherEntry{"HURRICANE", "Furacão", "Hurricane"},
		    WeatherEntry{"MISTY", "Neblina", "Misty"}, WeatherEntry{"OVERCAST", "Encoberto", "Overcast"},
		    WeatherEntry{"OVERCASTDARK", "Encoberto escuro", "Dark Overcast"}, WeatherEntry{"RAIN", "Chuva", "Rain"},
		    WeatherEntry{"SANDSTORM", "Tempestade de areia", "Sandstorm"}, WeatherEntry{"SHOWER", "Pancadas de chuva", "Showers"},
		    WeatherEntry{"SLEET", "Chuva congelada", "Sleet"}, WeatherEntry{"SNOW", "Neve", "Snow"},
		    WeatherEntry{"SNOWLIGHT", "Neve leve", "Light Snow"}, WeatherEntry{"SUNNY", "Ensolarado", "Sunny"},
		    WeatherEntry{"THUNDER", "Trovões", "Thunder"}, WeatherEntry{"THUNDERSTORM", "Tempestade com trovões", "Thunderstorm"},
		    WeatherEntry{"WHITEOUT", "Branco total", "Whiteout"},
		};

		constexpr std::array kWeatherVariants = {
		    WeatherVariantEntry{"BLIZZARD", "BLIZZARD_winter2", "Nevasca — Inverno 2", "Blizzard — Winter 2", false},
		    WeatherVariantEntry{"CLOUDS", "CLOUDS_mudtown3B", "Nublado — Mudtown 3B", "Clouds — Mudtown 3B", false},
		    WeatherVariantEntry{"DRIZZLE", "DRIZZLE_finale1", "Garoa — Finale 1", "Drizzle — Finale 1", false},
		    WeatherVariantEntry{"DRIZZLE", "DRIZZLE_finale1B", "Garoa — Finale 1B", "Drizzle — Finale 1B", false},
		    WeatherVariantEntry{"FOG", "FOG_guama", "Nevoeiro — Guarma", "Fog — Guarma", false},
		    WeatherVariantEntry{"FOG", "Fog_MP_Pred", "Nevoeiro — Online Predador", "Fog — Online Predator", false},
		    WeatherVariantEntry{"GROUNDBLIZZARD", "GROUNDBLIZZARD_odriscols", "Nevasca baixa — O'Driscolls", "Ground Blizzard — O'Driscolls", false},
		    WeatherVariantEntry{"GROUNDBLIZZARD", "GROUNDBLIZZARD_winter2", "Nevasca baixa — Inverno 2", "Ground Blizzard — Winter 2", false},
		    WeatherVariantEntry{"HIGHPRESSURE", "HIGHPRESSURE_guama", "Alta pressão — Guarma", "High Pressure — Guarma", false},
		    WeatherVariantEntry{"HURRICANE", "HURRICANE_guama", "Furacão — Guarma", "Hurricane — Guarma", false},
		    WeatherVariantEntry{"MISTY", "MISTY_braithwaites3", "Neblina — Braithwaites 3", "Misty — Braithwaites 3", false},
		    WeatherVariantEntry{"MISTY", "MISTY_finale1", "Neblina — Finale 1", "Misty — Finale 1", false},
		    WeatherVariantEntry{"MISTY", "MISTY_finale1B", "Neblina — Finale 1B", "Misty — Finale 1B", false},
		    WeatherVariantEntry{"MISTY", "MISTY_finale2", "Neblina — Finale 2", "Misty — Finale 2", false},
		    WeatherVariantEntry{"MISTY", "MISTY_guama", "Neblina — Guarma", "Misty — Guarma", false},
		    WeatherVariantEntry{"MISTY", "MISTY_MP_intro", "Neblina — Introdução Online", "Misty — Online Intro", false},
		    WeatherVariantEntry{"MISTY", "MISTY_MP_Pred", "Neblina — Online Predador", "Misty — Online Predator", false},
		    WeatherVariantEntry{"MISTY", "MISTY_train1", "Neblina — Trem 1", "Misty — Train 1", false},
		    WeatherVariantEntry{"OVERCASTDARK", "OVERCASTDARK_finale2", "Encoberto escuro — Finale 2", "Dark Overcast — Finale 2", false},
		    WeatherVariantEntry{"OVERCASTDARK", "OVERCASTDARK_Gang2", "Encoberto escuro — Gangue 2", "Dark Overcast — Gang 2", false},
		    WeatherVariantEntry{"OVERCASTDARK", "OVERCASTDARK_native3", "Encoberto escuro — Nativos 3", "Dark Overcast — Native 3", false},
		    WeatherVariantEntry{"OVERCASTDARK", "OVERCASTDARK_STD1", "Encoberto escuro — Padrão 1", "Dark Overcast — Standard 1", false},
		    WeatherVariantEntry{"SHOWER", "SHOWER_finale2", "Pancadas de chuva — Finale 2", "Showers — Finale 2", false},
		    WeatherVariantEntry{"SHOWER", "SHOWER_guama", "Pancadas de chuva — Guarma", "Showers — Guarma", false},
		    WeatherVariantEntry{"SHOWER", "shower_MP_Pred", "Pancadas de chuva — Online Predador", "Showers — Online Predator", false},
		    WeatherVariantEntry{"SNOW", "SNOW_Odriscolls1", "Neve — O'Driscolls 1", "Snow — O'Driscolls 1", false},
		    WeatherVariantEntry{"SNOW", "SNOW_Pearson1", "Neve — Pearson 1", "Snow — Pearson 1", false},
		    WeatherVariantEntry{"SNOWLIGHT", "SNOWLIGHT_finale2", "Neve leve — Finale 2", "Light Snow — Finale 2", false},
		    WeatherVariantEntry{"SNOWLIGHT", "SNOWLIGHT_Odriscolls1", "Neve leve — O'Driscolls 1", "Light Snow — O'Driscolls 1", false},
		    WeatherVariantEntry{"SNOWLIGHT", "SNOWLIGHT_Pearson1", "Neve leve — Pearson 1", "Light Snow — Pearson 1", false},
		    WeatherVariantEntry{"SUNNY", "Sunny_odriscols4", "Ensolarado — O'Driscolls 4", "Sunny — O'Driscolls 4", false},
		    WeatherVariantEntry{"THUNDERSTORM", "THUNDERSTORM_MP_Pred", "Tempestade — Online Predador", "Thunderstorm — Online Predator", false},
		    WeatherVariantEntry{"THUNDERSTORM", "THUNDERSTORM_nativeSon3", "Tempestade — Native Son 3", "Thunderstorm — Native Son 3", false},
		    WeatherVariantEntry{"WHITEOUT", "WHITEOUT_winter1", "Branco total — Inverno 1", "Whiteout — Winter 1", false},
		    WeatherVariantEntry{"SNOWCLEARING", "SNOWCLEARING_mud1", "Neve dissipando — Mud 1", "Snow Clearing — Mud 1", true},
		    WeatherVariantEntry{"SNOWCLEARING", "SNOWCLEARING_winter4", "Neve dissipando — Inverno 4", "Snow Clearing — Winter 4", true},
		};

		constexpr std::uint64_t kSetWeatherType = 0x59174F1AFE095B5AULL;
		constexpr std::uint64_t kSetWeatherVariation = 0x3373779BAF7CAF48ULL;
		constexpr std::uint64_t kClearWeatherVariation = 0x0E71C80FA4EC8147ULL;
		std::string g_ActiveVariantType;

		const char* WeatherLabel(const WeatherEntry& entry) { return Localization::IsPortuguese() ? entry.Pt : entry.En; }
		const char* VariantLabel(const WeatherVariantEntry& entry) { return Localization::IsPortuguese() ? entry.Pt : entry.En; }

		template<typename... Args>
		bool CallNative(std::uint64_t hash, Args&&... args)
		{
			if (!Pointers.GetNativeHandler) return false;
			auto handler = Pointers.GetNativeHandler(static_cast<rage::scrNativeHash>(hash));
			if (!handler) return false;
			CustomCallContext context{};
			context.reset();
			(context.PushArg(std::forward<Args>(args)), ...);
			handler(&context);
			return true;
		}

		void ClearActiveVariant()
		{
			if (g_ActiveVariantType.empty()) return;
			CallNative(kClearWeatherVariation, g_ActiveVariantType.c_str(), true);
			g_ActiveVariantType.clear();
		}

		void ApplyWeather(const char* id)
		{
			const std::string weather = id;
			FiberPool::Push([weather] {
				ClearActiveVariant(); MISC::CLEAR_OVERRIDE_WEATHER();
				MISC::_SET_OVERRIDE_WEATHER(MISC::GET_HASH_KEY(weather.c_str()));
				Notifications::Show("Tenebris", Localization::IsPortuguese() ? "Clima aplicado." : "Weather applied.", NotificationType::Success, 2200);
			});
		}

		void ApplyVariant(const WeatherVariantEntry& entry)
		{
			const std::string type = entry.Type, variant = entry.Variant;
			FiberPool::Push([type, variant] {
				ClearActiveVariant(); MISC::CLEAR_OVERRIDE_WEATHER();
				const bool variationOk = CallNative(kSetWeatherVariation, type.c_str(), variant.c_str());
				const bool typeOk = CallNative(kSetWeatherType, MISC::GET_HASH_KEY(type.c_str()), true, true, true, 3.0f, false);
				if (variationOk && typeOk) {
					g_ActiveVariantType = type;
					Notifications::Show("Tenebris", Localization::IsPortuguese() ? "Variante de clima aplicada." : "Weather variation applied.", NotificationType::Success, 2200);
				} else {
					Notifications::Show("Tenebris", Localization::IsPortuguese() ? "Não foi possível aplicar esta variante." : "Could not apply this weather variation.", NotificationType::Warning, 2600);
				}
			});
		}

		void RestoreWeather()
		{
			FiberPool::Push([] {
				ClearActiveVariant(); MISC::CLEAR_OVERRIDE_WEATHER();
				Notifications::Show("Tenebris", Localization::IsPortuguese() ? "Clima restaurado ao controle do jogo." : "Weather restored to game control.", NotificationType::Success, 2200);
			});
		}

		class WeatherItem final : public UIItem
		{
			WeatherEntry m_Entry;
		public:
			explicit WeatherItem(const WeatherEntry& entry) : m_Entry(entry) {}
			void Draw() override {}
			std::string_view GetMenuLabel() const override { return WeatherLabel(m_Entry); }
			std::string GetMenuValue() const override { return m_Entry.Id; }
			std::string_view GetMenuDescription() const override { return Localization::IsPortuguese() ? "Aplica este clima imediatamente." : "Applies this weather immediately."; }
			void HandleMenuAction(MenuAction action) override { if (action == MenuAction::Enter) ApplyWeather(m_Entry.Id); }
		};

		class WeatherVariantItem final : public UIItem
		{
			WeatherVariantEntry m_Entry;
		public:
			explicit WeatherVariantItem(const WeatherVariantEntry& entry) : m_Entry(entry) {}
			void Draw() override {}
			std::string_view GetMenuLabel() const override { return VariantLabel(m_Entry); }
			std::string GetMenuValue() const override { return m_Entry.SinglePlayerOnly ? (Localization::IsPortuguese() ? "HISTÓRIA" : "STORY") : std::string{}; }
			std::string_view GetMenuDescription() const override {
				return m_Entry.SinglePlayerOnly ? (Localization::IsPortuguese() ? "Variante documentada como exclusiva do modo História." : "Variation documented as Story Mode only.") : (Localization::IsPortuguese() ? "Aplica esta variante de clima." : "Applies this weather variation.");
			}
			void HandleMenuAction(MenuAction action) override { if (action == MenuAction::Enter) ApplyVariant(m_Entry); }
		};

		class WeatherSectionItem final : public UIItem
		{
		public:
			void Draw() override {}
			std::string_view GetMenuLabel() const override { return Localization::IsPortuguese() ? "--- Variantes de clima ---" : "--- Weather variants ---"; }
			std::string_view GetMenuDescription() const override { return Localization::IsPortuguese() ? "Lista completa de variantes documentadas." : "Complete list of documented weather variations."; }
		};

		class RestoreWeatherItem final : public UIItem
		{
		public:
			void Draw() override {}
			std::string_view GetMenuLabel() const override { return Localization::IsPortuguese() ? "Restaurar clima automático" : "Restore automatic weather"; }
			void HandleMenuAction(MenuAction action) override { if (action == MenuAction::Enter) RestoreWeather(); }
		};
	}

	std::shared_ptr<UIItem> CreateWeatherSelectorItem()
	{
		auto list = std::make_shared<Group>("", 1);
		for (const auto& weather : kWeatherEntries) list->AddItem(std::make_shared<WeatherItem>(weather));
		list->AddItem(std::make_shared<RestoreWeatherItem>());
		list->AddItem(std::make_shared<WeatherSectionItem>());
		for (const auto& variant : kWeatherVariants) list->AddItem(std::make_shared<WeatherVariantItem>(variant));
		return list;
	}
}
