#include "World.hpp"

#include "World/Shows.hpp"
#include "World/SimplePedSpawner.hpp"
#include "World/Train.hpp"
#include "World/VehicleSpawner.hpp"
#include "World/Weather.hpp"
#include "World/WeatherSelector.hpp"
#include "core/commands/Commands.hpp"
#include "core/commands/HotkeySystem.hpp"
#include "core/commands/LoopedCommand.hpp"
#include "core/frontend/Localization.hpp"
#include "game/backend/FiberPool.hpp"
#include "game/backend/ScriptMgr.hpp"
#include "game/backend/Self.hpp"
#include "game/frontend/items/Items.hpp"
#include "game/rdr/Natives.hpp"
#include "game/rdr/Pools.hpp"

#include <rage/fwBasePool.hpp>
#include <rage/pools.hpp>

namespace YimMenu::Submenus
{
	void DisplayCurrentDate()
	{
		const auto hours = CLOCK::GET_CLOCK_HOURS();
		const auto minutes = CLOCK::GET_CLOCK_MINUTES();
		const auto seconds = CLOCK::GET_CLOCK_SECONDS();
		const auto dayOfWeek = CLOCK::GET_CLOCK_DAY_OF_WEEK();
		const auto dayOfMonth = CLOCK::GET_CLOCK_DAY_OF_MONTH();
		const auto month = CLOCK::GET_CLOCK_MONTH();
		const auto year = CLOCK::GET_CLOCK_YEAR();

		static const char* daysPt[] = {"Domingo", "Segunda-feira", "Terça-feira", "Quarta-feira", "Quinta-feira", "Sexta-feira", "Sábado"};
		static const char* monthsPt[] = {"Janeiro", "Fevereiro", "Março", "Abril", "Maio", "Junho", "Julho", "Agosto", "Setembro", "Outubro", "Novembro", "Dezembro"};
		static const char* daysEn[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
		static const char* monthsEn[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};

		const char* day = Localization::IsPortuguese() ? daysPt[dayOfWeek] : daysEn[dayOfWeek];
		const char* monthName = Localization::IsPortuguese() ? monthsPt[month] : monthsEn[month];
		const std::string dateString = Localization::IsPortuguese()
		    ? std::format("{}, {} de {} de {}", day, dayOfMonth, monthName, year)
		    : std::format("{}, {} {}, {}", day, monthName, dayOfMonth, year);
		const std::string time = std::format("{:02}:{:02}:{:02}", hours, minutes, seconds);

		ImGui::Text("%s: %s", Localization::IsPortuguese() ? "Data" : "Date", dateString.c_str());
		ImGui::Text("%s: %s", Localization::IsPortuguese() ? "Hora" : "Time", time.c_str());
	}

	World::World() :
	    Submenu::Submenu("Mundo")
	{
		auto main = std::make_shared<Category>("Principal");
		auto weather = std::make_shared<Category>("Clima");
		auto shows = std::make_shared<Category>("Espetáculos");
		auto time = std::make_shared<Category>("Horário");

		time->AddItem(std::make_shared<ImGuiItem>([] {
			static int hour, minute, second;
			static bool freeze;
			DisplayCurrentDate();
			ImGui::SliderInt(Localization::IsPortuguese() ? "Hora" : "Hour", &hour, 0, 23);
			ImGui::SliderInt(Localization::IsPortuguese() ? "Minuto" : "Minute", &minute, 0, 59);
			ImGui::SliderInt(Localization::IsPortuguese() ? "Segundo" : "Second", &second, 0, 59);
			ImGui::Checkbox(Localization::IsPortuguese() ? "Congelar horário" : "Freeze time", &freeze);
			if (ImGui::Button(Localization::IsPortuguese() ? "Aplicar horário" : "Apply time"))
			{
				FiberPool::Push([] { ChangeTime(hour, minute, second, 0, freeze); });
			}
			if (ImGui::Button(Localization::IsPortuguese() ? "Restaurar" : "Restore"))
			{
				FiberPool::Push([] { NETWORK::_NETWORK_CLEAR_CLOCK_OVERRIDE_OVERTIME(0); });
			}
		}, "Horário do mundo", "Define a hora do mundo, permite congelar o relógio e restaurar o controle normal do jogo.", 360.0f));

		weather->AddItem(CreateWeatherSelectorItem());

		auto spawners = std::make_shared<Category>("Criadores");
		auto pedSpawnerGroup = std::make_shared<Group>("PEDs", 1);
		auto vehicleSpawnerGroup = std::make_shared<Group>("Veículos", 1);
		auto trainSpawnerGroup = std::make_shared<Group>("Trens", 1);

		pedSpawnerGroup->AddItem(CreateHumanPedSpawnerItem());
		pedSpawnerGroup->AddItem(CreateAnimalPedSpawnerItem());
		vehicleSpawnerGroup->AddItem(CreateVehicleSpawnerItem());
		trainSpawnerGroup->AddItem(std::make_shared<ImGuiItem>([] { RenderTrainsMenu(); },
		    "Criar trem", "Abre a lista e os controles usados para criar e controlar trens."));

		spawners->AddItem(pedSpawnerGroup);
		spawners->AddItem(vehicleSpawnerGroup);
		spawners->AddItem(trainSpawnerGroup);

		auto poolCounter = std::make_shared<ImGuiItem>([] {
			if (GetPedPool())
				ImGui::Text("%s", std::format("PEDs: {}/{}", GetPedPool()->m_Size - GetPedPool()->GetNumFreeSlots(), GetPedPool()->m_Size).data());
			if (GetVehiclePool())
				ImGui::Text("%s", std::format("{}: {}/{}", Localization::IsPortuguese() ? "Veículos" : "Vehicles", GetVehiclePool()->m_Size - GetVehiclePool()->GetNumFreeSlots(), GetVehiclePool()->m_Size).data());
			if (GetObjectPool())
				ImGui::Text("%s", std::format("{}: {}/{}", Localization::IsPortuguese() ? "Objetos" : "Objects", GetObjectPool()->m_Size - GetObjectPool()->GetNumFreeSlots(), GetObjectPool()->m_Size).data());
		}, "Uso dos pools", "Mostra quantos PEDs, veículos e objetos estão carregados nos pools do jogo.", 300.0f);

		auto killPeds = std::make_shared<Group>("Eliminar", 1);
		killPeds->AddItem(std::make_shared<CommandItem>("killallpeds"_J));
		killPeds->AddItem(std::make_shared<CommandItem>("killallenemies"_J));
		auto deleteOpts = std::make_shared<Group>("Excluir", 1);
		deleteOpts->AddItem(std::make_shared<CommandItem>("delpeds"_J));
		deleteOpts->AddItem(std::make_shared<CommandItem>("delvehs"_J));
		deleteOpts->AddItem(std::make_shared<CommandItem>("delobjs"_J));
		auto bringOpts = std::make_shared<Group>("Trazer", 1);
		bringOpts->AddItem(std::make_shared<CommandItem>("bringpeds"_J));
		bringOpts->AddItem(std::make_shared<CommandItem>("bringvehs"_J));
		bringOpts->AddItem(std::make_shared<CommandItem>("bringobjs"_J));
		auto minigames = std::make_shared<Group>("Minijogos", 1);
		minigames->AddItem(std::make_shared<BoolCommandItem>("undeadnightmare"_J));
		minigames->AddItem(std::make_shared<ConditionalItem>("undeadnightmare"_J, std::make_shared<BoolCommandItem>("zombieslogging"_J)));
		minigames->AddItem(std::make_shared<ConditionalItem>("undeadnightmare"_J, std::make_shared<BoolCommandItem>("hardmode"_J)));
		auto misc = std::make_shared<Group>("Diversos");
		misc->AddItem(std::make_shared<BoolCommandItem>("disableguardzones"_J));
		auto eventOverride = std::make_shared<Group>("", 1);
		eventOverride->AddItem(std::make_shared<BoolCommandItem>("eventoverrideenabled"_J));
		eventOverride->AddItem(std::make_shared<ConditionalItem>("eventoverrideenabled"_J, std::make_shared<ListCommandItem>("eventoverride"_J)));
		misc->AddItem(std::move(eventOverride));
		misc->AddItem(std::make_shared<CommandItem>("mapeditor"_J));

		main->AddItem(std::move(poolCounter));
		main->AddItem(std::move(killPeds));
		main->AddItem(std::move(deleteOpts));
		main->AddItem(std::move(bringOpts));
		main->AddItem(std::move(minigames));
		main->AddItem(std::move(misc));

		shows->AddItem(std::make_shared<ImGuiItem>([] { RenderShowsMenu(); },
		    "Teatro e espetáculos", "Permite escolher e controlar apresentações e espetáculos existentes no mundo."));

		AddCategory(std::move(main));
		AddCategory(std::move(weather));
		AddCategory(std::move(spawners));
		AddCategory(std::move(shows));
		AddCategory(std::move(time));
	}
}
