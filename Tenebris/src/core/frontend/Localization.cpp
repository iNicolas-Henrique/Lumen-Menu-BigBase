#include "Localization.hpp"

#include <array>
#include <atomic>
#include <cctype>

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
		// Keep aliases for legacy strings that were created without accents so the
		// language switch also covers old menu code without invasive rewrites.
		constexpr std::array kTranslations = {
		    Translation{"Personagem", "Player"},
		    Translation{"Teleporte", "Teleport"},
		    Translation{"Rede", "Network"},
		    Translation{"Jogadores", "Players"},
		    Translation{"Mundo", "World"},
		    Translation{"Recuperação", "Recovery"},
		    Translation{"Recuperacao", "Recovery"},
		    Translation{"Configurações", "Settings"},
		    Translation{"Configuracoes", "Settings"},
		    Translation{"Depuração", "Debug"},
		    Translation{"Depuracao", "Debug"},
		    Translation{"Principal", "Main"},
		    Translation{"Utilidades", "Utilities"},
		    Translation{"Armas", "Weapons"},
		    Translation{"Cavalo", "Horse"},
		    Translation{"Veículo", "Vehicle"},
		    Translation{"Veiculo", "Vehicle"},
		    Translation{"Animações", "Animations"},
		    Translation{"Animacoes", "Animations"},
		    Translation{"Clima", "Weather"},
		    Translation{"Criadores", "Spawners"},
		    Translation{"Espetáculos", "Shows"},
		    Translation{"Espetaculos", "Shows"},
		    Translation{"Horário", "Time"},
		    Translation{"Horario", "Time"},
		    Translation{"Atalhos", "Hotkeys"},
		    Translation{"Proteções", "Protections"},
		    Translation{"Protecoes", "Protections"},
		    Translation{"Idioma", "Language"},
		    Translation{"Português", "Portuguese"},
		    Translation{"Inglês", "English"},
		    Translation{"Ações", "Actions"},
		    Translation{"Acoes", "Actions"},
		    Translation{"Lei e recompensa", "Law and bounty"},
		    Translation{"Aparência", "Appearance"},
		    Translation{"Aparencia", "Appearance"},
		    Translation{"Movimento", "Movement"},
		    Translation{"Ferramentas", "Tools"},
		    Translation{"Opções", "Options"},
		    Translation{"Options", "Options"},
		    Translation{"Sessão", "Session"},
		    Translation{"Sessao", "Session"},
		    Translation{"Mascaramento", "Spoofing"},
		    Translation{"Banco de jogadores", "Player database"},
		    Translation{"Registros e extras", "Logs and extras"},
		    Translation{"Voz", "Voice"},
		    Translation{"Informações", "Information"},
		    Translation{"Informacoes", "Information"},
		    Translation{"Ajuda", "Helpful"},
		    Translation{"Expulsar", "Kick"},
		    Translation{"Provocações", "Trolling"},
		    Translation{"Provocacoes", "Trolling"},
		    Translation{"Tóxico", "Toxic"},
		    Translation{"Toxico", "Toxic"},
		    Translation{"Diversos", "Misc"},
		    Translation{"Geral", "General"},
		    Translation{"Itens ilimitados", "Unlimited Items"},
		    Translation{"Concluir desafios diários", "Complete daily challenges"},
		    Translation{"Desafios diários", "Daily challenges"},
		    Translation{"Gerar colecionáveis", "Spawn collectibles"},
		    Translation{"Gerar colecionaveis", "Spawn collectibles"},
		    Translation{"Gerar ervas", "Spawn herbs"},
		    Translation{"Criar veículo", "Spawn Vehicle"},
		    Translation{"Criar PED Animais", "Spawn Animal PED"},
		    Translation{"Criar Ped Humanos", "Spawn Human PED"},
		    Translation{"Criar trem", "Spawn Train"},
		    Translation{"Clima do mundo", "World Weather"},
		    Translation{"Restaurar", "Restore"},
		    Translation{"Aplicar", "Apply"},
		    Translation{"Criar", "Spawn"},
		    Translation{"Sem nível de procurado", "No Wanted Level"},
		    Translation{"Nível máximo de procurado", "Maximum Wanted Level"},
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

		bool EqualsAsciiInsensitive(std::string_view a, std::string_view b)
		{
			if (a.size() != b.size())
				return false;
			for (std::size_t i = 0; i < a.size(); ++i)
			{
				const auto ca = static_cast<unsigned char>(a[i]);
				const auto cb = static_cast<unsigned char>(b[i]);
				if (ca < 128 && cb < 128)
				{
					if (std::tolower(ca) != std::tolower(cb))
						return false;
				}
				else if (ca != cb)
				{
					return false;
				}
			}
			return true;
		}
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
				if (EqualsAsciiInsensitive(text, entry.en) || text == entry.pt)
					return std::string(entry.pt);
			}
			else if (text == entry.pt || EqualsAsciiInsensitive(text, entry.en))
			{
				return std::string(entry.en);
			}
		}
		return std::string(text);
	}
}
