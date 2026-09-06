#include "VehicleSpawner.hpp"

#include "core/frontend/Localization.hpp"
#include "core/frontend/Notifications.hpp"
#include "core/frontend/manager/UIItem.hpp"
#include "game/backend/FiberPool.hpp"
#include "game/backend/Self.hpp"
#include "game/rdr/Natives.hpp"
#include "game/rdr/Vehicle.hpp"
#include "game/rdr/data/VehicleModels.hpp"
#include "util/Joaat.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace YimMenu::Submenus
{
	namespace
	{
		std::string Lower(std::string_view value)
		{
			std::string result(value);
			std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
			return result;
		}

		void SpawnSelectedVehicle(int index)
		{
			constexpr int count = static_cast<int>(std::size(Data::g_VehicleModels));
			if (index < 0 || index >= count)
				return;
			const auto entry = Data::g_VehicleModels[index];
			const Hash model = Joaat(entry.model);
			if (!STREAMING::IS_MODEL_VALID(model))
			{
				Notifications::Show("Tenebris", Localization::IsPortuguese() ? "O modelo de veículo selecionado é inválido." : "The selected vehicle model is invalid.", NotificationType::Error);
				return;
			}

			FiberPool::Push([model, name = entry.name] {
				auto coords = Self::GetPed().GetPosition();
				coords.x += 5.0f;
				const auto vehicle = Vehicle::Create(model, coords, Self::GetPed().GetRotation().z);
				if (!vehicle)
				{
					Notifications::Show("Tenebris", Localization::IsPortuguese() ? "Não foi possível criar o veículo selecionado." : "Could not spawn the selected vehicle.", NotificationType::Error);
					return;
				}
				Notifications::Show("Tenebris",
				    Localization::IsPortuguese() ? std::format("Veículo criado: {}", name) : std::format("Vehicle spawned: {}", name),
				    NotificationType::Success,
				    2600);
			});
		}

		class VehicleSpawnerItem final : public UIItem
		{
		public:
			void Draw() override
			{
				constexpr int count = static_cast<int>(std::size(Data::g_VehicleModels));
				m_Selected = std::clamp(m_Selected, 0, count - 1);
				ImGui::TextDisabled("%s", Localization::IsPortuguese()
				        ? "Selecione um veículo e pressione Enter ou o botão Criar."
				        : "Select a vehicle and press Enter or the Spawn button.");
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::InputTextWithHint("##vehicle_filter", Localization::IsPortuguese() ? "Pesquisar (opcional)" : "Search (optional)", m_Filter, sizeof(m_Filter));
				ImGui::TextDisabled(Localization::IsPortuguese() ? "%d veículos disponíveis" : "%d available vehicles", count);
				ImGui::Separator();

				const std::string filter = Lower(m_Filter);
				if (ImGui::BeginChild("##vehicle_list", ImVec2(0.0f, 430.0f), true))
				{
					for (int i = 0; i < count; ++i)
					{
						const auto& entry = Data::g_VehicleModels[i];
						if (!filter.empty() && Lower(entry.name + " " + entry.model).find(filter) == std::string::npos)
							continue;
						const std::string row = entry.name + "##" + entry.model;
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

				ImGui::TextDisabled("%s", Data::g_VehicleModels[m_Selected].model.c_str());
				if (ImGui::Button(Localization::IsPortuguese() ? "Criar" : "Spawn", ImVec2(-FLT_MIN, 0.0f)))
					SpawnSelectedVehicle(m_Selected);
			}

			std::string_view GetMenuLabel() const override { return "Criar veículo"; }
			std::string_view GetMenuDescription() const override
			{
				return "Mostra todos os veículos conhecidos para selecionar e criar sem digitar o nome do modelo.";
			}
			bool RequiresImGuiEditor() const override { return true; }
			float GetPreferredEditorHeight() const override { return 620.0f; }

			bool HandleEditorKey(int key) override
			{
				constexpr int count = static_cast<int>(std::size(Data::g_VehicleModels));
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
					SpawnSelectedVehicle(m_Selected);
					return true;
				}
				return false;
			}

		private:
			int m_Selected = 0;
			bool m_ScrollSelected = false;
			char m_Filter[96]{};
		};
	}

	std::shared_ptr<UIItem> CreateVehicleSpawnerItem()
	{
		return std::make_shared<VehicleSpawnerItem>();
	}
}
