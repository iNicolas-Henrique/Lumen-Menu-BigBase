#include "NetworkBeats.hpp"

#include "game/backend/FiberPool.hpp"
#include "game/backend/ScriptMgr.hpp"
#include "game/pointers/Pointers.hpp"
#include "game/rdr/Natives.hpp"
#include "game/rdr/ScriptGlobal.hpp"

#include <script/scrThread.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>

namespace YimMenu::Submenus
{
	namespace
	{
		struct BeatDefinition
		{
			int Type;
			const char* Name;
			int FixedVariationCount; // 0 = ler a quantidade do runtime/datafile
			const char* ScriptName;   // nullptr para familias dinamicas sem script unico verificado
			int ExitStateLocal;       // indice absoluto da stack de Local_X.f_8; -1 = nao suportado
		};

		// Tipos/variacoes: script_mp_rel/net_beat_manager.c da mesma geracao de globals
		// usada pelo Tenebris (Global_1272030/1268861/1257541).
		// ExitStateLocal foi validado no respectivo NB_*/LA_*: func_3 encerra quando
		// Local_X.f_8 == 6 e o EntryFunction executa func_5() (cleanup) antes de terminar.
		constexpr std::array<BeatDefinition, 42> kVerifiedBeats = {{
		    {1, "Ataque de animal", 10, "nb_animal_attack", 1389},
		    {2, "Ferido por flecha", 20, "nb_arrowhead_injury", 118},
		    {3, "Protetor de ovos", 21, "nb_egg_protector", 171},
		    {4, "Ladrao de tumulos", 11, "nb_graverobber", 212},
		    {5, "Coletor rival", 21, "nb_rival_collector", 98},
		    {6, "Sequestro", 16, "nb_kidnapped", 430},
		    {7, "Fotografo", 10, "nb_photography", 955},
		    {8, "Pessoa amarrada", 9, "nb_tied_up_ped", 1494},
		    {9, "Cacador de tesouro", 12, "nb_treasure_hunter", 887},
		    {10, "Mapa em arvore", 20, "nb_tree_map", 115},
		    {11, "Carroca desgovernada", 10, "nb_runaway_wagon", 311},
		    {12, "Andarilho com cachorro", 15, "nb_hobo_dog", 108},
		    {13, "Homem selvagem no acampamento", 1, "nb_wildman", 527},
		    {14, "Duelo", 10, "nb_duel", 171},
		    {15, "Acampamento de moonshine", 12, "nb_moonshine_camp", 677},
		    {16, "Mendigo", 15, "nb_beggar", 807},
		    {17, "Cacador perseguidor", 11, "nb_stalking_hunter", 802},
		    {18, "Cacador caido", 10, "nb_slumped_hunter", 680},
		    {19, "Carroca acidentada", 7, "nb_crashed_wagon", 675},
		    {20, "Armadilha de suspensao", 8, "nb_suspension_trap", 1180},
		    {21, "Animal lendario: urso", 13, "la_bear", 1332},
		    {22, "Animal lendario: puma", 20, "la_cougar", 772},
		    {23, "Animal lendario: pantera", 15, "la_panther", 541},
		    {24, "Animal lendario: javali", 11, "la_boar", 1324},
		    {25, "Animal lendario: bisao", 10, "la_bison", 1030},
		    {26, "Animal lendario: raposa", 20, "la_fox", 863},
		    {27, "Animal lendario: lobo", 25, "la_wolf", 778},
		    {28, "Animal lendario: castor", 10, "la_beaver", 648},
		    {29, "Animal lendario: coiote", 10, "la_coyote", 613},
		    {30, "Animal lendario: alce (moose)", 10, "la_moose", 506},
		    {31, "Animal lendario: jacare", 15, "la_alligator", 728},
		    {32, "Animal lendario: cervo", 10, "la_buck", 837},
		    {33, "Animal lendario: carneiro", 10, "la_ram", 832},
		    {34, "Animal lendario: elk", 10, "la_elk", 800},
		    {35, "Esconderijo (dinamico)", 0, nullptr, -1},
		    {36, "Emboscada (dinamico)", 0, nullptr, -1},
		    {37, "Resgate multiplo (dinamico)", 0, nullptr, -1},
		    {38, "Escolta (dinamico)", 0, nullptr, -1},
		    {39, "Defesa de acampamento (dinamico)", 0, nullptr, -1},
		    {40, "Sabotagem de moonshine (dinamico)", 0, nullptr, -1},
		    {41, "Destruir moonshine (dinamico)", 0, nullptr, -1},
		    {42, "Bloqueio de estrada moonshine (dinamico)", 0, nullptr, -1},
		}};

		constexpr int kStaticCandidateCount = 438;
		constexpr int kMaxRuntimeCandidateCount = 817;
		constexpr float kHostChanceGateBypassScore = 1.5f;
		constexpr int kScriptStartWaitFrames = 900;
		constexpr int kActiveCancelWaitFrames = 600;

		constexpr auto kBeatManagerThread = ScriptGlobal(1051252).At(16).At(16);
		constexpr auto kBeatRuntime = ScriptGlobal(1272030);
		constexpr auto kPlayerBeatData = ScriptGlobal(1268861);
		constexpr auto kCandidateRuntime = ScriptGlobal(1266405);
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
			PreparingExactRequest,
			ManagerEvaluating,
			RequestSubmitted,
			ScriptStarted,
			DynamicRequestSubmitted,
			RejectedByGame,
			ScriptStartTimeout,
			Cancelled,
			ActiveBeatAlreadyRunning,
			ActiveCancelQueued,
			ActiveCancelRequested,
			ActiveCancelled,
			ActiveCancelUnsupported,
			ActiveCancelThreadMissing,
			ActiveCancelLayoutMismatch,
			ActiveCancelTimeout,
			NotOnline,
			NotSolo,
			ManagerInactive,
			ManagerBusy,
			GlobalsUnavailable,
			DynamicDataUnavailable,
			LayoutMismatch,
			Busy,
		};

		std::atomic_bool s_TriggerBusy{false};
		std::atomic_bool s_CancelRequested{false};
		std::atomic_bool s_PendingCancellable{false};
		std::atomic_bool s_ActiveCancelBusy{false};
		std::atomic<TriggerResult> s_LastResult{TriggerResult::Idle};
		std::atomic_int s_LastType{0};
		std::atomic_int s_LastVariation{-1};
		std::atomic_int s_LastCandidate{-1};
		std::atomic_int s_LastManagerState{-1};
		std::atomic_int s_LastConfirmedType{-1};
		std::atomic_int s_LastConfirmedVariation{-1};
		std::atomic_int s_LastConfirmedThreadId{-1};

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

			const int hostByScript = static_cast<int>(NETWORK::NETWORK_GET_HOST_OF_SCRIPT("net_beat_manager", -1, 0));
			if (hostByScript >= 0 && hostByScript < 32)
				return hostByScript;

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

			const int activePlayers = CountActivePlayers();
			if (activePlayers == 1)
			{
				return GetBeatManagerHostId() == PLAYER::PLAYER_ID()
				    ? ControlAuthority::ScriptHost
				    : ControlAuthority::SoloOverride;
			}

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

		std::uint32_t JoaatLower(const char* text)
		{
			std::uint32_t hash = 0;
			if (!text)
				return 0;

			while (*text)
			{
				unsigned char c = static_cast<unsigned char>(*text++);
				if (c >= 'A' && c <= 'Z')
					c = static_cast<unsigned char>(c + ('a' - 'A'));
				hash += c;
				hash += (hash << 10);
				hash ^= (hash >> 6);
			}
			hash += (hash << 3);
			hash ^= (hash >> 11);
			hash += (hash << 15);
			return hash;
		}

		bool IsBeatScriptActive(const BeatDefinition& beat)
		{
			return beat.ScriptName
			    && NETWORK::NETWORK_IS_SCRIPT_ACTIVE(beat.ScriptName, -1, true, 0);
		}

		rage::scrThread* FindScriptThread(const BeatDefinition& beat)
		{
			if (!beat.ScriptName || !Pointers.ScriptThreads)
				return nullptr;

			const auto targetHash = JoaatLower(beat.ScriptName);
			for (auto* thread : *Pointers.ScriptThreads)
			{
				if (!thread || thread->m_Context.m_ThreadId == 0)
					continue;
				if (static_cast<std::uint32_t>(thread->m_Context.m_ScriptHash) == targetHash)
					return thread;
			}
			return nullptr;
		}

		const BeatDefinition* FindActiveKnownBeat()
		{
			for (const auto& beat : kVerifiedBeats)
			{
				if (beat.ScriptName && IsBeatScriptActive(beat))
					return &beat;
			}
			return nullptr;
		}

		const char* ResultText(TriggerResult result)
		{
			switch (result)
			{
				case TriggerResult::Idle: return "Pronto para controle manual.";
				case TriggerResult::Queued: return "Pedido exato colocado na fila.";
				case TriggerResult::PreparingExactRequest: return "Preparando somente o candidato/variacao escolhidos.";
				case TriggerResult::ManagerEvaluating: return "net_beat_manager avaliando o evento exato.";
				case TriggerResult::RequestSubmitted: return "Request oficial do manager enviada; aguardando o script exato nascer.";
				case TriggerResult::ScriptStarted: return "SUCESSO: o script exato do evento escolhido esta ATIVO.";
				case TriggerResult::DynamicRequestSubmitted: return "Evento dinamico enviado pelo manager; familia dinamica nao possui script unico mapeado para confirmacao.";
				case TriggerResult::RejectedByGame: return "Evento exato recusado pelas regras do jogo. Nenhum outro evento foi escolhido.";
				case TriggerResult::ScriptStartTimeout: return "O manager enviou/avaliou o pedido, mas o script exato nao ficou ativo dentro da janela de confirmacao.";
				case TriggerResult::Cancelled: return "Pedido ainda pendente cancelado antes da criacao confirmada do evento.";
				case TriggerResult::ActiveBeatAlreadyRunning: return "Ja existe um Network Beat conhecido ativo. Cancele/conclua-o antes de iniciar outro.";
				case TriggerResult::ActiveCancelQueued: return "Cancelamento seguro do evento ativo colocado na fila.";
				case TriggerResult::ActiveCancelRequested: return "O proprio script do evento recebeu estado de saida 6; aguardando cleanup.";
				case TriggerResult::ActiveCancelled: return "Evento ativo encerrado pelo cleanup do proprio script.";
				case TriggerResult::ActiveCancelUnsupported: return "Cancelamento seguro ainda nao suportado para esta familia de evento.";
				case TriggerResult::ActiveCancelThreadMissing: return "Script ativo detectado, mas a thread correspondente nao foi localizada.";
				case TriggerResult::ActiveCancelLayoutMismatch: return "Stack/local de saida nao corresponde ao layout verificado; nenhuma escrita foi feita.";
				case TriggerResult::ActiveCancelTimeout: return "Pedido de encerramento foi feito, mas o script nao terminou dentro da janela de confirmacao.";
				case TriggerResult::NotOnline: return "Red Dead Online nao esta em progresso.";
				case TriggerResult::NotSolo: return "Bloqueado: este controle exige exatamente 1 jogador ativo.";
				case TriggerResult::ManagerInactive: return "net_beat_manager nao esta ativo nesta sessao.";
				case TriggerResult::ManagerBusy: return "net_beat_manager esta avaliando/em cooldown de outro Beat.";
				case TriggerResult::GlobalsUnavailable: return "Globais necessarios indisponiveis; nenhuma escrita foi feita.";
				case TriggerResult::DynamicDataUnavailable: return "O datafile deste evento dinamico ainda nao forneceu uma contagem valida.";
				case TriggerResult::LayoutMismatch: return "Layout/valor diferente do esperado; operacao abortada por seguranca.";
				case TriggerResult::Busy: return "Ja existe uma operacao Tenebris em andamento.";
			}
			return "Estado desconhecido.";
		}

		bool ValidateManagerGlobals(int runtimeCandidateCount)
		{
			if (runtimeCandidateCount < kStaticCandidateCount || runtimeCandidateCount > kMaxRuntimeCandidateCount)
				return false;

			if (!kBeatRuntime.At(3279).CanAccess(true)
			    || !kBeatRuntime.At(3280).CanAccess(true)
			    || !kCandidateRuntime.At(2452).At(1).CanAccess(true))
				return false;

			for (int i = 0; i < 32; ++i)
			{
				if (!kBeatRuntime.At(3281).At(i).CanAccess(true)
				    || !kBeatRuntime.At(3314).At(i).CanAccess(true))
					return false;
			}

			for (int i = 0; i < runtimeCandidateCount; ++i)
			{
				if (!kCandidateRuntime.At(i, 3).At(1).CanAccess(true))
					return false;
			}
			return true;
		}

		void QueueExactBeat(int selectedType, int selectedVariation)
		{
			if (s_ActiveCancelBusy.load() || s_TriggerBusy.exchange(true))
			{
				s_LastResult = TriggerResult::Busy;
				return;
			}

			s_CancelRequested = false;
			s_PendingCancellable = true;
			s_LastResult = TriggerResult::Queued;

			FiberPool::Push([selectedType, selectedVariation] {
				auto finish = [](TriggerResult result) {
					s_LastResult = result;
					s_CancelRequested = false;
					s_PendingCancellable = false;
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
				if (FindActiveKnownBeat())
				{
					finish(TriggerResult::ActiveBeatAlreadyRunning);
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

				auto candidateCountGlobal = kBeatRuntime.At(3270);
				auto managerStateGlobal = kBeatRuntime.At(3279);
				auto managerIndexGlobal = kBeatRuntime.At(3280);
				auto localCandidateGlobal = kPlayerBeatData.At(localSlot, 99).At(92);
				auto localScoreGlobal = kPlayerBeatData.At(localSlot, 99).At(93);
				auto beatFlagsGlobal = kCandidateRuntime.At(2452).At(1);

				if (!candidateCountGlobal.CanAccess(true)
				    || !managerStateGlobal.CanAccess(true)
				    || !managerIndexGlobal.CanAccess(true)
				    || !localCandidateGlobal.CanAccess(true)
				    || !localScoreGlobal.CanAccess(true)
				    || !beatFlagsGlobal.CanAccess(true))
				{
					finish(TriggerResult::GlobalsUnavailable);
					return;
				}

				const int runtimeCandidateCount = *candidateCountGlobal.As<int*>();
				if (!ValidateManagerGlobals(runtimeCandidateCount))
				{
					finish(TriggerResult::LayoutMismatch);
					return;
				}

				const int base = CandidateBaseForType(selectedType);
				if (base < 0)
				{
					finish(beat->FixedVariationCount == 0 ? TriggerResult::DynamicDataUnavailable : TriggerResult::LayoutMismatch);
					return;
				}
				const int forcedCandidate = base + selectedVariation;
				if (forcedCandidate < 0 || forcedCandidate >= runtimeCandidateCount)
				{
					finish(TriggerResult::LayoutMismatch);
					return;
				}

				auto* state = managerStateGlobal.As<int*>();
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

				if (s_CancelRequested.load())
				{
					finish(TriggerResult::Cancelled);
					return;
				}
				if (CountActivePlayers() != 1)
				{
					finish(TriggerResult::NotSolo);
					return;
				}

				// Daqui em diante o handoff e imediato e nao existe rollback seguro da
				// maquina de estados do jogo. CANCELAR PEDIDO fica desabilitado antes
				// da primeira escrita; depois do spawn usa-se CANCELAR EVENTO ATIVO.
				s_PendingCancellable = false;

				s_LastType = selectedType;
				s_LastVariation = selectedVariation;
				s_LastCandidate = forcedCandidate;
				s_LastResult = TriggerResult::PreparingExactRequest;

				auto* localCandidate = localCandidateGlobal.As<int*>();
				auto* localScore = localScoreGlobal.As<float*>();
				const int originalLocalCandidate = *localCandidate;
				const float originalLocalScore = *localScore;

				// Caminho forcado deterministico, espelhando o trecho oficial de func_52:
				// func_75: limpa lista de candidatos do host.
				// func_76: zera flag f_1 de cada candidato runtime.
				// f_3280 = 0.
				// func_77: em sessao solo copiaria apenas nosso candidato/score.
				// func_78(2, false): limpa o bit 2 de Global_1266405.f_2452.f_1.
				// func_73(2): entra em avaliacao.
				//
				// O score 1.5 e proposital: o proprio manager so usa a rolagem aleatoria
				// quando score < 1.5, removendo a chance sem escolher outro candidato.
				for (int i = 0; i < 32; ++i)
				{
					auto hostCandidate = kBeatRuntime.At(3281).At(i);
					auto hostScore = kBeatRuntime.At(3314).At(i);
					*hostCandidate.As<int*>() = -1;
					*hostScore.As<float*>() = 0.0f;
				}
				for (int i = 0; i < runtimeCandidateCount; ++i)
				{
					auto candidateFlag = kCandidateRuntime.At(i, 3).At(1);
					*candidateFlag.As<int*>() = 0;
				}

				*localCandidate = forcedCandidate;
				*localScore = kHostChanceGateBypassScore;
				*kBeatRuntime.At(3281).At(0).As<int*>() = forcedCandidate;
				*kBeatRuntime.At(3314).At(0).As<float*>() = kHostChanceGateBypassScore;
				*managerIndexGlobal.As<int*>() = 0;

				auto* beatFlags = beatFlagsGlobal.As<int*>();
				*beatFlags &= ~(1 << 2);

				// Estado escrito por ultimo: ate aqui qualquer falha ainda nao entregou
				// a solicitacao ao manager. Depois daqui os valores passam a ser do jogo.
				*state = 2;
				s_LastManagerState = 2;
				s_LastResult = TriggerResult::ManagerEvaluating;

				// Restaura apenas os dois campos client-side se ainda forem exatamente nossos.
				// A lista host/state NAO e restaurada depois do handoff: o manager agora e dono dela.
				auto restoreLocalOwnedValues = [&] {
					if (*localCandidate == forcedCandidate)
						*localCandidate = originalLocalCandidate;
					if (*localScore == kHostChanceGateBypassScore)
						*localScore = originalLocalScore;
				};

				bool sawState3 = false;
				bool sawReturnToSelection = false;

				for (int frame = 0; frame < kScriptStartWaitFrames; ++frame)
				{
					ScriptMgr::Yield();

					if (CountActivePlayers() != 1)
					{
						restoreLocalOwnedValues();
						finish(TriggerResult::NotSolo);
						return;
					}
					if (!IsBeatManagerActive())
					{
						restoreLocalOwnedValues();
						finish(TriggerResult::ManagerInactive);
						return;
					}

					const int currentState = *state;
					s_LastManagerState = currentState;
					if (currentState < 0 || currentState > 3)
					{
						restoreLocalOwnedValues();
						finish(TriggerResult::LayoutMismatch);
						return;
					}

					if (beat->ScriptName && IsBeatScriptActive(*beat))
					{
						restoreLocalOwnedValues();
						auto* thread = FindScriptThread(*beat);
						s_LastConfirmedType = selectedType;
						s_LastConfirmedVariation = selectedVariation;
						s_LastConfirmedThreadId = thread ? static_cast<int>(thread->m_Context.m_ThreadId) : -1;
						finish(TriggerResult::ScriptStarted);
						return;
					}

					if (currentState == 3)
					{
						sawState3 = true;
						s_LastResult = TriggerResult::RequestSubmitted;
						if (!beat->ScriptName)
						{
							restoreLocalOwnedValues();
							finish(TriggerResult::DynamicRequestSubmitted);
							return;
						}
					}
					else if (currentState == 1 && !sawState3)
					{
						sawReturnToSelection = true;
						break;
					}
				}

				restoreLocalOwnedValues();
				if (sawReturnToSelection)
					finish(TriggerResult::RejectedByGame);
				else
					finish(TriggerResult::ScriptStartTimeout);
			});
		}

		void CancelPendingBeat()
		{
			if (s_TriggerBusy.load() && s_PendingCancellable.load())
				s_CancelRequested = true;
		}

		void QueueCancelActiveBeat(int type)
		{
			if (s_TriggerBusy.load() || s_ActiveCancelBusy.exchange(true))
			{
				s_LastResult = TriggerResult::Busy;
				return;
			}

			s_LastResult = TriggerResult::ActiveCancelQueued;
			FiberPool::Push([type] {
				auto finish = [](TriggerResult result) {
					s_LastResult = result;
					s_ActiveCancelBusy = false;
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

				const auto* beat = FindBeat(type);
				if (!beat || !beat->ScriptName || beat->ExitStateLocal < 0)
				{
					finish(TriggerResult::ActiveCancelUnsupported);
					return;
				}
				if (!IsBeatScriptActive(*beat))
				{
					finish(TriggerResult::ActiveCancelThreadMissing);
					return;
				}

				auto* thread = FindScriptThread(*beat);
				if (!thread || !thread->m_Stack || thread->m_Context.m_ThreadId == 0)
				{
					finish(TriggerResult::ActiveCancelThreadMissing);
					return;
				}

				const std::uint32_t stackSize = thread->m_Context.m_StackSize;
				if (beat->ExitStateLocal < 0 || static_cast<std::uint32_t>(beat->ExitStateLocal) >= stackSize)
				{
					finish(TriggerResult::ActiveCancelLayoutMismatch);
					return;
				}

				auto* stack = reinterpret_cast<std::int64_t*>(thread->m_Stack);
				auto* exitState = &stack[beat->ExitStateLocal];
				const std::int64_t currentState = *exitState;

				// Os scripts verificados usam 0..6 para este lifecycle. Nao escrevemos
				// se o slot nao parecer o mesmo Local_X.f_8 do decomp atual.
				if (currentState < 0 || currentState > 6)
				{
					finish(TriggerResult::ActiveCancelLayoutMismatch);
					return;
				}
				if (CountActivePlayers() != 1)
				{
					finish(TriggerResult::NotSolo);
					return;
				}

				*exitState = 6;
				s_LastResult = TriggerResult::ActiveCancelRequested;

				for (int frame = 0; frame < kActiveCancelWaitFrames; ++frame)
				{
					ScriptMgr::Yield();
					if (!IsBeatScriptActive(*beat))
					{
						if (s_LastConfirmedType.load() == type)
						{
							s_LastConfirmedType = -1;
							s_LastConfirmedVariation = -1;
							s_LastConfirmedThreadId = -1;
						}
						finish(TriggerResult::ActiveCancelled);
						return;
					}
				}

				finish(TriggerResult::ActiveCancelTimeout);
			});
		}
	}

	void RenderNetworkBeatsMenu()
	{
		static int selectedEntry = 0;
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

		ImGui::TextWrapped("CONTROLE MANUAL: o Tenebris entrega somente o evento e a variacao escolhidos. Score 1.5 remove a rolagem de chance do manager; se as regras proprias do evento recusarem local/streaming/requisito, nenhum outro evento e usado.");
		ImGui::Separator();

		const bool operationBusy = s_TriggerBusy.load() || s_ActiveCancelBusy.load();
		const bool allowed = solo && managerActive && selectionAvailable && !operationBusy;
		if (!allowed)
			ImGui::BeginDisabled();
		if (ImGui::Button("FORCAR AGORA - EVENTO EXATO"))
			QueueExactBeat(beat.Type, selectedVariation);
		if (!allowed)
			ImGui::EndDisabled();

		ImGui::SameLine();
		const bool canCancelPending = s_TriggerBusy.load() && s_PendingCancellable.load();
		if (!canCancelPending)
			ImGui::BeginDisabled();
		if (ImGui::Button("CANCELAR PEDIDO"))
			CancelPendingBeat();
		if (!canCancelPending)
			ImGui::EndDisabled();

		ImGui::Separator();
		ImGui::TextUnformatted("EVENTO ATIVO");

		const BeatDefinition* activeBeat = FindActiveKnownBeat();
		rage::scrThread* activeThread = activeBeat ? FindScriptThread(*activeBeat) : nullptr;
		if (activeBeat)
		{
			ImGui::Text("Nome: %s", activeBeat->Name);
			ImGui::Text("Tipo: %d", activeBeat->Type);
			ImGui::Text("Script: %s", activeBeat->ScriptName);
			ImGui::TextUnformatted("Estado: ACTIVE");
			ImGui::Text("Thread: %d", activeThread ? static_cast<int>(activeThread->m_Context.m_ThreadId) : -1);

			if (s_LastConfirmedType.load() == activeBeat->Type && s_LastConfirmedVariation.load() >= 0)
				ImGui::Text("Variacao: %d (rastreada pelo Tenebris)", s_LastConfirmedVariation.load());
			else
				ImGui::TextUnformatted("Variacao: nao rastreada (evento nao confirmado por este disparo)");

			const bool canCancelActive = solo
			    && activeBeat->ExitStateLocal >= 0
			    && !operationBusy;
			if (!canCancelActive)
				ImGui::BeginDisabled();
			if (ImGui::Button("CANCELAR EVENTO ATIVO"))
				QueueCancelActiveBeat(activeBeat->Type);
			if (!canCancelActive)
				ImGui::EndDisabled();

			if (activeBeat->ExitStateLocal < 0)
				ImGui::TextWrapped("Cancelamento seguro ainda nao mapeado para este script.");
		}
		else
		{
			ImGui::TextUnformatted("Nenhum NB_*/LA_* conhecido ativo.");
			ImGui::BeginDisabled();
			ImGui::Button("CANCELAR EVENTO ATIVO");
			ImGui::EndDisabled();
		}

		ImGui::Separator();
		ImGui::TextWrapped("Status: %s", ResultText(s_LastResult.load()));
		if (s_LastCandidate.load() >= 0)
			ImGui::Text("Ultimo candidato: %d | tipo %d | variacao %d", s_LastCandidate.load(), s_LastType.load(), s_LastVariation.load());
		if (s_LastManagerState.load() >= 0)
			ImGui::Text("Ultimo estado observado do manager: %d", s_LastManagerState.load());

		ImGui::TextWrapped("Confirmacao: estado 3 do net_beat_manager e apenas cooldown/request entregue. O Tenebris so mostra SUCESSO para tipos 1..34 quando NETWORK_IS_SCRIPT_ACTIVE confirma o script exato escolhido.");
		ImGui::TextWrapped("CANCELAR EVENTO ATIVO nao mata a thread. Ele grava 6 somente no Local_X.f_8 previamente verificado daquele script, fazendo o proprio NB_*/LA_* sair do loop e executar seu cleanup normal antes de terminar.");
		ImGui::TextWrapped("Protecao: qualquer escrita forcada/cancelamento e bloqueada se houver mais de 1 jogador ativo.");
	}
}
