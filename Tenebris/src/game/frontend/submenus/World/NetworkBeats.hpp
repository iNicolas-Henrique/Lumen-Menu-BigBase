#pragma once

// O gerador de natives desta base expoe o hash 0xB4A25351D79B444C
// pelo nome NETWORK_GET_HOST_OF_THREAD. Mantemos este alias local ao
// modulo de Network Beats para compatibilidade com o nome do decompilado.
#ifndef _0xB4A25351D79B444C
#define _0xB4A25351D79B444C NETWORK_GET_HOST_OF_THREAD
#endif

namespace YimMenu::Submenus
{
	void RenderNetworkBeatsMenu();
}
