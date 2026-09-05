#include "Localization.hpp"

#include <array>
#include <atomic>

namespace YimMenu::Localization
{
	namespace
	{
		std::atomic<Language> g_Language{Language::Portuguese};

		struct Translation
		{
			std::string_view pt;
			std::string_view en;
		};

		// Central vocabulary used by the classic menu and the most common editors.
		// Feature-specific texts can be added here without changing menu logic.
		constexpr std::array kTranslations = {
		    Translation{"Personagem", "Player"},
		    Translation{"Teleporte", "Teleport"},
		    Translation{"Rede", "Network"},
		    Translation{"Jogadores", "Players"},
		    Translation{"Mundo", "World"},
		    Translation{"Recuperação", "Recovery"},
		    Translation{"Configurações", "Settings"},
		    Translation{"Depuração", "Debug"},
		    Translation{"Principal", "Main"},
		    Translation{"Utilidades", "Utilities"},
		    Translation{"Armas", "Weapons"},
		    Translation{"Cavalo", "Horse"},
		    Translation{"Veículo", "Vehicle"},
		    Translation{"Animações", "Animations"},
		    Translation{"Clima", "Weather"},
		    Translation{"Criadores", "Spawners"},
		    Translation{"Espetáculos", "Shows"},
		    Translation{"Horário", "Time"},
		    Translation{"Atalhos", "Hotkeys"},
		    Translation{"Proteções", "Protections"},
		    Translation{"Idioma", "Language"},
		    Translation{"Português", "Portuguese"},
		    Translation{"Inglês", "English"},
		    Translation{"Ações", "Actions"},
		    Translation{"Lei e recompensa", "Law and bounty"},
		    Translation{"Aparência", "Appearance"},
		    Translation{"Movimento", "Movement"},
		    Translation{"Ferramentas", "Tools"},
		    Translation{"Opções", "Options"},
		    Translation{"Itens ilimitados", "Unlimited Items"},
		    Translation{"Concluir desafios diários", "Complete daily challenges"},
		    Translation{"Criar veículo", "Spawn Vehicle"},
		    Translation{"Criar PED Animais", "Spawn Animal PED"},
		    Translation{"Criar Ped Humanos", "Spawn Human PED"},
		    Translation{"Criar trem", "Spawn Train"},
		    Translation{"Clima do mundo", "World Weather"},
		    Translation{"Restaurar", "Restore"},
		    Translation{"Aplicar", "Apply"},
		    Translation{"Criar", "Spawn"},
		    Translation{"Sem nível de procurado", "No wanted level"},
		    Translation{"Nível máximo de procurado", "Maximum wanted level"},
		    Translation{"Cartas de habilidade", "Ability Cards"},
		    Translation{"Olho da Morte", "Dead Eye"},
		    Translation{"Passiva 1", "Passive 1"},
		    Translation{"Passiva 2", "Passive 2"},
		    Translation{"Passiva 3", "Passive 3"},
		    Translation{"Carta equipada", "Equipped card"},
		    Translation{"Editar efeito", "Edit effect"},
		    Translation{"Restaurar padrões", "Restore defaults"},
		    Translation{"Ainda não mapeado com segurança.", "Not safely mapped yet."},
		    Translation{"MENU PRINCIPAL", "MAIN MENU"},
		    Translation{"ENCERRAR TENEBRIS", "EXIT TENEBRIS"},
		    Translation{"Encerrar Tenebris", "Exit Tenebris"},
		    Translation{"Sim", "Yes"},
		    Translation{"Não", "No"},
		    Translation{"Deseja realmente encerrar o Tenebris?", "Do you really want to exit Tenebris?"},
		    Translation{"Setas: navegar  |  Enter: selecionar  |  Backspace: Voltar", "Arrows: navigate  |  Enter: select  |  Backspace: Back"},
		    Translation{"Jogo", "Game"},
		    Translation{"desconhecida", "unknown"},
		    Translation{"Ativado", "Enabled"},
		    Translation{"Desativado", "Disabled"},
		    Translation{"Selecionar", "Select"},
		    Translation{"Selecionado", "Selected"},
		    Translation{"Nenhum", "None"},
		    Translation{"Fechar", "Close"},
		    Translation{"Voltar", "Back"},
		    Translation{"Salvar", "Save"},
		    Translation{"Filtro", "Filter"},
		    Translation{"Pesquisar", "Search"},
		    Translation{"Todos", "All"},
		    Translation{"Animais", "Animals"},
		    Translation{"Inimigos", "Enemies"},
		    Translation{"Força", "Force"},
		    Translation{"Direcional", "Directional"},
		    Translation{"Escala do personagem", "Player Scale"},
		    Translation{"Escala do cavalo", "Horse Scale"},
		    Translation{"Animações e músicas", "Animations and music"},
		    Translation{"Músicas favoritas", "Favorite music"},
		    Translation{"Músicas recentes", "Recent music"},
		    Translation{"Limpar histórico", "Clear history"},
		    Translation{"Adicionar aos favoritos", "Add to favorites"},
		    Translation{"Remover dos favoritos", "Remove from favorites"},
		    Translation{"Limpar favoritos", "Clear favorites"},
		};
	}

	void SetLanguage(Language language)
	{
		g_Language.store(language, std::memory_order_relaxed);
	}

	Language GetLanguage()
	{
		return g_Language.load(std::memory_order_relaxed);
	}

	bool IsPortuguese()
	{
		return GetLanguage() == Language::Portuguese;
	}

	const char* GetLanguageCode()
	{
		return IsPortuguese() ? "pt-BR" : "en";
	}

	Language FromLanguageCode(std::string_view code)
	{
		return code == "en" || code == "en-US" || code == "english" ? Language::English : Language::Portuguese;
	}

	std::string Text(std::string_view text)
	{
		for (const auto& entry : kTranslations)
		{
			if (IsPortuguese())
			{
				if (text == entry.en || text == entry.pt)
					return std::string(entry.pt);
			}
			else if (text == entry.pt || text == entry.en)
			{
				return std::string(entry.en);
			}
		}
		return std::string(text);
	}
}
