#include "util/Joaat.hpp"
#include "Command.hpp"
#include "Commands.hpp"

#include <algorithm>
#include <array>
#include <cctype>
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

		bool AlreadyAcceptedPortuguese(std::string_view description)
		{
			static constexpr std::array markers = {
			    "Abre", "Ajusta", "Aplica", "Ativa", "Configura", "Cria", "Escolha", "Entrega", "Executa", "Mostra", "Permite", "Retorna", "Seleciona", "Reune"};
			for (const auto marker : markers)
				if (description.find(marker) != std::string_view::npos)
					return true;
			return false;
		}

		bool LooksPortuguese(std::string_view text)
		{
			const auto lower = ToLower(text);
			int hits = 0;
			for (const auto word : {" personagem", " jogador", " sessao", " valor", " vida", " vigor", " recompensa", " procura", " arma", " veiculo", " cavalo", " bloque", " remove", " restaura", " preenche", " aplica", " ajusta", " seleciona"})
				if (Has(lower, word))
					++hits;
			return hits >= 2;
		}

		std::string BuildPortugueseDescription(std::string_view name, std::string_view label, std::string_view description)
		{
			if (AlreadyAcceptedPortuguese(description))
				return std::string(description);

			const std::string key = ToLower(std::string(name) + " " + std::string(label) + " " + std::string(description));

			// Protecoes: explicacoes curtas e diretas para quem nao conhece os termos tecnicos.
			if (HasAny(key, {"blockkickfrommissionlobby", "block kick from mission lobby"}))
				return "Ativa uma protecao contra expulsao indevida do lobby de missao.";
			if (Has(key, "block") && Has(key, "spectator") && Has(key, "session"))
				return "Ativa o bloqueio de espectadores indesejados na sessao.";
			if (Has(key, "block") && Has(key, "spectat"))
				return "Ativa o bloqueio de tentativas de observar seu personagem.";
			if (Has(key, "block") && Has(key, "attachment"))
				return "Ativa o bloqueio de objetos anexados ao seu personagem.";
			if (Has(key, "block") && Has(key, "vehicle") && HasAny(key, {"flood", "spam"}))
				return "Ativa o bloqueio de excesso de veiculos ao seu redor.";
			if (Has(key, "block") && HasAny(key, {"explosion", "explode"}))
				return "Ativa o bloqueio de explosoes recebidas de terceiros.";
			if (Has(key, "block") && HasAny(key, {"ptfx", "particle"}))
				return "Ativa o bloqueio de efeitos de particulas recebidos.";
			if (Has(key, "block") && Has(key, "clear") && Has(key, "task"))
				return "Ativa o bloqueio de tentativas de interromper suas tarefas.";
			if (Has(key, "block") && HasAny(key, {"remote native", "script command"}))
				return "Ativa o bloqueio de chamadas remotas de funcoes do jogo.";
			if (Has(key, "block") && Has(key, "honor"))
				return "Ativa o bloqueio de eventos remotos que alteram sua honra.";
			if (Has(key, "block") && Has(key, "defensive"))
				return "Ativa o bloqueio de tentativas de forcar o modo defensivo.";
			if (Has(key, "block") && Has(key, "offensive"))
				return "Ativa o bloqueio de tentativas de forcar o modo ofensivo.";
			if (Has(key, "block") && HasAny(key, {"press charges", "charges"}))
				return "Ativa o bloqueio de eventos remotos de denuncia.";
			if (Has(key, "block") && Has(key, "start") && Has(key, "parlay"))
				return "Ativa o bloqueio de tentativas remotas de iniciar tregua.";
			if (Has(key, "block") && Has(key, "end") && Has(key, "parlay"))
				return "Ativa o bloqueio de tentativas remotas de encerrar tregua.";
			if (Has(key, "block") && HasAny(key, {"ticker", "notification spam"}))
				return "Ativa o bloqueio de spam de avisos na tela.";
			if (Has(key, "block") && Has(key, "stable"))
				return "Ativa o bloqueio de eventos remotos ligados ao estabulo.";
			if (HasAny(key, {"relay connection", "relay connections"}))
				return "Configura o uso de conexoes retransmitidas quando disponiveis.";

			// Acoes de sessao potencialmente ofensivas ficam descritas sem detalhes tecnicos do metodo.
			if (Has(key, "kick") && !Has(key, "block"))
				return "Executa uma tentativa de remover o jogador selecionado da sessao.";
			if (HasAny(key, {"explodeall", "explode player", "kill player", "beatdown", "beat down", "poodle attack", "remote bolas", "spank", "cage player", "delete horse", "delete vehicle"}))
				return "Executa uma interacao ofensiva no alvo selecionado; use apenas em ambiente autorizado.";
			if (HasAny(key, {"spoof", "hidegod", "hide god", "hidespectate", "hide spectate"}))
				return "Configura dados ou comportamento de sessao para testes autorizados.";

			// Personagem e utilidades locais.
			if (HasAny(key, {"restoreplayer", "restore player"}))
				return "Executa a restauracao da vida e do vigor do personagem.";
			if (HasAny(key, {"cleanplayer", "keepclean", "keep clean"}))
				return "Ativa ou executa a limpeza de sangue e sujeira do personagem.";
			if (HasAny(key, {"refillcores", "keepcoresfilled", "keep cores"}))
				return "Ativa ou executa o preenchimento dos nucleos do personagem.";
			if (HasAny(key, {"refilldeadeye", "dead eye", "deadeye"}) && HasAny(key, {"refill", "restore", "filled"}))
				return "Executa o preenchimento do Olho da Morte.";
			if (HasAny(key, {"randomoutfit", "random outfit"}))
				return "Executa a troca para um traje aleatorio compativel.";
			if (HasAny(key, {"cleartasks", "clear tasks"}) && !Has(key, "block"))
				return "Executa o cancelamento da tarefa ou animacao atual.";
			if (HasAny(key, {"ragdollplayer", "ragdoll player"}))
				return "Executa uma queda temporaria do proprio personagem.";
			if (HasAny(key, {"groundplayer", "place on ground"}))
				return "Executa o reposicionamento do personagem sobre o solo.";
			if (HasAny(key, {"removeallweapons", "remove all weapons"}))
				return "Executa a remocao das armas do proprio personagem.";
			if (HasAny(key, {"bountyamount", "bounty amount"}))
				return "Configura o valor da recompensa que sera aplicado ao personagem.";
			if (HasAny(key, {"applybounty", "apply bounty"}))
				return "Executa a aplicacao do valor de recompensa configurado.";
			if (HasAny(key, {"wantedscore", "wanted score"}))
				return "Configura a intensidade local de procura da lei.";
			if (HasAny(key, {"applywantedscore", "apply wanted"}))
				return "Executa a aplicacao do nivel de procura configurado.";
			if (HasAny(key, {"maximumhostility", "maximum hostility"}))
				return "Executa a definicao da procura da lei no nivel maximo.";
			if (HasAny(key, {"readlawstate", "read law"}))
				return "Mostra os valores atuais de recompensa e procura da lei.";
			if (HasAny(key, {"clearlawstate", "clear law"}))
				return "Executa a limpeza da recompensa e da procura da lei.";

			if (HasAny(key, {"godmode", "god mode"}))
				return "Ativa invulnerabilidade contra dano enquanto estiver ligado.";
			if (HasAny(key, {"noragdoll", "no ragdoll"}))
				return "Ativa protecao contra quedas e efeito de ragdoll.";
			if (HasAny(key, {"antihogtie", "anti hogtie"}))
				return "Ativa protecao contra tentativas de amarrar o personagem.";
			if (HasAny(key, {"antilasso", "anti lasso"}))
				return "Ativa protecao contra tentativas de usar laco no personagem.";
			if (HasAny(key, {"antimelee", "anti melee"}))
				return "Ativa protecao contra golpes corpo a corpo.";
			if (HasAny(key, {"superjump", "super jump"}))
				return "Ativa saltos mais altos que o normal.";
			if (HasAny(key, {"superrun", "super run"}))
				return "Ativa corrida mais rapida que o normal.";
			if (Has(key, "noclip"))
				return "Ativa movimento livre atraves do cenario.";
			if (HasAny(key, {"freecam", "free cam"}))
				return "Ativa uma camera livre independente do personagem.";
			if (HasAny(key, {"infiniteammo", "infinite ammo"}))
				return "Ativa municao de reserva sem consumo.";
			if (HasAny(key, {"infiniteclip", "infinite clip"}))
				return "Ativa o carregador sem consumir municao.";
			if (HasAny(key, {"nospread", "no spread"}))
				return "Ativa tiros com menor dispersao.";
			if (HasAny(key, {"autocock", "auto cock"}))
				return "Ativa o preparo automatico da arma entre disparos.";
			if (HasAny(key, {"keepgunsclean", "keep guns clean"}))
				return "Ativa a limpeza automatica das armas.";
			if (HasAny(key, {"npcignore", "npc ignore"}))
				return "Ativa para que personagens do jogo ignorem voce.";
			if (HasAny(key, {"eagleeye", "eagle eye"}))
				return "Ativa ou mantem a habilidade Olho de Aguia.";
			if (HasAny(key, {"whistle", "assobio"}))
				return "Ajusta o som e o comportamento do assobio.";
			if (HasAny(key, {"scale", "escala"}))
				return "Ajusta o tamanho visual do personagem ou montaria.";

			// Cavalo e veiculos.
			if (Has(key, "horse") && HasAny(key, {"bar", "core", "agitation"}))
				return "Ativa a manutencao automatica dos atributos do cavalo.";
			if (Has(key, "horse") && HasAny(key, {"clean", "limp"}))
				return "Ativa a limpeza automatica do cavalo.";
			if (Has(key, "horse") && HasAny(key, {"superrun", "super run"}))
				return "Ativa corrida mais rapida para o cavalo.";
			if (HasAny(key, {"flaminghooves", "flaming hooves"}))
				return "Ativa um efeito visual de fogo nos cascos.";
			if (HasAny(key, {"tpmounttoself", "mount to self"}))
				return "Executa o teleporte da montaria atual ate voce.";
			if (HasAny(key, {"repairvehicle", "repair vehicle"}))
				return "Executa o reparo do veiculo atual.";
			if (HasAny(key, {"superdrive", "super drive"}))
				return "Ativa um modo de conducao com impulso adicional.";
			if (HasAny(key, {"superbrake", "super brake"}))
				return "Ativa uma frenagem mais forte para o veiculo.";

			// Teleporte e navegacao.
			if (HasAny(key, {"autotp", "auto tp"}))
				return "Ativa o teleporte automatico quando a opcao exigir.";
			if (HasAny(key, {"waypoint", "map marker"}) && HasAny(key, {"tp", "teleport"}))
				return "Executa o teleporte para o ponto marcado no mapa.";
			if (HasAny(key, {"moonshine", "shack"}) && HasAny(key, {"tp", "teleport"}))
				return "Executa o teleporte para a cabana de moonshine.";
			if (HasAny(key, {"nazar", "madam"}) && HasAny(key, {"tp", "teleport"}))
				return "Executa o teleporte para a localizacao da Madame Nazar.";
			if (Has(key, "guarma") && HasAny(key, {"tp", "teleport"}))
				return "Executa o teleporte para Guarma.";
			if (Has(key, "train") && HasAny(key, {"tp", "teleport"}))
				return "Executa o teleporte para uma linha de trem proxima.";
			if (Has(key, "mount") && HasAny(key, {"tp", "teleport"}))
				return "Executa o teleporte para a montaria atual.";
			if (HasAny(key, {"teleport", "tpto", "tpinto", "tpbehind", "bring"}))
				return "Executa uma acao de teleporte ligada ao destino selecionado.";

			// Sessao, interface e utilidades gerais.
			if (HasAny(key, {"newsession", "new session"}))
				return "Executa a troca para uma nova sessao.";
			if (HasAny(key, {"blockalltelemetry", "block telemetry"}))
				return "Ativa o bloqueio de telemetria tratada por esta opcao.";
			if (HasAny(key, {"locklobby", "lock lobby"}))
				return "Configura o bloqueio de novas entradas na sessao atual.";
			if (HasAny(key, {"revealall", "reveal all"}))
				return "Mostra os jogadores da sessao quando o recurso estiver ativo.";
			if (HasAny(key, {"voice", "audio"}))
				return "Configura o comportamento de voz ou audio desta opcao.";
			if (HasAny(key, {"animation", "animacao"}))
				return "Configura a animacao que sera usada pelo personagem.";
			if (HasAny(key, {"music", "musica"}))
				return "Configura a musica ou evento de audio selecionado.";
			if (HasAny(key, {"weather", "clima"}))
				return "Configura o clima usado no mundo do jogo.";
			if (HasAny(key, {"time", "clock", "hora"}))
				return "Configura o horario usado no mundo do jogo.";
			if (HasAny(key, {"spawn", "spawner"}))
				return "Cria no jogo o item, personagem ou veiculo selecionado.";

			// Se a descricao original ja esta em portugues, conserva a informacao e apenas
			// adiciona um verbo reconhecido pelo painel inferior.
			if (LooksPortuguese(description))
				return "Configura este recurso: " + std::string(description);

			// Ultimo recurso: nunca deixa a caixa inferior em ingles nem repete o nome cru.
			return "Configura o comportamento deste recurso no jogo.";
		}
	}

	Command::Command(std::string name, std::string label, std::string description, int num_args) :
	    m_Name(name),
	    m_Label(label),
	    m_Description(BuildPortugueseDescription(name, label, description)),
	    m_NumArgs(num_args),
	    m_Hash(Joaat(name))
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
