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

		// Verified against script_mp_rel/net_beat_manager.c for the current decompiled layout.
		// These are the first 16 Network Beat types; keeping the table explicit makes a
		// future game/global-layout update fail closed instead of silently selecting a
		// different activity.
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

		// net_beat_manager globals recovered from the same layout used by the current
		// RDO decompiled scripts. Every access is validated before use.
		constexpr auto kBeatManagerThread = ScriptGlobal(1051252).At(16).At(16);
		constexpr auto kBeatRuntime       = ScriptGlobal(1272030);
		constexpr auto kPlayerBeatData    = ScriptGlobal(1268861);

		enum class TriggerResult : int
		{
			Idle,
			Queued,
			Attempting,
			Submitted,
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
				case TriggerResult::Attempting: return "Enviando candidato ao net_beat_manager...";
				case TriggerResult::Submitted: return "Tentativa enviada. O jogo ainda pode recusar por local, horario ou outras regras do Beat.";
				case TriggerResult::ChanceMiss: return "A porcentagem desta tentativa nao passou; nenhum global foi alterado.";
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
				if (!candidateGlobal.CanAccess(true) || !scoreGlobal.CanAccess(true) || !candidateCount.CanAccess(true))
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
					// One randomized variation from every verified type. This avoids biasing
					// the random option toward event families that simply have more locations.
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
				auto* score     = scoreGlobal.As<float*>();
				const int originalCandidate = *candidate;
				const float originalScore   = *score;
				int lastForcedCandidate     = -1;
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

					// Keep the candidate above the local scanner long enough for the host
					// arbitration pass to see it. The manager still performs its own normal
					// eligibility/visibility/location checks before an activity is launched.
					for (int pulse = 0; pulse < 8; ++pulse)
					{
						*candidate = forcedCandidate;
						*score = 1000000.0f;
						ScriptMgr::Yield(std::chrono::milliseconds(75));
					}
				}

				// Do not leave a forced candidate resident. Restore only if our own value
				// is still present; if the game already replaced it, preserve the game's data.
				if (lastForcedCandidate >= 0 && *candidate == lastForcedCandidate)
				{
					*candidate = originalCandidate;
					*score = originalScore;
				}

				finish(TriggerResult::Submitted);
			});
		}
	}

	void RenderNetworkBeatsMenu()
	{
		static int selectedEntry = 0; // 0 = random, 1..N = verified beat
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
			ImGui::Text("Aleatorio usa 1 variacao de cada um dos 16 tipos verificados e deixa o manager validar.");
		}

		ImGui::Separator();
		ImGui::SliderInt("Chance da tentativa Tenebris", &chance, 1, 100, "%d%%");
		ImGui::TextWrapped("Esta porcentagem controla se o Tenebris envia a tentativa. Nao altera a chance interna da Rockstar. Mesmo em 100%%, o jogo ainda pode rejeitar o Beat por localizacao, horario, clima, cooldown ou outra regra de elegibilidade.");

		const bool allowed = solo && managerActive && managerHost && !s_TriggerBusy.load();
		if (!allowed)
			ImGui::BeginDisabled();

		if (ImGui::Button("Tentar agora (respeitar chance)"))
			QueueBeatAttempt(selectedType, automaticVariation, selectedVariation, chance, true);
		ImGui::SameLine();
		if (ImGui::Button("Forcar tentativa (100%)"))
			QueueBeatAttempt(selectedType, automaticVariation, selectedVariation, 100, false);

		if (!allowed)
			ImGui::EndDisabled();

		ImGui::Separator();
		ImGui::TextWrapped("Status: %s", ResultText(s_LastResult.load()));
		if (s_LastRoll.load() > 0)
			ImGui::Text("Ultimo sorteio Tenebris: %d/100", s_LastRoll.load());
		if (s_LastCandidate.load() >= 0)
			ImGui::Text("Ultimo candidato: %d | tipo %d | variacao %d", s_LastCandidate.load(), s_LastType.load(), s_LastVariation.load());

		ImGui::TextWrapped("Protecao: o acionamento e bloqueado fora de sessao solo e quando voce nao controla o net_beat_manager. O recurso nao tenta obter/roubar Script Host.");
	}
}
