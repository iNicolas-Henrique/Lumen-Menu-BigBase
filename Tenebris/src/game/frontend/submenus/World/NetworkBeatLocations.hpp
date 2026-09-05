#pragma once

#include <array>
#include <string>

namespace YimMenu::Submenus::NetworkBeatLocations
{
	// A ORDEM destes arrays segue os cases de func_109..func_124 e
	// func_139..func_142 em script_mp_rel/net_beat_manager.c (geracao 1.23/1311.12),
	// isto e, o indice daqui e o MESMO selectedVariation entregue ao manager.
	//
	// Os nomes legiveis foram cruzados com data/encounters.json do Jean Ropke RDOMap.
	// Para os poucos cases cujo decomp so conserva o hash, o ponto foi casado pelas
	// coordenadas do proprio manager com o marcador correspondente do RDOMap.
	// Este arquivo e SOMENTE apresentacao: ele nao altera candidato, score, globals,
	// host, cooldown ou qualquer outra parte do disparo do Network Beat.

	constexpr std::array<const char*, 10> kType1 = {{
	    "Roanoke Ridge #1", "Roanoke Ridge #2", "Big Valley #1", "Big Valley #2",
	    "Big Valley #3", "Big Valley #4", "Heartlands #1", "Heartlands #2",
	    "Great Plains #1", "Grizzlies West #1",
	}};

	constexpr std::array<const char*, 20> kType2 = {{
	    "MacFarlane's Ranch", "Heartlands #1", "Grizzlies #1", "Roanoke #1",
	    "Bayou #1", "Scarlett Meadows #1", "Cumberland #1", "Strawberry #1",
	    "Great Plains #1", "Tall Trees #1", "Hennigan's Stead #1", "Rio Bravo #1",
	    "Cholla Springs #1", "Gaptooth #1", "Thieves' Landing #1", "Grizzlies #2",
	    "Grizzlies #3", "Valentine #1", "Emerald #1", "Bolger Glade #1",
	}};

	constexpr std::array<const char*, 21> kType3 = {{
	    "Roanoke Ridge #1", "Roanoke Ridge #2", "Roanoke Ridge #3",
	    "Heartlands #1", "Heartlands #2", "Heartlands #3",
	    "Big Valley #1", "Big Valley #2", "Big Valley #3",
	    "Great Plains #1", "Great Plains #2", "Great Plains #3",
	    "Grizzlies #1", "Grizzlies #2", "Grizzlies #3",
	    "Bluewater Marsh #1", "Bluewater Marsh #2", "Bluewater Marsh #3",
	    "New Austin #1", "New Austin #2", "New Austin #3",
	}};

	constexpr std::array<const char*, 11> kType4 = {{
	    "Valentine #1", "Rhodes #1", "Blackwater #1", "Tumbleweed #1",
	    "Odd Fellows Rest #1", "Coot's Chapel #1", "Ringneck Creek #1",
	    "Pleasance #1", "Shady Belle #1", "Saint Denis #1", "Old Tom's Blind #1",
	}};

	constexpr std::array<const char*, 21> kType5 = {{
	    "Bolger Glade #1", "Eris Field #1", "Roanoke Ridge #1", "Moonstone Pond #1",
	    "Cotorra Springs #1", "Cumberland #1", "Beartooth #1", "Flatneck #1",
	    "Little Creek #1", "Upper Montana River #1", "Blackwater #1", "Aurora Basin #1",
	    "Great Plains #1", "Manteca Falls #1", "MacFarlane's Ranch #1", "San Luis #1",
	    "Hennigan's Stead #1", "Jorge's Gap #1", "Fort Mercer #1", "Benedict Point #1",
	    "Gaptooth Ridge #1",
	}};

	constexpr std::array<const char*, 16> kType6 = {{
	    "Cumberland Forest #1", "Cumberland Forest #2", "Bluewater Marsh #1", "Bluewater Marsh #2",
	    "Bayou Nwa #1", "Bayou Nwa #2", "Grizzlies East #1", "Grizzlies East #2",
	    "Grizzlies West #1", "Grizzlies West #2", "Great Plains #1", "Great Plains #2",
	    "Hennigan's Stead #1", "Hennigan's Stead #2", "Rio Bravo #1", "Rio Bravo #2",
	}};

	constexpr std::array<const char*, 10> kType7 = {{
	    "Heartlands Cliff #1", "Heartlands River #1", "Tall Trees #1", "Tall Trees #2",
	    "Big Valley #1", "Big Valley #2", "Big Valley #3", "Rio Bravo #1",
	    "Hennigan's Stead #1", "Cholla Springs #1",
	}};

	constexpr std::array<const char*, 9> kType8 = {{
	    "Scarlett Meadows #1", "Scarlett Meadows #2", "Scarlett Meadows #3",
	    "Scarlett Meadows #4", "Scarlett Meadows #5", "Scarlett Meadows #6",
	    "Scarlett Meadows #7", "Scarlett Meadows #8", "Scarlett Meadows #9",
	}};

	constexpr std::array<const char*, 12> kType9 = {{
	    "Big Valley #0", "Big Valley #1", "Big Valley #2", "Big Valley #3",
	    "Big Valley #4", "Big Valley #5", "Big Valley #6", "Heartlands #0",
	    "Heartlands #1", "Heartlands #2", "Heartlands #3", "Heartlands #4",
	}};

	constexpr std::array<const char*, 20> kType10 = {{
	    "MacFarlane's Ranch #1", "Thieves' Landing #1", "Hanging Rock #1", "Del Lobo #1",
	    "Beecher's Hope #1", "Blackwater #1", "Montana River #1", "Monto's Rest #1",
	    "Watson's Cabin #1", "Dakota River #1", "Flatneck #1", "Emerald Ranch #1",
	    "Emerald Ranch #2", "Dreamcatcher Tree #1", "Cumberland #1", "Fort Wallace #1",
	    "O'Creagh's Run #1", "Moonstone Pond #1", "Kamassa River #1", "Van Horn #1",
	}};

	constexpr std::array<const char*, 10> kType11 = {{
	    "Emerald Ranch #1", "Lemoyne #1", "Riggs Station #1", "Blackwater #1",
	    "Rio Bravo #1", "Hennigan's Stead #1", "Great Plains #1", "Strawberry #1",
	    "Scarlett Meadows #1", "Bolger Glade #1",
	}};

	constexpr std::array<const char*, 15> kType12 = {{
	    "Van Horn #1", "Bolger Glade #1", "Bayou #1", "Annesburg #1", "Emerald #1",
	    "Cumberland #1", "Flatneck #1", "Wallace #1", "Strawberry #1", "Quaker's Cove #1",
	    "MacFarlane's Ranch #1", "Don Julio #1", "Armadillo #1", "Plainview #1", "Tumbleweed #1",
	}};

	constexpr std::array<const char*, 1> kType13 = {{"Acampamento do jogador"}};

	constexpr std::array<const char*, 10> kType14 = {{
	    "Valentine #1", "Valentine #2", "Van Horn #1", "Van Horn #2", "Rhodes #1",
	    "Rhodes #2", "Annesburg #1", "Annesburg #2", "Blackwater #1", "Blackwater #2",
	}};

	constexpr std::array<const char*, 12> kType15 = {{
	    "Bayou #1", "Bayou #2", "Big Valley #1", "Cumberland #1", "Cumberland #2",
	    "Heartlands #1", "Heartlands #2", "Heartlands #3", "Roanoke #1", "Roanoke #2",
	    "Scarlett Meadows #1", "Scarlett Meadows #2",
	}};

	constexpr std::array<const char*, 15> kType16 = {{
	    "Saint Denis #0", "Saint Denis #1", "Saint Denis #2", "Saint Denis #3",
	    "Saint Denis #4", "Saint Denis #5", "Saint Denis #6", "Saint Denis #7",
	    "Saint Denis #8", "Saint Denis #9", "Saint Denis #10", "Saint Denis #11",
	    "Blackwater #0", "Blackwater #1", "Blackwater #2",
	}};

	constexpr std::array<const char*, 11> kType17 = {{
	    "Big Valley #0", "Big Valley #1", "Big Valley #2", "Great Plains #0", "Great Plains #1",
	    "Grizzlies East #0", "Grizzlies East #1", "Grizzlies East #2", "Grizzlies East #3",
	    "Tall Trees #0", "Tall Trees #1",
	}};

	constexpr std::array<const char*, 10> kType18 = {{
	    "Cairn Lake #1", "Rio Bravo #1", "Cholla Springs #1", "Hennigan's Stead #1",
	    "Tall Trees #1", "Cumberland #1", "Twin Stack Pass #1", "O'Creagh's Run #1",
	    "Annesburg #1", "Bolger Glade #1",
	}};

	constexpr std::array<const char*, 7> kType19 = {{
	    "Big Valley #1", "Big Valley #2", "Big Valley #3", "Big Valley #4",
	    "Scarlett Meadows #1", "Scarlett Meadows #2", "Scarlett Meadows #3",
	}};

	constexpr std::array<const char*, 8> kType20 = {{
	    "Hennigan's Stead #1", "Hennigan's Stead #2", "Hennigan's Stead #3", "Hennigan's Stead #4",
	    "MacFarlane's Ranch #1", "Armadillo #1", "Rio Bravo #1", "Rio Bravo #2",
	}};

	template <std::size_t N>
	inline const char* At(const std::array<const char*, N>& locations, int variation)
	{
		return variation >= 0 && variation < static_cast<int>(N) ? locations[variation] : nullptr;
	}

	inline const char* Get(int type, int variation)
	{
		switch (type)
		{
			case 1: return At(kType1, variation);
			case 2: return At(kType2, variation);
			case 3: return At(kType3, variation);
			case 4: return At(kType4, variation);
			case 5: return At(kType5, variation);
			case 6: return At(kType6, variation);
			case 7: return At(kType7, variation);
			case 8: return At(kType8, variation);
			case 9: return At(kType9, variation);
			case 10: return At(kType10, variation);
			case 11: return At(kType11, variation);
			case 12: return At(kType12, variation);
			case 13: return At(kType13, variation);
			case 14: return At(kType14, variation);
			case 15: return At(kType15, variation);
			case 16: return At(kType16, variation);
			case 17: return At(kType17, variation);
			case 18: return At(kType18, variation);
			case 19: return At(kType19, variation);
			case 20: return At(kType20, variation);
			default: return nullptr;
		}
	}

	inline std::string Display(int type, int variation)
	{
		if (const char* location = Get(type, variation))
			return std::string(location) + "  [var " + std::to_string(variation) + "]";
		return "Variacao " + std::to_string(variation) + "  [local ainda nao mapeado]";
	}
}
