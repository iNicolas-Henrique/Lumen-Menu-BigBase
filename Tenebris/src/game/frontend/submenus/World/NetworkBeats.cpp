#include "NetworkBeats.hpp"

#include "game/backend/FiberPool.hpp"
#include "game/backend/ScriptMgr.hpp"
#include "game/rdr/Natives.hpp"
#include "game/rdr/ScriptGlobal.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <random>
#include <vector>

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

		// func_166 do net_beat_manager possui uma barreira aleatoria somente enquanto
		// o score do candidato e menor que 1.5. Usar exatamente 1.5 remove apenas
		// essa barreira especifica; todos os demais filtros do jogo continuam valendo.
		constexpr float kHostChanceGateBypassScore = 1.5f;

		// Globals recuperados do mesmo layout do net_beat_manager decompilado.
		constexpr auto kBeatManagerThread = ScriptGlobal(1051252).At(16).At(16);
		constexpr auto kBeatRuntime = ScriptGlobal(1272030);
		constexpr auto kPlayerBeatData = ScriptGlobal(1268861);
		constexpr auto kDynamicBeatData = ScriptGlobal(1257541);

		enum class TriggerResult : int
		{
			Idle,
			Queued,
			Attempting,
			ManagerAccepted,
			SubmittedNoConfirmation,
			ChanceMiss,
			NotOnline,
			NotSolo,
			ManagerInactive,
			NotManagerHost,
			GlobalsUnavailable,
			DynamicDataUnavailable,
			LayoutMismatch,
			Busy,
		};

		std::atomic_bool s_TriggerBusy{false};
		std::atomic<TriggerResult> s_LastResult{TriggerResult::Idle};
		std::atomic_int s_LastRoll{0};
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

		bool IsBeatManagerHost()
		{
			if (!IsBeatManagerActive() || !kBeatManagerThread.CanAccess(true))
				return false;

			const int threadId = *kBeatManagerThread.As<int*>();
			if (threadId <= 0)
				return false;

			return PLAYER::PLAYER_ID() == NETWORK::_0xB4A25351D79B444C(threadId);
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

			// net_beat_manager::func_86 -> Global_1272030.f_3348[type]
			// net_beat_manager::func_188 -> Global_1257541[dataIndex /*5*/].f_3
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
				case TriggerResult::Idle: return "Pronto.";
				case TriggerResult::Queued: return "Tentativa colocada na fila.";
				case TriggerResult::Attempting: return "Entregando candidato ao net_beat_manager...";
				case TriggerResult::ManagerAccepted: return "O manager entrou no estado de execucao do Beat. Ainda podem existir validacoes do script especifico do evento.";
				case TriggerResult::SubmittedNoConfirmation: return "Candidato enviado, mas o manager nao confirmou a transicao durante a janela observada.";
				case TriggerResult::ChanceMiss: return "A rolagem configurada no Tenebris nao passou; nenhum global foi alterado.";
				case TriggerResult::NotOnline: return "Red Dead Online nao esta em progresso.";
				case TriggerResult::NotSolo: return "Bloqueado: esta funcao so aceita sessao solo (1 jogador ativo).";
				case TriggerResult::ManagerInactive: return "net_beat_manager nao esta ativo nesta sessao.";
				case TriggerResult::NotManagerHost: return "Bloqueado: voce nao e o Script Host do net_beat_manager.";
				case TriggerResult::GlobalsUnavailable: return "Globais do Beat Manager indisponiveis; nenhuma escrita foi feita.";
				case TriggerResult::DynamicDataUnavailable: return "O datafile deste evento dinamico ainda nao forneceu uma contagem valida; nenhuma escrita foi feita.";
				case TriggerResult::LayoutMismatch: return "Layout de globals diferente do esperado; recurso desativado por seguranca.";
				case TriggerResult::Busy: return "Ja existe uma tentativa em andamento.";
			}
			return "Estado desconhecido.";
		}

		void QueueBeatAttempt(int selectedType, bool automaticVariation, int selectedVariation, int chance, bool respectChance)
		{
			if (s_TriggerBusy.exchange(true))
			{
				s_LastResult = TriggerResult::Busy;
				return;
			}

			s_LastResult = TriggerResult::Queued;
			FiberPool::Push([selectedType, automaticVariation, selectedVariation, chance, respectChance] {
				auto finish = [](TriggerResult result) {
					s_LastResult = result;
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
				if (!IsBeatManagerHost())
				{
					finish(TriggerResult::NotManagerHost);
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

				const int initialManagerState = *managerState.As<int*>();
				if (initialManagerState < 0 || initialManagerState > 3)
				{
					finish(TriggerResult::LayoutMismatch);
					return;
				}
				s_LastManagerState = initialManagerState;

				std::random_device rd;
				std::mt19937 rng(rd());
				std::uniform_int_distribution<int> rollDistribution(1, 100);
				const int roll = rollDistribution(rng);
				s_LastRoll = roll;
				if (respectChance && roll > std::clamp(chance, 1, 100))
				{
					finish(TriggerResult::ChanceMiss);
					return;
				}

				std::vector<std::pair<int, int>> attempts; // type, variation
				if (selectedType == 0)
				{
					// Uma variacao sorteada de cada familia valida evita favorecer tipos que
					// simplesmente possuem mais pontos no mapa. Dinamicos sem datafile sao ignorados.
					for (const auto& beat : kVerifiedBeats)
					{
						const int variationCount = RuntimeVariationCount(beat);
						if (variationCount <= 0)
							continue;
						std::uniform_int_distribution<int> variationDistribution(0, variationCount - 1);
						attempts.emplace_back(beat.Type, variationDistribution(rng));
					}
					std::shuffle(attempts.begin(), attempts.end(), rng);
				}
				else
				{
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

					if (automaticVariation)
					{
						for (int variation = 0; variation < variationCount; ++variation)
							attempts.emplace_back(beat->Type, variation);
						std::shuffle(attempts.begin(), attempts.end(), rng);
					}
					else
					{
						attempts.emplace_back(beat->Type, std::clamp(selectedVariation, 0, variationCount - 1));
					}
				}

				if (attempts.empty())
				{
					finish(TriggerResult::DynamicDataUnavailable);
					return;
				}

				auto* candidate = candidateGlobal.As<int*>();
				auto* score = scoreGlobal.As<float*>();
				auto* state = managerState.As<int*>();
				const int originalCandidate = *candidate;
				const float originalScore = *score;
				int lastForcedCandidate = -1;
				bool managerAccepted = false;
				s_LastResult = TriggerResult::Attempting;

				for (const auto& [type, variation] : attempts)
				{
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
					if (!IsBeatManagerHost())
					{
						finish(TriggerResult::NotManagerHost);
						return;
					}

					const auto* beat = FindBeat(type);
					if (!beat)
						continue;

					const int variationCount = RuntimeVariationCount(*beat);
					const int base = CandidateBaseForType(type);
					if (base < 0 || variationCount <= 0 || variation < 0 || variation >= variationCount)
						continue;

					const int forcedCandidate = base + variation;
					if (forcedCandidate < 0 || forcedCandidate >= runtimeCandidateCount)
						continue;

					lastForcedCandidate = forcedCandidate;
					s_LastType = type;
					s_LastVariation = variation;
					s_LastCandidate = forcedCandidate;

					*candidate = forcedCandidate;
					*score = kHostChanceGateBypassScore;

					// O manager trabalha por estados 0..3. No decompilado, o estado 3 e
					// alcancado depois que func_53 encontra um candidato e envia a atividade.
					// Se o jogo substituir nosso candidato, nao lutamos contra a escrita dele.
					for (int pulse = 0; pulse < 8; ++pulse)
					{
						ScriptMgr::Yield(std::chrono::milliseconds(75));
						s_LastManagerState = *state;

						if (*state == 3)
						{
							managerAccepted = true;
							break;
						}

						if (*candidate != forcedCandidate)
							break;

						// Mantem somente o score minimo necessario; nao reescreve o candidato.
						*score = kHostChanceGateBypassScore;
					}

					if (managerAccepted)
						break;
				}

				// Nao deixa valor forcado residente. Restaura somente se o jogo ainda nao
				// substituiu o candidato; assim nunca sobrescrevemos uma atualizacao nova.
				if (lastForcedCandidate >= 0 && *candidate == lastForcedCandidate)
				{
					*candidate = originalCandidate;
					*score = originalScore;
				}

				finish(managerAccepted ? TriggerResult::ManagerAccepted : TriggerResult::SubmittedNoConfirmation);
			});
		}
	}

	void RenderNetworkBeatsMenu()
	{
		static int selectedEntry = 0; // 0 = aleatorio, 1..N = Beat verificado
		static int selectedVariation = 0;
		static bool automaticVariation = true;
		static int chance = 100;

		const int activePlayers = NETWORK::NETWORK_IS_GAME_IN_PROGRESS() ? CountActivePlayers() : 0;
		const bool managerActive = IsBeatManagerActive();
		const bool managerHost = managerActive && IsBeatManagerHost();
		const bool solo = activePlayers == 1;

		ImGui::Text("Sessao solo: %s", solo ? "SIM" : "NAO");
		ImGui::Text("Jogadores ativos: %d", activePlayers);
		ImGui::Text("net_beat_manager: %s", managerActive ? "ATIVO" : "INATIVO");
		ImGui::Text("Script Host do manager: %s", managerHost ? "SIM" : "NAO");
		ImGui::Separator();

		const char* preview = selectedEntry == 0 ? "Aleatorio (tipos verificados)" : kVerifiedBeats[selectedEntry - 1].Name;
		if (ImGui::BeginCombo("Evento", preview))
		{
			if (ImGui::Selectable("Aleatorio (tipos verificados)", selectedEntry == 0))
				selectedEntry = 0;

			for (int i = 0; i < static_cast<int>(kVerifiedBeats.size()); ++i)
			{
				const bool selected = selectedEntry == (i + 1);
				if (ImGui::Selectable(kVerifiedBeats[i].Name, selected))
				{
					selectedEntry = i + 1;
					selectedVariation = 0;
				}
				if (selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		int selectedType = 0;
		bool selectionAvailable = true;
		if (selectedEntry > 0)
		{
			const auto& beat = kVerifiedBeats[selectedEntry - 1];
			selectedType = beat.Type;
			const int variationCount = RuntimeVariationCount(beat);
			selectionAvailable = variationCount > 0;

			if (beat.FixedVariationCount > 0)
				ImGui::Text("Tipo interno: %d | Variacoes verificadas: %d", beat.Type, variationCount);
			else if (selectionAvailable)
				ImGui::Text("Tipo interno: %d | Variacoes lidas do runtime: %d", beat.Type, variationCount);
			else
				ImGui::Text("Tipo interno: %d | Datafile dinamico ainda indisponivel", beat.Type);

			ImGui::Checkbox("Variacao automatica", &automaticVariation);
			if (!automaticVariation && selectionAvailable)
			{
				selectedVariation = std::clamp(selectedVariation, 0, variationCount - 1);
				ImGui::SliderInt("Variacao", &selectedVariation, 0, variationCount - 1);
			}
		}
		else
		{
			ImGui::TextWrapped("Aleatorio sorteia entre todas as familias verificadas. Eventos dinamicos entram no sorteio somente quando o proprio jogo fornece uma contagem valida pelo datafile.");
		}

		ImGui::Separator();
		ImGui::SliderInt("Chance de disparar a tentativa", &chance, 1, 100, "%d%%");
		ImGui::TextWrapped("A porcentagem acima decide se o Tenebris entrega o candidato ao manager nesta tentativa. Quando entrega, usa score 1.5, que evita a barreira aleatoria especifica vista em func_166 do net_beat_manager. Isso NAO ignora localizacao, horario, clima, cooldown, visibilidade, recursos de rede ou regras do script do evento; portanto 100%% nao significa spawn garantido no mundo.");

		const bool allowed = solo && managerActive && managerHost && selectionAvailable && !s_TriggerBusy.load();
		if (!allowed)
			ImGui::BeginDisabled();

		if (ImGui::Button("Tentar agora (usar chance)"))
			QueueBeatAttempt(selectedType, automaticVariation, selectedVariation, chance, true);
		ImGui::SameLine();
		if (ImGui::Button("Forcar tentativa agora"))
			QueueBeatAttempt(selectedType, automaticVariation, selectedVariation, 100, false);

		if (!allowed)
			ImGui::EndDisabled();

		ImGui::Separator();
		ImGui::TextWrapped("Status: %s", ResultText(s_LastResult.load()));
		if (s_LastRoll.load() > 0)
			ImGui::Text("Ultima rolagem Tenebris: %d/100", s_LastRoll.load());
		if (s_LastCandidate.load() >= 0)
			ImGui::Text("Ultimo candidato: %d | tipo %d | variacao %d", s_LastCandidate.load(), s_LastType.load(), s_LastVariation.load());
		if (s_LastManagerState.load() >= 0)
			ImGui::Text("Ultimo estado observado do manager: %d", s_LastManagerState.load());

		ImGui::TextWrapped("Protecao: bloqueado fora de sessao solo e sem autoridade real sobre o net_beat_manager. O recurso nao tenta obter, migrar ou roubar Script Host.");
	}
}
