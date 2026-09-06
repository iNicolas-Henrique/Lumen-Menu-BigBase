#include "Command.hpp"
#include "Commands.hpp"
#include "util/Joaat.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <format>
#include <initializer_list>
#include <string_view>

namespace YimMenu
{
	namespace
	{
		std::string ToLower(std::string_view text)
		{
			std::string result(text);
			std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
				return static_cast<char>(std::tolower(c));
			});
			return result;
		}

		bool Has(std::string_view text, std::string_view token)
		{
			return text.find(token) != std::string_view::npos;
		}

		bool HasAny(std::string_view text, std::initializer_list<std::string_view> tokens)
		{
			for (const auto token : tokens)
				if (Has(text, token))
					return true;
			return false;
		}

		void ReplaceAll(std::string& text, std::string_view from, std::string_view to)
		{
			if (from.empty())
				return;
			for (std::size_t pos = 0; (pos = text.find(from, pos)) != std::string::npos; pos += to.size())
				text.replace(pos, from.size(), to);
		}

		std::string FixPortuguese(std::string text)
		{
			for (const auto& [from, to] : std::array<std::pair<std::string_view, std::string_view>, 36>{
			         {{"Configuracoes", "Configurações"}, {"configuracoes", "configurações"}, {"Protecoes", "Proteções"},
			             {"protecoes", "proteções"}, {"protecao", "proteção"}, {"Protecao", "Proteção"},
			             {"sessao", "sessão"}, {"Sessao", "Sessão"}, {"opcao", "opção"}, {"Opcoes", "Opções"},
			             {"veiculo", "veículo"}, {"Veiculo", "Veículo"}, {"veiculos", "veículos"}, {"Veiculos", "Veículos"},
			             {"animacao", "animação"}, {"Animacao", "Animação"}, {"animacoes", "animações"}, {"Animacoes", "Animações"},
			             {"municao", "munição"}, {"Municao", "Munição"}, {"nucleos", "núcleos"}, {"Nucleos", "Núcleos"},
			             {"nivel", "nível"}, {"Nivel", "Nível"}, {"maximo", "máximo"}, {"Maximo", "Máximo"},
			             {"aleatorio", "aleatório"}, {"Aleatorio", "Aleatório"}, {"proxima", "próxima"}, {"Proxima", "Próxima"},
			             {"localizacao", "localização"}, {"Localizacao", "Localização"}, {"cenario", "cenário"}, {"Cenario", "Cenário"},
			             {"voce", "você"}, {"Voce", "Você"}}})
			ReplaceAll(text, from, to);
		return text;
		}

		bool LooksPortuguese(std::string_view text)
		{
			const auto lower = ToLower(text);
			int hits = 0;
			for (const auto word : {" personagem", " jogador", " valor", " vida", " vigor", " recompensa", " procura", " arma", " cavalo", " bloque", " remove", " restaura", " preenche", " aplica", " ajusta", " seleciona", " permite", " executa", " ativa"})
				if (Has(lower, word))
					++hits;
			return hits >= 2;
		}

		std::string BuildPortugueseDescription(std::string_view name, std::string_view label, std::string_view description)
		{
			const std::string key = ToLower(std::string(name) + " " + std::string(label) + " " + std::string(description));
			if (HasAny(key, {"blockkickfrommissionlobby", "block kick from mission lobby"})) return "Bloqueia tentativas indevidas de expulsar você de um lobby de missão.";
			if (Has(key, "block") && Has(key, "spectator") && Has(key, "session")) return "Bloqueia espectadores indesejados na sessão.";
			if (Has(key, "block") && Has(key, "spectat")) return "Bloqueia tentativas de outros jogadores observarem seu personagem.";
			if (Has(key, "block") && Has(key, "attachment")) return "Impede que objetos remotos sejam anexados ao seu personagem.";
			if (Has(key, "block") && Has(key, "vehicle") && HasAny(key, {"flood", "spam"})) return "Bloqueia spam ou excesso de veículos criados ao seu redor.";
			if (Has(key, "block") && HasAny(key, {"explosion", "explode"})) return "Bloqueia explosões remotas recebidas de terceiros.";
			if (Has(key, "block") && HasAny(key, {"ptfx", "particle"})) return "Bloqueia efeitos de partículas remotos enviados ao seu personagem.";
			if (Has(key, "block") && Has(key, "clear") && Has(key, "task")) return "Bloqueia tentativas remotas de interromper as tarefas do seu personagem.";
			if (Has(key, "block") && HasAny(key, {"remote native", "script command"})) return "Bloqueia comandos remotos que tentam chamar funções do jogo no seu cliente.";
			if (Has(key, "block") && Has(key, "honor")) return "Bloqueia eventos remotos que tentam alterar sua honra.";
			if (Has(key, "block") && Has(key, "defensive")) return "Bloqueia tentativas remotas de forçar o modo defensivo.";
			if (Has(key, "block") && Has(key, "offensive")) return "Bloqueia tentativas remotas de forçar o modo ofensivo.";
			if (Has(key, "block") && HasAny(key, {"press charges", "charges"})) return "Bloqueia eventos remotos de denúncia contra seu personagem.";
			if (Has(key, "block") && Has(key, "start") && Has(key, "parlay")) return "Bloqueia tentativas remotas de iniciar uma trégua.";
			if (Has(key, "block") && Has(key, "end") && Has(key, "parlay")) return "Bloqueia tentativas remotas de encerrar uma trégua.";
			if (Has(key, "block") && HasAny(key, {"ticker", "notification spam"})) return "Bloqueia spam de avisos e mensagens na tela.";
			if (Has(key, "block") && Has(key, "stable")) return "Bloqueia eventos remotos ligados ao estábulo.";
			if (HasAny(key, {"relay connection", "relay connections", "userelaycxns"})) return "Usa conexões retransmitidas quando o jogo disponibiliza esse caminho de rede.";

			if (HasAny(key, {"restoreplayer", "restore player"})) return "Revive o personagem, recupera a vida e restaura o vigor.";
			if (HasAny(key, {"cleanplayer", "keepclean", "keep clean"})) return "Remove sangue e sujeira do personagem; quando contínuo, mantém a aparência limpa.";
			if (HasAny(key, {"refillcores", "keepcoresfilled", "keep cores"})) return "Preenche ou mantém cheios os núcleos de vida, vigor e Olho da Morte.";
			if (HasAny(key, {"refilldeadeye", "dead eye", "deadeye"}) && HasAny(key, {"refill", "restore", "filled"})) return "Restaura ou mantém cheia a reserva do Olho da Morte.";
			if (HasAny(key, {"cleartasks", "clear tasks"}) && !Has(key, "block")) return "Interrompe imediatamente a animação, o cenário ou a tarefa atual.";
			if (HasAny(key, {"ragdollplayer", "ragdoll player"})) return "Faz o próprio personagem entrar em ragdoll por alguns segundos.";
			if (HasAny(key, {"removeallweapons", "remove all weapons"})) return "Remove todas as armas carregadas pelo seu personagem.";
			if (HasAny(key, {"maximumhostility", "maximum hostility"})) return "Coloca imediatamente a perseguição da lei no nível máximo.";
			if (HasAny(key, {"clearlawstate", "clear law"})) return "Remove a recompensa e encerra a perseguição atual da lei.";
			if (HasAny(key, {"unlimiteditems", "unlimited items"})) return "Impede o consumo de itens ao bloquear a mensagem de uso enviada ao servidor enquanto a opção estiver ativada.";
			if (HasAny(key, {"zombieslogging", "zombies logging"})) return "Registra no log o início das rodadas, a quantidade de zumbis restantes, falhas de criação e reinícios do minijogo.";
			if (HasAny(key, {"hardmode", "hard mode"})) return "Aumenta a vida, a armadura, a velocidade e os atributos de combate dos zumbis criados pelo minijogo.";
			if (HasAny(key, {"undeadnightmare", "undead nightmare"})) return "Inicia o minijogo de ondas de zumbis; uma nova rodada começa quando os zumbis da rodada atual são eliminados.";
			if (HasAny(key, {"godmode", "god mode"})) return "Impede que seu personagem receba dano enquanto a opção estiver ativada.";
			if (HasAny(key, {"noragdoll", "no ragdoll"})) return "Impede quedas e efeitos de ragdoll no seu personagem.";
			if (HasAny(key, {"antihogtie", "anti hogtie"})) return "Impede que seu personagem seja amarrado.";
			if (HasAny(key, {"antilasso", "anti lasso"})) return "Impede que laços prendam seu personagem.";
			if (HasAny(key, {"antimelee", "anti melee"})) return "Bloqueia golpes corpo a corpo contra seu personagem.";
			if (HasAny(key, {"superjump", "super jump"})) return "Aumenta a altura dos saltos do personagem.";
			if (HasAny(key, {"superrun", "super run"}) && !Has(key, "horse")) return "Aumenta a velocidade de corrida do personagem.";
			if (Has(key, "noclip")) return Has(key, "speed") ? "Ajusta a velocidade usada pelo movimento livre do noclip." : "Permite mover o personagem livremente através do cenário.";
			if (HasAny(key, {"freecam", "free cam"})) return Has(key, "speed") ? "Ajusta a velocidade de movimento da câmera livre." : "Libera a câmera para se mover independentemente do personagem.";
			if (HasAny(key, {"npcignore", "npc ignore"})) return "Faz os personagens controlados pelo jogo ignorarem você quando possível.";
			if (HasAny(key, {"eagleeye", "eagle eye"})) return "Mantém a habilidade Olho de Águia ativa conforme o estado da opção.";
			if (HasAny(key, {"whistle", "assobio"})) return "Ajusta as características do assobio usado para chamar a montaria.";

			if (HasAny(key, {"infiniteammo", "infinite ammo"})) return "Mantém a munição de reserva sem consumo.";
			if (HasAny(key, {"infiniteclip", "infinite clip"})) return "Evita que os disparos consumam a munição carregada na arma.";
			if (HasAny(key, {"nospread", "no spread"})) return "Reduz a dispersão dos disparos para deixar os tiros mais precisos.";
			if (HasAny(key, {"autocock", "auto cock"})) return "Prepara automaticamente a arma entre disparos quando necessário.";
			if (HasAny(key, {"keepgunsclean", "keep guns clean"})) return "Mantém as armas limpas para evitar perda de condição.";
			if (HasAny(key, {"olddeadeye", "old deadeye"})) return "Ativa o conjunto alternativo de comportamento do Olho da Morte.";
			if (HasAny(key, {"deadeyetagging", "dead eye tagging"})) return "Controla a marcação automática de alvos durante o Olho da Morte.";

			if (Has(key, "horse") && HasAny(key, {"bar", "core", "agitation"})) return "Mantém automaticamente os atributos correspondentes do cavalo em boas condições.";
			if (Has(key, "horse") && HasAny(key, {"clean", "limp"})) return "Mantém o cavalo limpo automaticamente.";
			if (Has(key, "horse") && HasAny(key, {"superrun", "super run"})) return "Aumenta a velocidade de corrida do cavalo.";
			if (HasAny(key, {"horsegodmode", "horse god"})) return "Impede que a montaria receba dano enquanto a opção estiver ativada.";
			if (HasAny(key, {"horsenoragdoll", "horse no ragdoll"})) return "Reduz quedas e efeitos de ragdoll da montaria.";
			if (HasAny(key, {"flaminghooves", "flaming hooves"})) return "Adiciona um efeito visual de fogo aos cascos da montaria.";
			if (HasAny(key, {"tpmounttoself", "mount to self"})) return "Teleporta sua montaria atual até a sua posição.";
			if (HasAny(key, {"repairvehicle", "repair vehicle"})) return "Repara o veículo que você está usando.";
			if (HasAny(key, {"vehiclegodmode", "vehicle god"})) return "Protege o veículo atual contra dano enquanto estiver ativado.";
			if (HasAny(key, {"superdrive", "super drive"})) return "Adiciona impulso extra ao dirigir o veículo atual.";
			if (HasAny(key, {"superbrake", "super brake"})) return "Aumenta a força de frenagem do veículo atual.";

			if (HasAny(key, {"autotp", "auto tp"})) return "Executa automaticamente o teleporte quando a função correspondente exigir.";
			if (HasAny(key, {"waypoint", "map marker"}) && HasAny(key, {"tp", "teleport"})) return "Teleporta você para o ponto marcado no mapa.";
			if (HasAny(key, {"moonshine", "shack"}) && HasAny(key, {"tp", "teleport"})) return "Teleporta você para a cabana de moonshine.";
			if (HasAny(key, {"nazar", "madam"}) && HasAny(key, {"tp", "teleport"})) return "Teleporta você para a localização atual da Madame Nazar.";
			if (Has(key, "guarma") && HasAny(key, {"tp", "teleport"})) return "Teleporta você para Guarma.";
			if (Has(key, "train") && HasAny(key, {"tp", "teleport"})) return "Teleporta você para uma linha de trem próxima.";
			if (Has(key, "mount") && HasAny(key, {"tp", "teleport"})) return "Teleporta você para a montaria atual.";
			if (HasAny(key, {"teleport", "tpto", "tpinto", "tpbehind", "bring"})) return "Executa o teleporte indicado pelo nome da opção para o destino selecionado.";

			if (HasAny(key, {"newsession", "new session"})) return "Sai da sessão atual e solicita uma nova sessão do jogo.";
			if (HasAny(key, {"blockalltelemetry", "block telemetry"})) return "Bloqueia a telemetria tratada por esta função.";
			if (HasAny(key, {"locklobby", "lock lobby"})) return "Impede novas entradas no lobby enquanto o bloqueio estiver ativo.";
			if (HasAny(key, {"revealall", "reveal all"})) return "Mostra os jogadores da sessão quando o recurso estiver ativado.";
			if (HasAny(key, {"overlayfps", "fps overlay"})) return "Mostra a taxa de quadros por segundo na sobreposição do Tenebris.";
			if (HasAny(key, {"overlay"})) return "Ativa a sobreposição de informações do Tenebris na tela.";
			if (HasAny(key, {"espdrawplayers"})) return "Mostra informações visuais dos jogadores na tela através do ESP.";
			if (HasAny(key, {"espdrawpeds"})) return "Mostra informações visuais dos PEDs na tela através do ESP.";
			if (HasAny(key, {"ctxmenu"})) return "Ativa o menu de contexto usado para interagir com entidades selecionadas.";
			if (HasAny(key, {"togglemenukey"})) return "Alterna a tecla usada para abrir o Tenebris entre F5 e Insert.";
			if (HasAny(key, {"weather", "clima"})) return "Define o clima que será aplicado ao mundo até que seja restaurado.";
			if (HasAny(key, {"time", "clock", "hora"})) return "Ajusta o horário usado no mundo do jogo.";
			if (HasAny(key, {"spawn", "spawner"})) return "Cria no mundo a entidade selecionada por esta opção.";

			if (Has(key, "kick") && !Has(key, "block")) return "Tenta remover o jogador selecionado da sessão atual.";
			if (HasAny(key, {"explodeall", "explode player", "kill player", "beatdown", "beat down", "poodle attack", "remote bolas", "spank", "cage player", "delete horse", "delete vehicle"})) return "Executa a ação indicada pelo nome da opção no jogador selecionado.";
			if (HasAny(key, {"spoof", "hidegod", "hide god", "hidespectate", "hide spectate"})) return "Altera o dado ou comportamento indicado pelo nome da opção durante a sessão.";
			if (LooksPortuguese(description)) return FixPortuguese(std::string(description));
			return FixPortuguese(std::string(description));
		}

		std::string BuildEnglishDescription(std::string_view name, std::string_view label, std::string_view description)
		{
			const std::string key = ToLower(std::string(name) + " " + std::string(label));
			if (HasAny(key, {"unlimiteditems"})) return "Prevents item consumption by blocking the item-use message sent to the server while enabled.";
			if (HasAny(key, {"completedailies"})) return "Completes the available daily challenges using their corresponding stats.";
			if (HasAny(key, {"zombieslogging"})) return "Logs round starts, remaining zombie counts, spawn failures, and minigame resets.";
			if (HasAny(key, {"hardmode"})) return "Increases zombie health, armor, movement speed, and combat attributes in the minigame.";
			if (HasAny(key, {"undeadnightmare"})) return "Starts the zombie-wave minigame; a new round begins after the current wave is eliminated.";
			if (!LooksPortuguese(description) && !description.empty()) return std::string(description);
			if (HasAny(key, {"maximumhostility"})) return "Immediately sets law pursuit to the maximum wanted level.";
			if (HasAny(key, {"clearlawstate"})) return "Clears the bounty and ends the current law pursuit.";
			if (HasAny(key, {"restoreplayer"})) return "Revives the player and restores health and stamina.";
			if (HasAny(key, {"cleanplayer"})) return "Removes visible blood and dirt from the player.";
			if (HasAny(key, {"refillcores"})) return "Refills the health, stamina and Dead Eye cores.";
			if (HasAny(key, {"refilldeadeye"})) return "Refills the Dead Eye meter.";
			if (HasAny(key, {"cleartasks"})) return "Stops the player's current animation, scenario or task.";
			if (HasAny(key, {"removeallweapons"})) return "Removes all weapons currently carried by the player.";
			return std::string(description);
		}
	}

	Command::Command(std::string name, std::string label, std::string description, int num_args) :
	    m_Name(name),
	    m_Label(label),
	    m_Description(BuildPortugueseDescription(name, label, description)),
	    m_EnglishDescription(BuildEnglishDescription(name, label, description)),
	    m_Hash(Joaat(name)),
	    m_NumArgs(num_args)
	{
		Commands::AddCommand(this);
	}

	void Command::Call()
	{
		OnCall();
	}

	void Command::MarkDirty()
	{
		Commands::MarkDirty();
	}
}