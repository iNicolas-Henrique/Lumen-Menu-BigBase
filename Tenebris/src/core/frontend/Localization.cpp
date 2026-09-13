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
		    Translation{"Veículos", "Vehicles"},
		    Translation{"Animações", "Animations"},
		    Translation{"Animacoes", "Animations"},
		    Translation{"Clima", "Weather"},
		    Translation{"Criadores", "Spawners"},
		    Translation{"Espetáculos", "Shows"},
		    Translation{"Espetaculos", "Shows"},
		    Translation{"Horário", "Time"},
		    Translation{"Horario", "Time"},
		    Translation{"Horário do mundo", "World Time"},
		    Translation{"Atalhos", "Hotkeys"},
		    Translation{"Teclas de atalho", "Hotkeys"},
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
		    Translation{"Diversão", "Fun"},
		    Translation{"Geral", "General"},
		    Translation{"Gerais", "General"},
		    Translation{"Sincronização", "Synchronization"},
		    Translation{"Eventos de rede", "Network events"},
		    Translation{"Eventos de script", "Script events"},
		    Translation{"ESP de jogadores", "Player ESP"},
		    Translation{"ESP de PEDs", "PED ESP"},
		    Translation{"Sobreposição", "Overlay"},
		    Translation{"Menu de contexto", "Context menu"},
		    Translation{"Nome do jogador", "Player Name"},
		    Translation{"Distância do jogador", "Player Distance"},
		    Translation{"Esqueleto do jogador", "Player Skeleton"},
		    Translation{"Hashes de PED", "Ped Hashes"},
		    Translation{"Informações de rede do PED", "Ped Net Info"},
		    Translation{"Informações de script do PED", "Ped Script Info"},
		    Translation{"Distância do PED", "Ped Distance"},
		    Translation{"Esqueleto do PED", "Ped Skeleton"},
		    Translation{"Esqueleto do cavalo", "Horse Skeleton"},
		    Translation{"PEDs", "Peds"},
		    Translation{"Objetos", "Objects"},
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
		    Translation{"PEDs", "PEDs"},
		    Translation{"Trens", "Trains"},
		    Translation{"Eliminar", "Eliminate"},
		    Translation{"Excluir", "Delete"},
		    Translation{"Trazer", "Bring"},
		    Translation{"Minijogos", "Minigames"},
		    Translation{"Teatro e espetáculos", "Theatre and shows"},
		    Translation{"Uso dos pools", "Pool usage"},
		    Translation{"Sem nível de procurado", "No Wanted Level"},
		    Translation{"Nível máximo de procurado", "Maximum Wanted Level"},
		    Translation{"Cartas de habilidade", "Ability Cards"},
		    Translation{"Olho da Morte", "Dead Eye"},
		    Translation{"Olho da Morte clássico", "Classic Dead Eye"},
		    Translation{"Dead Eye clássico", "Classic Dead Eye"},
		    Translation{"Alvos automáticos do Olho da Morte", "Automatic Dead Eye Targets"},
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
		    Translation{"Setas: navegar | Enter: selecionar | Backspace: Voltar", "Arrows: navigate | Enter: select | Backspace: Back"},
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
		    Translation{"Tom", "Pitch"},
		    Translation{"Clareza", "Clarity"},
		    Translation{"Formato", "Shape"},
		    Translation{"Tecla do menu", "Menu Key"},
		    Translation{"Escala do personagem", "Player Scale"},
		    Translation{"Escala do cavalo", "Horse Scale"},
		    Translation{"Entregar armas e munição", "Give weapons and ammo"},
		    Translation{"Animações e músicas", "Animations and music"},
		    Translation{"Músicas favoritas", "Favorite music"},
		    Translation{"Músicas recentes", "Recent music"},
		    Translation{"Limpar histórico", "Clear history"},
		    Translation{"Adicionar aos favoritos", "Add to favorites"},
		    Translation{"Remover dos favoritos", "Remove from favorites"},
		    Translation{"Limpar favoritos", "Clear favorites"},
		    Translation{"Registro de zumbis", "Zombies Logging"},
		    Translation{"Modo difícil", "Hard Mode"},
		    Translation{"Pesadelo dos mortos-vivos", "Undead Nightmare"},
		    Translation{"Ajusta o tamanho visual do seu personagem.", "Adjusts the visual size of your player."},
		    Translation{"Entrega todas as armas ou recarrega toda a munição.", "Gives all weapons or refills all ammunition."},
		    Translation{"Mostra o conjunto de opções do Olho da Morte clássico.", "Shows the classic Dead Eye option set."},
		    Translation{"Escolhe se o Olho da Morte marca inimigos, animais ou todos os alvos.", "Chooses whether Dead Eye tags enemies, animals, or all targets."},
		    Translation{"Ajusta o tamanho visual do seu cavalo atual.", "Adjusts the visual size of your current horse."},
		    Translation{"Seleciona animações, emotes e músicas para reproduzir no personagem.", "Selects animations, emotes, and music to play on the player."},
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
