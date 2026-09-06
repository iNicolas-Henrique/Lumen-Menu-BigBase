#include "WeatherSelector.hpp"

#include "core/frontend/Localization.hpp"
#include "core/frontend/Notifications.hpp"
#include "core/frontend/manager/UIItem.hpp"
#include "game/backend/FiberPool.hpp"
#include "game/rdr/Natives.hpp"

#include <algorithm>
#include <array>
#include <string>

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

		const char* WeatherLabel(const WeatherEntry& entry)
		{
			return Localization::IsPortuguese() ? entry.Pt : entry.En;
		}

		void ApplyWeather(int index)
		{
			index = std::clamp(index, 0, static_cast<int>(kWeatherEntries.size()) - 1);
			const std::string weather = kWeatherEntries[index].Id;
			FiberPool::Push([weather] {
				MISC::_SET_OVERRIDE_WEATHER(MISC::GET_HASH_KEY(weather.c_str()));
				Notifications::Show("Tenebris",
				    Localization::IsPortuguese() ? "Clima aplicado." : "Weather applied.",
				    NotificationType::Success,
				    2200);
			});
		}

		void RestoreWeather()
		{
			FiberPool::Push([] {
				MISC::CLEAR_OVERRIDE_WEATHER();
				Notifications::Show("Tenebris",
				    Localization::IsPortuguese() ? "Clima restaurado ao controle do jogo." : "Weather restored to game control.",
				    NotificationType::Success,
				    2200);
			});
		}

		class WeatherSelectorItem final : public UIItem
		{
		public:
			void Draw() override
			{
				ImGui::TextDisabled("%s", Localization::IsPortuguese()
				        ? "Selecione um clima e pressione Enter ou Aplicar."
				        : "Select a weather type and press Enter or Apply.");
				ImGui::Separator();
				if (ImGui::BeginChild("##weather_list", ImVec2(0.0f, 420.0f), true))
				{
					for (int i = 0; i < static_cast<int>(kWeatherEntries.size()); ++i)
					{
						const auto& entry = kWeatherEntries[i];
						const std::string row = std::string(WeatherLabel(entry)) + "##" + entry.Id;
						if (ImGui::Selectable(row.c_str(), m_Selected == i))
							m_Selected = i;
						if (m_Selected == i && m_ScrollSelected)
						{
							ImGui::SetScrollHereY(0.5f);
							m_ScrollSelected = false;
						}
					}
				}
				ImGui::EndChild();
				ImGui::TextDisabled("ID: %s", kWeatherEntries[m_Selected].Id);
				if (ImGui::Button(Localization::IsPortuguese() ? "Aplicar" : "Apply", ImVec2(-FLT_MIN, 0.0f)))
					ApplyWeather(m_Selected);
				if (ImGui::Button(Localization::IsPortuguese() ? "Restaurar" : "Restore", ImVec2(-FLT_MIN, 0.0f)))
					RestoreWeather();
			}

			std::string_view GetMenuLabel() const override { return "Clima do mundo"; }
			std::string_view GetMenuDescription() const override
			{
				return "Mostra todos os climas disponíveis; permite aplicar o selecionado ou devolver o clima ao controle normal do jogo.";
			}
			bool RequiresImGuiEditor() const override { return true; }
			float GetPreferredEditorHeight() const override { return 610.0f; }

			bool HandleEditorKey(int key) override
			{
				const int count = static_cast<int>(kWeatherEntries.size());
				if (key == VK_UP)
				{
					m_Selected = m_Selected <= 0 ? count - 1 : m_Selected - 1;
					m_ScrollSelected = true;
					return true;
				}
				if (key == VK_DOWN)
				{
					m_Selected = (m_Selected + 1) % count;
					m_ScrollSelected = true;
					return true;
				}
				if (key == VK_RETURN)
				{
					ApplyWeather(m_Selected);
					return true;
				}
				return false;
			}

		private:
			int m_Selected = 0;
			bool m_ScrollSelected = false;
		};
	}

	std::shared_ptr<UIItem> CreateWeatherSelectorItem()
	{
		return std::make_shared<WeatherSelectorItem>();
	}
}
