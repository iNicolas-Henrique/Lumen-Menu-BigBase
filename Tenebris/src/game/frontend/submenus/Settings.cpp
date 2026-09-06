#include "Settings.hpp"

#include "core/commands/Commands.hpp"
#include "core/commands/HotkeySystem.hpp"
#include "core/commands/LoopedCommand.hpp"
#include "core/frontend/Localization.hpp"
#include "game/backend/Self.hpp"
#include "game/features/Features.hpp"
#include "game/frontend/items/Items.hpp"

#include <Windows.h>
#include <filesystem>
#include <fstream>

namespace YimMenu::Submenus
{
	int Settings::GetWindowWidth() const { return m_WindowWidth; }
	int Settings::GetWindowHeight() const { return m_WindowHeight; }
	int Settings::GetWindowPosX() const { return m_WindowPosX; }
	int Settings::GetWindowPosY() const { return m_WindowPosY; }
	bool Settings::IsWindowMaximized() const { return m_WindowMaximized; }

	void Settings::SetWindowWidth(int w) { m_WindowWidth = w; }
	void Settings::SetWindowHeight(int h) { m_WindowHeight = h; }
	void Settings::SetWindowPosX(int x) { m_WindowPosX = x; }
	void Settings::SetWindowPosY(int y) { m_WindowPosY = y; }
	void Settings::SetWindowMaximized(bool maximized) { m_WindowMaximized = maximized; }

	std::string Settings::GetSettingsFilePath() const
	{
		char* appdata = std::getenv("appdata");
		if (!appdata)
			return "";
		std::filesystem::path path = std::filesystem::path(appdata) / "Tenebris" / "settings.json";
		return path.string();
	}

	void Settings::LoadWindowSettings(const nlohmann::json& j)
	{
		int defW = 800, defH = 600, defX = 100, defY = 100;
		bool defMax = false;
		if (j.contains("window") && j["window"].is_object())
		{
			const auto& win = j["window"];
			m_WindowWidth = win.value("width", defW);
			m_WindowHeight = win.value("height", defH);
			m_WindowPosX = win.value("pos_x", defX);
			m_WindowPosY = win.value("pos_y", defY);
			m_WindowMaximized = win.value("is_maximized", defMax);
		}
		else
		{
			m_WindowWidth = defW;
			m_WindowHeight = defH;
			m_WindowPosX = defX;
			m_WindowPosY = defY;
			m_WindowMaximized = defMax;
		}

		int screenW = GetSystemMetrics(SM_CXSCREEN);
		int screenH = GetSystemMetrics(SM_CYSCREEN);
		if (m_WindowWidth < 300 || m_WindowWidth > screenW) m_WindowWidth = defW;
		if (m_WindowHeight < 200 || m_WindowHeight > screenH) m_WindowHeight = defH;
		if (m_WindowPosX < 0 || m_WindowPosX > screenW - 50) m_WindowPosX = defX;
		if (m_WindowPosY < 0 || m_WindowPosY > screenH - 50) m_WindowPosY = defY;

		m_LastSavedWidth = m_WindowWidth;
		m_LastSavedHeight = m_WindowHeight;
		m_LastSavedPosX = m_WindowPosX;
		m_LastSavedPosY = m_WindowPosY;
		m_LastSavedMaximized = m_WindowMaximized;
	}

	void Settings::SaveWindowSettings(nlohmann::json& j) const
	{
		nlohmann::json win;
		win["width"] = m_WindowWidth;
		win["height"] = m_WindowHeight;
		win["pos_x"] = m_WindowPosX;
		win["pos_y"] = m_WindowPosY;
		win["is_maximized"] = m_WindowMaximized;
		j["window"] = win;
	}

	void Settings::LoadSettings()
	{
		std::string path = GetSettingsFilePath();
		std::ifstream in(path);
		if (in)
		{
			try
			{
				nlohmann::json j;
				in >> j;
				LoadWindowSettings(j);
				Localization::SetLanguage(Localization::FromLanguageCode(j.value("language", "pt-BR")));
			}
			catch (...)
			{
				m_WindowWidth = 800;
				m_WindowHeight = 600;
				m_WindowPosX = 100;
				m_WindowPosY = 100;
				m_WindowMaximized = false;
				Localization::SetLanguage(Localization::Language::Portuguese);
			}
		}
		else
		{
			m_WindowWidth = 800;
			m_WindowHeight = 600;
			m_WindowPosX = 100;
			m_WindowPosY = 100;
			m_WindowMaximized = false;
			Localization::SetLanguage(Localization::Language::Portuguese);
		}
		m_LastSavedWidth = m_WindowWidth;
		m_LastSavedHeight = m_WindowHeight;
		m_LastSavedPosX = m_WindowPosX;
		m_LastSavedPosY = m_WindowPosY;
		m_LastSavedMaximized = m_WindowMaximized;
	}

	void Settings::SaveSettings()
	{
		std::string path = GetSettingsFilePath();
		if (path.empty())
			return;
		std::filesystem::create_directories(std::filesystem::path(path).parent_path());
		nlohmann::json j;
		std::ifstream in(path);
		if (in)
		{
			try
			{
				in >> j;
			}
			catch (...)
			{
				j = nlohmann::json::object();
			}
		}
		SaveWindowSettings(j);
		j["language"] = Localization::GetLanguageCode();
		std::ofstream out(path);
		if (out)
			out << j.dump(4);
	}

	void Settings::SaveIfWindowChanged(int width, int height, int posX, int posY, bool maximized)
	{
		if (width != m_LastSavedWidth || height != m_LastSavedHeight || posX != m_LastSavedPosX || posY != m_LastSavedPosY || maximized != m_LastSavedMaximized)
		{
			m_WindowWidth = width;
			m_WindowHeight = height;
			m_WindowPosX = posX;
			m_WindowPosY = posY;
			m_WindowMaximized = maximized;
			SaveSettings();
			m_LastSavedWidth = width;
			m_LastSavedHeight = height;
			m_LastSavedPosX = posX;
			m_LastSavedPosY = posY;
			m_LastSavedMaximized = maximized;
		}
	}

	static BoolCommand g_UseInsertForMenuToggle("togglemenukey", "Menu Key", "Choose whether Tenebris opens with F5 or Insert.", false);

	static void DrawHotkeySettings()
	{
		if (Localization::IsPortuguese())
		{
			ImGui::BulletText("Clique no nome de um comando para alterar o atalho.");
			ImGui::BulletText("Pressione uma tecla já registrada para remover o atalho.");
		}
		else
		{
			ImGui::BulletText("Click a command name to change its hotkey.");
			ImGui::BulletText("Press an already registered key to remove the hotkey.");
		}
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
		for (auto& [Hash, Command] : Commands::GetCommands())
		{
			ImGui::PushID(Hash);
			if (g_HotkeySystem.m_CommandHotkeys.find(Hash) != g_HotkeySystem.m_CommandHotkeys.end())
				HotkeySetter(Hash).Draw();
			ImGui::Spacing();
			ImGui::PopID();
		}
	}

	Settings::Settings() :
	    Submenu::Submenu("Configurações")
	{
		auto language = std::make_shared<Category>("Idioma");
		auto languageGroup = std::make_shared<Group>("Idioma", 1);
		languageGroup->AddItem(std::make_shared<ImGuiItem>([this] {
			int selected = Localization::IsPortuguese() ? 0 : 1;
			const char* languages[] = {"Português", "English"};
			if (ImGui::Combo("##TenebrisLanguage", &selected, languages, IM_ARRAYSIZE(languages)))
			{
				Localization::SetLanguage(selected == 0 ? Localization::Language::Portuguese : Localization::Language::English);
				SaveSettings();
			}
			ImGui::Spacing();
			ImGui::TextWrapped("%s", Localization::IsPortuguese()
			        ? "O idioma escolhido é salvo e será usado na próxima vez que o Tenebris for aberto."
			        : "The selected language is saved and will be used the next time Tenebris starts.");
		}, "Idioma", "Escolha o idioma da interface. A seleção fica salva.", 220.0f));
		language->AddItem(languageGroup);

		auto hotkeys = std::make_shared<Category>("Atalhos");
		auto gui = std::make_shared<Category>("GUI");
		auto protections = std::make_shared<Category>("Proteções");
		auto syncGroup = std::make_shared<Group>("Sincronização");
		auto networkEventGroup = std::make_shared<Group>("Eventos de rede");
		auto scriptEventGroup = std::make_shared<Group>("Eventos de script");
		auto playerEsp = std::make_shared<Group>("ESP de jogadores", 10);
		auto pedEsp = std::make_shared<Group>("ESP de PEDs", 10);
		auto overlay = std::make_shared<Group>("Sobreposição");
		auto context = std::make_shared<Group>("Menu de contexto");
		auto misc = std::make_shared<Group>("Diversos");

		hotkeys->AddItem(std::make_shared<ImGuiItem>(DrawHotkeySettings, "Teclas de atalho", "Permite definir teclas para executar comandos sem procurar a opção no menu."));

		playerEsp->AddItem(std::make_shared<BoolCommandItem>("espdrawplayers"_J));
		playerEsp->AddItem(std::make_shared<ConditionalItem>("espdrawplayers"_J, std::make_shared<BoolCommandItem>("espdrawdeadplayers"_J)));
		playerEsp->AddItem(std::make_shared<ConditionalItem>("espdrawplayers"_J, std::make_shared<BoolCommandItem>("espnameplayers"_J, "Player Name")));
		playerEsp->AddItem(std::make_shared<ConditionalItem>("espdrawplayers"_J, std::make_shared<ColorCommandItem>("namecolorplayers"_J)));
		playerEsp->AddItem(std::make_shared<ConditionalItem>("espdrawplayers"_J, std::make_shared<BoolCommandItem>("espdistanceplayers"_J, "Player Distance")));
		playerEsp->AddItem(std::make_shared<ConditionalItem>("espdrawplayers"_J, std::make_shared<ColorCommandItem>("distancecolorplayers"_J)));
		playerEsp->AddItem(std::make_shared<ConditionalItem>("espdrawplayers"_J, std::make_shared<BoolCommandItem>("espskeletonplayers"_J, "Player Skeleton")));
		playerEsp->AddItem(std::make_shared<ConditionalItem>("espdrawplayers"_J, std::make_shared<ColorCommandItem>("skeletoncolorplayers"_J)));

		pedEsp->AddItem(std::make_shared<BoolCommandItem>("espdrawpeds"_J));
		pedEsp->AddItem(std::make_shared<ConditionalItem>("espdrawpeds"_J, std::make_shared<BoolCommandItem>("espdrawdeadpeds"_J)));
		pedEsp->AddItem(std::make_shared<ConditionalItem>("espdrawpeds"_J, std::make_shared<BoolCommandItem>("espmodelspeds"_J, "Ped Hashes")));
		pedEsp->AddItem(std::make_shared<ConditionalItem>("espdrawpeds"_J, std::make_shared<ColorCommandItem>("hashcolorpeds"_J)));
		pedEsp->AddItem(std::make_shared<ConditionalItem>("espdrawpeds"_J, std::make_shared<BoolCommandItem>("espnetinfopeds"_J, "Ped Net Info")));
		pedEsp->AddItem(std::make_shared<ConditionalItem>("espdrawpeds"_J, std::make_shared<BoolCommandItem>("espscriptinfopeds"_J, "Ped Script Info")));
		pedEsp->AddItem(std::make_shared<ConditionalItem>("espdrawpeds"_J, std::make_shared<BoolCommandItem>("espdistancepeds"_J, "Ped Distance")));
		pedEsp->AddItem(std::make_shared<ConditionalItem>("espdrawpeds"_J, std::make_shared<ColorCommandItem>("distancecolorpeds"_J)));
		pedEsp->AddItem(std::make_shared<ConditionalItem>("espdrawpeds"_J, std::make_shared<BoolCommandItem>("espskeletonpeds"_J, "Ped Skeleton")));
		pedEsp->AddItem(std::make_shared<ConditionalItem>("espdrawpeds"_J, std::make_shared<ColorCommandItem>("skeletoncolorpeds"_J)));
		pedEsp->AddItem(std::make_shared<ConditionalItem>("espdrawpeds"_J, std::make_shared<BoolCommandItem>("espskeletonhorse"_J, "Horse Skeleton")));
		pedEsp->AddItem(std::make_shared<ConditionalItem>("espdrawpeds"_J, std::make_shared<ColorCommandItem>("skeletoncolorhorse"_J)));

		overlay->AddItem(std::make_shared<BoolCommandItem>("overlay"_J));
		overlay->AddItem(std::make_shared<ConditionalItem>("overlay"_J, std::make_shared<BoolCommandItem>("overlayfps"_J)));

		context->AddItem(std::make_shared<BoolCommandItem>("ctxmenu"_J));
		context->AddItem(std::make_shared<ConditionalItem>("ctxmenu"_J, std::make_shared<BoolCommandItem>("ctxmenuplayers"_J, "Players")));
		context->AddItem(std::make_shared<ConditionalItem>("ctxmenu"_J, std::make_shared<BoolCommandItem>("ctxmenupeds"_J, "Peds")));
		context->AddItem(std::make_shared<ConditionalItem>("ctxmenu"_J, std::make_shared<BoolCommandItem>("ctxmenuvehicles"_J, "Vehicles")));
		context->AddItem(std::make_shared<ConditionalItem>("ctxmenu"_J, std::make_shared<BoolCommandItem>("ctxmenuobjects"_J, "Objects")));
		misc->AddItem(std::make_shared<BoolCommandItem>("togglemenukey"_J));

		syncGroup->AddItem(std::make_shared<BoolCommandItem>("blockspectate"_J));
		syncGroup->AddItem(std::make_shared<BoolCommandItem>("blockspectatesession"_J));
		syncGroup->AddItem(std::make_shared<BoolCommandItem>("blockattach"_J));
		syncGroup->AddItem(std::make_shared<BoolCommandItem>("blockvehflood"_J));

		networkEventGroup->AddItem(std::make_shared<BoolCommandItem>("blockexplosions"_J));
		networkEventGroup->AddItem(std::make_shared<BoolCommandItem>("blockptfx"_J));
		networkEventGroup->AddItem(std::make_shared<BoolCommandItem>("blockclearpedtasks"_J));
		networkEventGroup->AddItem(std::make_shared<BoolCommandItem>("blockscriptcommand"_J));
		networkEventGroup->AddItem(std::make_shared<BoolCommandItem>("userelaycxns"_J));

		scriptEventGroup->AddItem(std::make_shared<BoolCommandItem>("blockhonormanipulation"_J));
		scriptEventGroup->AddItem(std::make_shared<BoolCommandItem>("blockdefensive"_J));
		scriptEventGroup->AddItem(std::make_shared<BoolCommandItem>("blockoffensive"_J));
		scriptEventGroup->AddItem(std::make_shared<BoolCommandItem>("blockpresscharges"_J));
		scriptEventGroup->AddItem(std::make_shared<BoolCommandItem>("blockstartparlay"_J));
		scriptEventGroup->AddItem(std::make_shared<BoolCommandItem>("blockendparlay"_J));
		scriptEventGroup->AddItem(std::make_shared<BoolCommandItem>("blocktickerspam"_J));
		scriptEventGroup->AddItem(std::make_shared<BoolCommandItem>("blockstableevents"_J));
		scriptEventGroup->AddItem(std::make_shared<BoolCommandItem>("blockkickfrommissionlobby"_J));

		gui->AddItem(playerEsp);
		gui->AddItem(pedEsp);
		gui->AddItem(overlay);
		gui->AddItem(context);
		gui->AddItem(misc);

		protections->AddItem(syncGroup);
		protections->AddItem(networkEventGroup);
		protections->AddItem(scriptEventGroup);

		AddCategory(std::move(language));
		AddCategory(std::move(hotkeys));
		AddCategory(std::move(gui));
		AddCategory(std::move(protections));
	}
}
