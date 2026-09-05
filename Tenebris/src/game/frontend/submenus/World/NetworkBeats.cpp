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
			int VariationCount;
		};

		// Mapeado diretamente de script_mp_rel/net_beat_manager.c.
		// Mantemos apenas a faixa 1..16 porque seus tamanhos/variacoes sao fixos e
		// verificaveis no script. Isso evita calcular offsets de tipos posteriores
		// que dependem de tabelas/datafiles carregados em runtime.
		constexpr std::array<BeatDefinition, 16> kVerifiedBeats = {{
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
		}};

		constexpr int VerifiedCandidateCount()
		{
			int total = 0;
			for (const auto& beat : kVerifiedBeats)
				total += beat.VariationCount;
			return total;
		}

		constexpr int kVerifiedCandidateCount = VerifiedCandidateCount(); // 213

		// func_166 do net_beat_manager possui uma barreira aleatoria somente enquanto
		// o score do candidato e menor que 1.5. Usar exatamente 1.5 evita o valor
		// exagerado usado na primeira versao e remove apenas essa barreira especifica;
		// todos os demais filtros do jogo continuam valendo.
		constexpr float kHostChanceGateBypassScore = 1.5f;

		// Globals recuperados do mesmo layout do net_beat_manager decompilado.
		constexpr auto kBeatManagerThread = ScriptGlobal(1051252).At(16).At(16);
		constexpr auto kBeatRuntime       = ScriptGlobal(1272030);
		constexpr auto kPlayerBeatData    = ScriptGlobal(1268861);

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

		int CandidateBaseForType(int type)
		{
			int base = 0;
			for (const auto& beat : kVerifiedBeats)
			{
				if (beat.Type == type)
					return base;
				base += beat.VariationCount;
			}
			return -1;
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
				auto scoreGlobal     = kPlayerBeatData.At(localSlot, 99).At(93);
				auto candidateCount  = kBeatRuntime.At(3270);
				auto managerState    = kBeatRuntime.At(3279);
				if (!candidateGlobal.CanAccess(true) || !scoreGlobal.CanAccess(true) || !candidateCount.CanAccess(true) || !managerState.CanAccess(true))
				{
					finish(TriggerResult::GlobalsUnavailable);
					return;
				}

				const int runtimeCandidateCount = *candidateCount.As<int*>();
				if (runtimeCandidateCount < kVerifiedCandidateCount || runtimeCandidateCount > 817)
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
					// Uma variacao sorteada de cada familia evita favorecer tipos que apenas
					// possuem mais pontos no mapa.
					for (const auto& beat : kVerifiedBeats)
					{
						std::uniform_int_distribution<int> variationDistribution(0, beat.VariationCount - 1);
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

					if (automaticVariation)
					{
						for (int variation = 0; variation < beat->VariationCount; ++variation)
							attempts.emplace_back(beat->Type, variation);
						std::shuffle(attempts.begin(), attempts.end(), rng);
					}
					else
					{
						attempts.emplace_back(beat->Type, std::clamp(selectedVariation, 0, beat->VariationCount - 1));
					}
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

					const int base = CandidateBaseForType(type);
					const auto* beat = FindBeat(type);
					if (base < 0 || !beat || variation < 0 || variation >= beat->VariationCount)
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
					// Se o jogo substituir nosso candidato antes disso, nao lutamos contra
					// a escrita dele: abandonamos esta variacao e testamos a seguinte.
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

						// Mantem apenas o score minimo necessario; nao reescreve o candidato.
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
		if (selectedEntry > 0)
		{
			const auto& beat = kVerifiedBeats[selectedEntry - 1];
			selectedType = beat.Type;
			ImGui::Text("Tipo interno: %d | Variacoes conhecidas: %d", beat.Type, beat.VariationCount);
			ImGui::Checkbox("Variacao automatica", &automaticVariation);
			if (!automaticVariation)
			{
				selectedVariation = std::clamp(selectedVariation, 0, beat.VariationCount - 1);
				ImGui::SliderInt("Variacao", &selectedVariation, 0, beat.VariationCount - 1);
			}
		}
		else
		{
			ImGui::TextWrapped("Aleatorio sorteia entre as 16 familias verificadas. Cada familia recebe uma variacao antes de a ordem ser embaralhada.");
		}

		ImGui::Separator();
		ImGui::SliderInt("Chance de disparar a tentativa", &chance, 1, 100, "%d%%");
		ImGui::TextWrapped("A porcentagem acima decide se o Tenebris entrega o candidato ao manager nesta tentativa. Quando entrega, usa score 1.5, que evita a barreira aleatoria especifica vista em func_166 do net_beat_manager. Isso NAO ignora localizacao, horario, clima, cooldown, visibilidade, recursos de rede ou regras do script do evento; portanto 100%% nao significa spawn garantido no mundo.");

		const bool allowed = solo && managerActive && managerHost && !s_TriggerBusy.load();
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
