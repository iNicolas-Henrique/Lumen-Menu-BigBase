#include "NetworkBeats.hpp"

#include "game/backend/FiberPool.hpp"
#include "game/backend/ScriptMgr.hpp"
#include "game/rdr/Natives.hpp"
#include "game/rdr/ScriptGlobal.hpp"

#include <algorithm>
#include <array>
#include <atomic>

namespace YimMenu::Submenus
{
	namespace
	{
		struct BeatDefinition
		{
			int Type;
			const char* Name;
			int FixedVariationCount; // 0 = ler a quantidade do runtime/datafile
		};

		// Mapeado diretamente de script_mp_rel/net_beat_manager.c.
		// Tipos 1..34 possuem quantidade fixa no proprio script.
		// Tipos 35..42 usam datafiles carregados em runtime; nunca chutamos a quantidade.
		// O tipo 43 permanece de fora porque o hash ainda nao foi nomeado com seguranca.
		constexpr std::array<BeatDefinition, 42> kVerifiedBeats = {{
		    {1, "Ataque de animal", 10},
		    {2, "Ferido por flecha", 20},
		    {3, "Protetor de ovos", 21},
		    {4, "Ladrao de tumulos", 11},
		    {5, "Coletor rival", 21},
		    {6, "Sequestro", 16},
		    {7, "Fotografo", 10},
		    {8, "Pessoa amarrada", 9},
		    {9, "Cacador de tesouro", 12},
		    {10, "Mapa em arvore", 20},
		    {11, "Carroca desgovernada", 10},
		    {12, "Andarilho com cachorro", 15},
		    {13, "Homem selvagem no acampamento", 1},
		    {14, "Duelo", 10},
		    {15, "Acampamento de moonshine", 12},
		    {16, "Mendigo", 15},
		    {17, "Cacador perseguidor", 11},
		    {18, "Cacador caido", 10},
		    {19, "Carroca acidentada", 7},
		    {20, "Armadilha de suspensao", 8},
		    {21, "Animal lendario: urso", 13},
		    {22, "Animal lendario: puma", 20},
		    {23, "Animal lendario: pantera", 15},
		    {24, "Animal lendario: javali", 11},
		    {25, "Animal lendario: bisao", 10},
		    {26, "Animal lendario: raposa", 20},
		    {27, "Animal lendario: lobo", 25},
		    {28, "Animal lendario: castor", 10},
		    {29, "Animal lendario: coiote", 10},
		    {30, "Animal lendario: alce (moose)", 10},
		    {31, "Animal lendario: jacare", 15},
		    {32, "Animal lendario: cervo", 10},
		    {33, "Animal lendario: carneiro", 10},
		    {34, "Animal lendario: elk", 10},
		    {35, "Esconderijo (dinamico)", 0},
		    {36, "Emboscada (dinamico)", 0},
		    {37, "Resgate multiplo (dinamico)", 0},
		    {38, "Escolta (dinamico)", 0},
		    {39, "Defesa de acampamento (dinamico)", 0},
		    {40, "Sabotagem de moonshine (dinamico)", 0},
		    {41, "Destruir moonshine (dinamico)", 0},
		    {42, "Bloqueio de estrada moonshine (dinamico)", 0},
		}};

		constexpr int kStaticCandidateCount = 438; // soma exata dos tipos 1..34
		constexpr int kMaxRuntimeCandidateCount = 817;
		constexpr float kHostChanceGateBypassScore = 1.5f;

		constexpr auto kBeatManagerThread = ScriptGlobal(1051252).At(16).At(16);
		constexpr auto kBeatRuntime = ScriptGlobal(1272030);
		constexpr auto kPlayerBeatData = ScriptGlobal(1268861);
		constexpr auto kDynamicBeatData = ScriptGlobal(1257541);

		enum class ControlAuthority : int
		{
			None,
			ScriptHost,
			SoloOverride,
		};

		enum class TriggerResult : int
		{
			Idle,
			Queued,
			WaitingForManager,
			Attempting,
			ManagerAccepted,
			SubmittedNoConfirmation,
			Cancelled,
			NotOnline,
			NotSolo,
			ManagerInactive,
			ManagerBusy,
			ManagerNotReady,
			GlobalsUnavailable,
			DynamicDataUnavailable,
			LayoutMismatch,
			Busy,
		};

		std::atomic_bool s_TriggerBusy{false};
		std::atomic_bool s_CancelRequested{false};
		std::atomic<TriggerResult> s_LastResult{TriggerResult::Idle};
		std::atomic_int s_LastType{0};
		std::atomic_int s_LastVariation{-1};
		std::atomic_int s_LastCandidate{-1};
		std::atomic_int s_LastManagerState{-1};

		int CountActivePlayers()
		{
			int count = 0;
			for (int i = 0; i < 32; ++i)
			{
				if (NETWORK::NETWORK_IS_PLAYER_ACTIVE(PLAYER::INT_TO_PLAYERINDEX(i)))
					++count;
			}
			return count;
		}

		bool IsBeatManagerActive()
		{
			return NETWORK::NETWORK_IS_GAME_IN_PROGRESS()
			    && NETWORK::NETWORK_IS_SCRIPT_ACTIVE("net_beat_manager", -1, true, 0);
		}

		int GetBeatManagerHostId()
		{
			if (!IsBeatManagerActive())
				return -1;

			// A native por nome consulta o host do script alvo, em vez do script
			// injetado do Tenebris. Esse e o caminho principal.
			const int hostByScript = static_cast<int>(NETWORK::NETWORK_GET_HOST_OF_SCRIPT("net_beat_manager", -1, 0));
			if (hostByScript >= 0 && hostByScript < 32)
				return hostByScript;

			// Fallback do layout atual: thread id usado pelo proprio manager.
			if (!kBeatManagerThread.CanAccess(true))
				return -1;

			const int threadId = *kBeatManagerThread.As<int*>();
			if (threadId <= 0)
				return -1;

			const int hostByThread = static_cast<int>(NETWORK::_0xB4A25351D79B444C(threadId));
			return (hostByThread >= 0 && hostByThread < 32) ? hostByThread : -1;
		}

		ControlAuthority GetControlAuthority()
		{
			if (!NETWORK::NETWORK_IS_GAME_IN_PROGRESS() || !IsBeatManagerActive())
				return ControlAuthority::None;

			if (CountActivePlayers() == 1)
				return ControlAuthority::SoloOverride;

			return GetBeatManagerHostId() == PLAYER::PLAYER_ID()
			    ? ControlAuthority::ScriptHost
			    : ControlAuthority::None;
		}

		const char* AuthorityText(ControlAuthority authority)
		{
			switch (authority)
			{
				case ControlAuthority::ScriptHost: return "SCRIPT HOST DO net_beat_manager";
				case ControlAuthority::SoloOverride: return "SOLO OVERRIDE (1 JOGADOR)";
				case ControlAuthority::None: return "SEM AUTORIDADE";
			}
			return "DESCONHECIDA";
		}

		const BeatDefinition* FindBeat(int type)
		{
			for (const auto& beat : kVerifiedBeats)
			{
				if (beat.Type == type)
					return &beat;
			}
			return nullptr;
		}

		int RuntimeVariationCount(const BeatDefinition& beat)
		{
			if (beat.FixedVariationCount > 0)
				return beat.FixedVariationCount;

			if (beat.Type < 35 || beat.Type > 42)
				return -1;

			// func_86 -> Global_1272030.f_3348[type]
			// func_188 -> Global_1257541[dataIndex /*5*/].f_3
			auto dataIndexGlobal = kBeatRuntime.At(3348).At(beat.Type);
			if (!dataIndexGlobal.CanAccess(true))
				return -1;

			const int dataIndex = *dataIndexGlobal.As<int*>();
			if (dataIndex < 0)
				return -1;

			auto countGlobal = kDynamicBeatData.At(dataIndex, 5).At(3);
			if (!countGlobal.CanAccess(true))
				return -1;

			const int count = *countGlobal.As<int*>();
			if (count <= 0 || count > kMaxRuntimeCandidateCount)
				return -1;

			return count;
		}

		int CandidateBaseForType(int type)
		{
			int base = 0;
			for (const auto& beat : kVerifiedBeats)
			{
				if (beat.Type == type)
					return base;

				const int count = RuntimeVariationCount(beat);
				if (count <= 0 || base > (kMaxRuntimeCandidateCount - count))
					return -1;
				base += count;
			}
			return -1;
		}

		const char* ResultText(TriggerResult result)
		{
			switch (result)
			{
				case TriggerResult::Idle: return "Pronto para controle manual.";
				case TriggerResult::Queued: return "Pedido exato colocado na fila.";
				case TriggerResult::WaitingForManager: return "Aguardando a janela de selecao do net_beat_manager...";
				case TriggerResult::Attempting: return "Forcando o candidato exato escolhido enquanto o manager esta em selecao...";
				case TriggerResult::ManagerAccepted: return "O net_beat_manager aceitou e entrou no ciclo do Beat escolhido.";
				case TriggerResult::SubmittedNoConfirmation: return "O candidato exato foi mantido na janela de selecao, mas o manager nao confirmou a execucao.";
				case TriggerResult::Cancelled: return "Pedido manual cancelado e valores que pertenciam ao Tenebris restaurados.";
				case TriggerResult::NotOnline: return "Red Dead Online nao esta em progresso.";
				case TriggerResult::NotSolo: return "Bloqueado: o controle forcado do Tenebris exige exatamente 1 jogador ativo.";
				case TriggerResult::ManagerInactive: return "net_beat_manager nao esta ativo nesta sessao.";
				case TriggerResult::ManagerBusy: return "O net_beat_manager ja esta avaliando/em cooldown de outro Beat.";
				case TriggerResult::ManagerNotReady: return "O manager nao abriu a janela de selecao durante a tentativa.";
				case TriggerResult::GlobalsUnavailable: return "Globais do Beat Manager indisponiveis; nenhuma escrita foi feita.";
				case TriggerResult::DynamicDataUnavailable: return "O datafile deste evento dinamico ainda nao forneceu uma contagem valida.";
				case TriggerResult::LayoutMismatch: return "Layout de globals diferente do esperado; recurso desativado por seguranca.";
				case TriggerResult::Busy: return "Ja existe um pedido manual Tenebris em andamento.";
			}
			return "Estado desconhecido.";
		}

		void QueueExactBeat(int selectedType, int selectedVariation)
		{
			if (s_TriggerBusy.exchange(true))
			{
				s_LastResult = TriggerResult::Busy;
				return;
			}

			s_CancelRequested = false;
			s_LastResult = TriggerResult::Queued;
			FiberPool::Push([selectedType, selectedVariation] {
				auto finish = [](TriggerResult result) {
					s_LastResult = result;
					s_CancelRequested = false;
					s_TriggerBusy = false;
				};

				if (!NETWORK::NETWORK_IS_GAME_IN_PROGRESS())
				{
					finish(TriggerResult::NotOnline);
					return;
				}
				if (CountActivePlayers() != 1)
				{
					finish(TriggerResult::NotSolo);
					return;
				}
				if (!IsBeatManagerActive())
				{
					finish(TriggerResult::ManagerInactive);
					return;
				}

				const auto* beat = FindBeat(selectedType);
				if (!beat)
				{
					finish(TriggerResult::LayoutMismatch);
					return;
				}

				const int variationCount = RuntimeVariationCount(*beat);
				if (variationCount <= 0)
				{
					finish(beat->FixedVariationCount == 0 ? TriggerResult::DynamicDataUnavailable : TriggerResult::LayoutMismatch);
					return;
				}
				if (selectedVariation < 0 || selectedVariation >= variationCount)
				{
					finish(TriggerResult::LayoutMismatch);
					return;
				}

				const int localSlot = PLAYER::NETWORK_PLAYER_ID_TO_INT();
				if (localSlot < 0 || localSlot >= 32)
				{
					finish(TriggerResult::GlobalsUnavailable);
					return;
				}

				auto candidateGlobal = kPlayerBeatData.At(localSlot, 99).At(92);
				auto scoreGlobal = kPlayerBeatData.At(localSlot, 99).At(93);
				auto candidateCount = kBeatRuntime.At(3270);
				auto managerState = kBeatRuntime.At(3279);
				if (!candidateGlobal.CanAccess(true) || !scoreGlobal.CanAccess(true) || !candidateCount.CanAccess(true) || !managerState.CanAccess(true))
				{
					finish(TriggerResult::GlobalsUnavailable);
					return;
				}

				const int runtimeCandidateCount = *candidateCount.As<int*>();
				if (runtimeCandidateCount < kStaticCandidateCount || runtimeCandidateCount > kMaxRuntimeCandidateCount)
				{
					finish(TriggerResult::LayoutMismatch);
					return;
				}

				auto* candidate = candidateGlobal.As<int*>();
				auto* score = scoreGlobal.As<float*>();
				auto* state = managerState.As<int*>();
				const int initialManagerState = *state;
				if (initialManagerState < 0 || initialManagerState > 3)
				{
					finish(TriggerResult::LayoutMismatch);
					return;
				}
				s_LastManagerState = initialManagerState;

				if (initialManagerState == 2 || initialManagerState == 3)
				{
					finish(TriggerResult::ManagerBusy);
					return;
				}

				const int base = CandidateBaseForType(selectedType);
				if (base < 0)
				{
					finish(TriggerResult::DynamicDataUnavailable);
					return;
				}

				const int forcedCandidate = base + selectedVariation;
				if (forcedCandidate < 0 || forcedCandidate >= runtimeCandidateCount)
				{
					finish(TriggerResult::LayoutMismatch);
					return;
				}

				// Espera somente a janela correta. Enquanto isso nao tocamos em globals.
				s_LastResult = TriggerResult::WaitingForManager;
				bool selectionWindow = (*state == 1);
				for (int frame = 0; !selectionWindow && frame < 180; ++frame)
				{
					if (s_CancelRequested.load())
					{
						finish(TriggerResult::Cancelled);
						return;
					}
					ScriptMgr::Yield();
					s_LastManagerState = *state;
					if (CountActivePlayers() != 1)
					{
						finish(TriggerResult::NotSolo);
						return;
					}
					if (!IsBeatManagerActive())
					{
						finish(TriggerResult::ManagerInactive);
						return;
					}
					if (*state == 2 || *state == 3)
					{
						finish(TriggerResult::ManagerBusy);
						return;
					}
					selectionWindow = (*state == 1);
				}
				if (!selectionWindow)
				{
					finish(TriggerResult::ManagerNotReady);
					return;
				}

				const int originalCandidate = *candidate;
				const float originalScore = *score;
				s_LastType = selectedType;
				s_LastVariation = selectedVariation;
				s_LastCandidate = forcedCandidate;
				s_LastResult = TriggerResult::Attempting;

				auto restoreOwnedValues = [&] {
					if (*candidate == forcedCandidate)
						*candidate = originalCandidate;
					if (*score == kHostChanceGateBypassScore)
						*score = originalScore;
				};

				bool managerAccepted = false;
				bool sawEvaluationState = false;
				bool submittedAtLeastOnce = false;

				// Modo deterministico: nao ha sorteio de tipo, variacao nem chance Tenebris.
				// Enquanto o manager continua em estado 1, reassertamos SOMENTE o candidato
				// escolhido pelo usuario. Isso impede o scanner local de trocar o pedido
				// antes da copia do manager, sem alterar offsets desconhecidos/nao verificados.
				for (int frame = 0; frame < 240; ++frame)
				{
					if (s_CancelRequested.load())
					{
						restoreOwnedValues();
						finish(TriggerResult::Cancelled);
						return;
					}
					if (CountActivePlayers() != 1)
					{
						restoreOwnedValues();
						finish(TriggerResult::NotSolo);
						return;
					}
					if (!IsBeatManagerActive())
					{
						restoreOwnedValues();
						finish(TriggerResult::ManagerInactive);
						return;
					}

					s_LastManagerState = *state;
					if (*state == 3)
					{
						managerAccepted = submittedAtLeastOnce;
						break;
					}
					if (*state == 2)
					{
						sawEvaluationState = true;
						ScriptMgr::Yield();
						continue;
					}
					if (sawEvaluationState && *state == 1)
					{
						// O manager avaliou o candidato e retornou a selecao: regras internas
						// do evento recusaram o spawn. Nao trocamos para outra variacao.
						break;
					}
					if (*state != 1)
						break;

					*candidate = forcedCandidate;
					*score = kHostChanceGateBypassScore;
					submittedAtLeastOnce = true;
					ScriptMgr::Yield();
				}

				restoreOwnedValues();
				finish(managerAccepted ? TriggerResult::ManagerAccepted : TriggerResult::SubmittedNoConfirmation);
			});
		}

		void CancelPendingBeat()
		{
			if (s_TriggerBusy.load())
				s_CancelRequested = true;
		}
	}

	void RenderNetworkBeatsMenu()
	{
		static int selectedEntry = 0; // indice direto em kVerifiedBeats; sem opcao aleatoria
		static int selectedVariation = 0;

		const int activePlayers = NETWORK::NETWORK_IS_GAME_IN_PROGRESS() ? CountActivePlayers() : 0;
		const bool managerActive = IsBeatManagerActive();
		const int managerHostId = managerActive ? GetBeatManagerHostId() : -1;
		const bool actualManagerHost = managerHostId >= 0 && managerHostId == PLAYER::PLAYER_ID();
		const bool solo = activePlayers == 1;
		const ControlAuthority authority = GetControlAuthority();

		ImGui::Text("Sessao solo: %s", solo ? "SIM" : "NAO");
		ImGui::Text("Jogadores ativos: %d", activePlayers);
		ImGui::Text("net_beat_manager: %s", managerActive ? "ATIVO" : "INATIVO");
		if (managerHostId >= 0)
			ImGui::Text("Host real do manager: jogador %d%s", managerHostId, actualManagerHost ? " (VOCE)" : "");
		else
			ImGui::TextUnformatted("Host real do manager: NAO RESOLVIDO");
		ImGui::Text("Autoridade Tenebris: %s", AuthorityText(authority));
		ImGui::Separator();

		selectedEntry = std::clamp(selectedEntry, 0, static_cast<int>(kVerifiedBeats.size()) - 1);
		const char* preview = kVerifiedBeats[selectedEntry].Name;
		if (ImGui::BeginCombo("Evento exato", preview))
		{
			for (int i = 0; i < static_cast<int>(kVerifiedBeats.size()); ++i)
			{
				const bool selected = selectedEntry == i;
				if (ImGui::Selectable(kVerifiedBeats[i].Name, selected))
				{
					selectedEntry = i;
					selectedVariation = 0;
				}
				if (selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		const auto& beat = kVerifiedBeats[selectedEntry];
		const int selectedType = beat.Type;
		const int variationCount = RuntimeVariationCount(beat);
		const bool selectionAvailable = variationCount > 0;

		if (beat.FixedVariationCount > 0)
			ImGui::Text("Tipo interno: %d | Variacoes verificadas: %d", beat.Type, variationCount);
		else if (selectionAvailable)
			ImGui::Text("Tipo interno: %d | Variacoes lidas do runtime: %d", beat.Type, variationCount);
		else
			ImGui::Text("Tipo interno: %d | Datafile dinamico ainda indisponivel", beat.Type);

		if (selectionAvailable)
		{
			selectedVariation = std::clamp(selectedVariation, 0, variationCount - 1);
			ImGui::SliderInt("Variacao EXATA", &selectedVariation, 0, variationCount - 1);
		}

		ImGui::TextWrapped("CONTROLE MANUAL: nao existe sorteio de evento, variacao ou porcentagem no caminho abaixo. O Tenebris envia somente o evento e a variacao escolhidos.");
		ImGui::Separator();

		// Mesmo que sejamos Script Host, o disparo forcado continua limitado a 1 jogador
		// para nao alterar a experiencia de terceiros. SoloOverride existe justamente para
		// a sessao privada em que a consulta de host pode ser inconclusiva.
		const bool allowed = solo && managerActive && selectionAvailable && !s_TriggerBusy.load();
		if (!allowed)
			ImGui::BeginDisabled();
		if (ImGui::Button("FORCAR AGORA - EVENTO EXATO"))
			QueueExactBeat(selectedType, selectedVariation);
		if (!allowed)
			ImGui::EndDisabled();

		ImGui::SameLine();
		const bool canCancelPending = s_TriggerBusy.load();
		if (!canCancelPending)
			ImGui::BeginDisabled();
		if (ImGui::Button("CANCELAR PEDIDO"))
			CancelPendingBeat();
		if (!canCancelPending)
			ImGui::EndDisabled();

		ImGui::Separator();
		ImGui::TextWrapped("Status: %s", ResultText(s_LastResult.load()));
		if (s_LastCandidate.load() >= 0)
			ImGui::Text("Ultimo candidato: %d | tipo %d | variacao %d", s_LastCandidate.load(), s_LastType.load(), s_LastVariation.load());
		if (s_LastManagerState.load() >= 0)
			ImGui::Text("Ultimo estado observado do manager: %d", s_LastManagerState.load());

		ImGui::TextWrapped("Importante: FORCAR AGORA remove a aleatoriedade do Tenebris e mantem o candidato escolhido durante a janela do manager. O script especifico do evento ainda pode recusar por regras proprias do jogo (local, streaming, visibilidade ou requisito do Beat). Nesse caso o Tenebris nao substitui silenciosamente por outro evento.");
		ImGui::TextWrapped("CANCELAR PEDIDO interrompe apenas uma injecao Tenebris ainda em andamento e restaura os valores que ele proprio escreveu. Cancelamento de um evento que JA FOI CRIADO nao e simulado aqui: o estado 3 do net_beat_manager e apenas cooldown, nao o lifecycle do script/evento ativo.");
		ImGui::TextWrapped("Protecao: o disparo forcado permanece bloqueado quando houver mais de 1 jogador ativo, mesmo se voce for o Script Host.");
	}
}
