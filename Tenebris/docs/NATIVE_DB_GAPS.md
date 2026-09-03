# Auditoria da RDR2 Native DB

Esta auditoria compara os recursos expostos no frontend do Tenebris com o snapshot
da RDR2 Native DB armazenado em `src/game/rdr/invoker/natives.json`. O snapshot
é a fonte local usada pelo gerador de `src/game/rdr/Natives.hpp` e corresponde à
base de referência publicada por alloc8or.

## Lacunas implementadas

| Área | Natives já disponíveis, mas sem opção direta | Nova opção |
| --- | --- | --- |
| Recuperação | `RESURRECT_PED`, `REVIVE_INJURED_PED`, `SET_ENTITY_HEALTH`, `RESTORE_PLAYER_STAMINA` | Restaurar personagem |
| Aparência | `CLEAR_PED_BLOOD_DAMAGE`, `_CLEAR_PED_BLOOD_DAMAGE_FACIAL` | Limpar personagem |
| Aparência | `_SET_RANDOM_OUTFIT_VARIATION` | Traje aleatório |
| Tarefas | `CLEAR_PED_TASKS_IMMEDIATELY` | Cancelar tarefas |
| Física | `SET_PED_TO_RAGDOLL` | Cair no chão |
| Posicionamento | `PLACE_ENTITY_ON_GROUND_PROPERLY` | Colocar no solo |
| Armas | `REMOVE_ALL_PED_WEAPONS` | Remover todas as armas |
| Lei | `GET_BOUNTY`, `SET_BOUNTY`, `CLEAR_BOUNTY` | Consultar, definir e limpar recompensa |
| Lei | `GET_WANTED_SCORE`, `SET_WANTED_SCORE`, `CLEAR_WANTED_SCORE` | Procura da lei ajustável de 0 a 5 |
| Lei | `_FORCE_LAW_ON_LOCAL_PLAYER_IMMEDIATELY`, `_SET_BOUNTY_HUNTER_PURSUIT_CLEARED` | Procura máxima e fim da perseguição |
| Atributos | `_SET_ATTRIBUTE_CORE_VALUE` | Preencher os três núcleos sob demanda |
| Jogador | `_SPECIAL_ABILITY_RESTORE_BY_AMOUNT`, `_SPECIAL_ABILITY_RESTORE_OUTER_RING` | Restaurar barra e anel do Olho da Morte |

> **Hostilidade online:** a Native DB não expõe uma função documentada para
> definir diretamente a hostilidade do perfil de baixa a alta. `WANTED_SCORE`
> controla a procura da lei e não deve ser apresentado como hostilidade online.
> O Tenebris possui eventos de notoriedade/press charges, mas eles não fornecem um
> setter determinístico e documentado para o nível de hostilidade do perfil.

## Segunda auditoria solicitada

Também foi tentada a consulta ao repositório
[`Halen84/rdr3-nativedb-data`](https://github.com/Halen84/rdr3-nativedb-data),
que redistribui os dados da Native DB de alloc8or. O proxy deste ambiente
recusou tanto Git quanto `raw.githubusercontent.com` com HTTP 403. Para não
inventar assinaturas, a comparação executável foi feita contra o snapshot
`src/game/rdr/invoker/natives.json` já versionado no Tenebris.

A nova varredura encontrou ainda controles de natação, ruído, humor, nível de
Dead Eye e incapacitação. Eles não foram expostos automaticamente: alguns têm
limites pouco documentados, outros alteram progressão ou exigem estado contínuo.
Nesta etapa entraram somente as duas ações cuja assinatura e uso puderam ser
confirmados no snapshot local: preencher núcleos e restaurar o Olho da Morte.

### O que significa "humor do jogador"

`_SET_PLAYER_MOOD` e `_GET_PLAYER_MOOD` leem ou definem um valor do enum
`ePedMood`. Trata-se do estado de apresentação/comportamento do personagem
(por exemplo, a disposição usada por animações e expressões), não de honra,
hostilidade, recompensa ou relacionamento entre jogadores. A própria Native DB
remete a enumeração para o repositório de flags do Halen84, portanto essa opção
só deve ser implementada depois de importar e validar os valores simbólicos; um
campo numérico sem nomes seria pouco seguro e pouco compreensível.

### 38 candidatos adicionais ainda não expostos

A lista abaixo exclui as funções já documentadas nas seções anteriores e também
foi comparada por nome com os usos existentes em `src`. "Candidato" não significa
que a função seja segura para sessões online: cada uma ainda precisa de limites,
restauração do valor original e teste em DX12/Vulkan antes de virar opção.

| Área | Native candidata | Possível opção no Tenebris |
|---|---|---|
| Movimento | `SET_SWIM_MULTIPLIER_FOR_PLAYER` | Multiplicador de natação (documentado até 1,49) |
| Regeneração | `SET_PLAYER_HEALTH_RECHARGE_MULTIPLIER` | Velocidade de regeneração de vida |
| Regeneração | `SET_PLAYER_STAMINA_RECHARGE_MULTIPLIER` | Velocidade de regeneração de vigor |
| Regeneração | `_SET_PLAYER_STAMINA_SPRINT_DEPLETION_MULTIPLIER` | Consumo de vigor ao correr |
| Furtividade | `SET_PLAYER_NOISE_MULTIPLIER` | Multiplicador de ruído geral |
| Furtividade | `SET_PLAYER_SNEAKING_NOISE_MULTIPLIER` | Ruído ao andar furtivamente |
| Dead Eye | `_SET_DEADEYE_ABILITY_LEVEL` | Nível local de Dead Eye (máximo documentado: 5) |
| Dead Eye | `_SET_DEADEYE_ABILITY_DEPLETION_DELAY` | Atraso antes do consumo de Dead Eye |
| Personagem | `_SET_PLAYER_MOOD` | Humor por valores nomeados de `ePedMood` |
| Incapacitação | `SET_PED_CAN_BE_INCAPACITATED` | Permitir ou impedir estado incapacitado |
| Incapacitação | `_SET_PED_INCAPACITATION_TOTAL_BLEED_OUT_DURATION` | Duração total do sangramento |
| Incapacitação | `_INCAPACITATED_REVIVE` | Reviver do estado incapacitado |
| Ped/NPC | `SET_PED_SHOOT_RATE` | Cadência de tiro de NPCs (0–1000) |
| Mundo | `PAUSE_CLOCK` | Pausar ou retomar o relógio |
| Mundo | `SET_CLOCK_TIME` | Definir hora, minuto e segundo |
| Mundo | `SET_WIND_SPEED` | Controlar velocidade do vento |
| População | `SET_RANDOM_TRAINS` | Ativar ou desativar trens aleatórios |
| População | `SET_RANDOM_BOATS` | Ativar ou desativar barcos aleatórios |
| População | `SET_RANDOM_VEHICLE_DENSITY_MULTIPLIER_THIS_FRAME` | Densidade de veículos por frame |
| População | `SET_SCENARIO_PED_DENSITY_MULTIPLIER_THIS_FRAME` | Densidade de pedestres de cenário |
| Armas | `SET_PED_DROPS_WEAPONS_WHEN_DEAD` | Controlar queda de armas de NPCs |
| Ped/NPC | `SET_PED_MONEY` | Definir dinheiro carregado por um ped |
| Controle | `SET_PLAYER_CONTROL` | Bloquear/restaurar controle com flags explícitas |
| Controle | `DISABLE_PLAYER_FIRING` | Bloquear disparos e combate do jogador |
| Combate | `SET_PLAYER_WEAPON_DAMAGE_MODIFIER` | Multiplicador de dano de armas |
| Combate | `SET_PLAYER_WEAPON_DEFENSE_MODIFIER` | Multiplicador de defesa contra armas |
| Física | `SET_PED_CAN_RAGDOLL_FROM_PLAYER_IMPACT` | Ragdoll causado por impacto do jogador |
| Montaria | `SET_PED_CAN_BE_KNOCKED_OFF_VEHICLE` | Queda de montaria/veículo |
| Animação | `SET_PED_CAN_PLAY_AMBIENT_ANIMS` | Permitir animações ambientes |
| Animação | `SET_PED_CAN_PLAY_AMBIENT_BASE_ANIMS` | Permitir animações ambientes básicas |
| Animação | `SET_PED_CAN_PLAY_GESTURE_ANIMS` | Permitir gestos |
| Câmera | `SHAKE_GAMEPLAY_CAM` | Aplicar efeito de tremor selecionável |
| Câmera | `STOP_GAMEPLAY_CAM_SHAKING` | Interromper tremor imediatamente |
| HUD | `DISPLAY_RADAR` | Mostrar ou ocultar radar |
| HUD | `HIDE_HUD_AND_RADAR_THIS_FRAME` | Modo limpo para captura de tela |
| Mapa | `SET_MINIMAP_FOW_REVEAL_COORDINATE` | Revelar neblina do mapa em uma coordenada |
| Mapa | `SET_RADAR_ZOOM` | Ajustar zoom do radar |
| Mapa | `SET_WAYPOINT_OFF` | Limpar waypoint atual |

As opções foram adicionadas à categoria **Personagem > Utilidades** e reutilizam
o sistema `Command`/`CommandItem`. Nenhum hook, pointer, backend gráfico ou native
de GTA V foi importado.

## Critérios usados

- priorizar natives nomeadas e documentadas no snapshot local;
- evitar hashes desconhecidos e assinaturas sem parâmetros compreendidos;
- reutilizar wrappers já gerados em `Natives.hpp`;
- validar a existência do `Ped` antes de chamar uma native;
- manter ações que alteram o inventário explicitamente identificadas no menu.

O banco contém milhares de natives. A presença de uma native não é suficiente
para transformá-la automaticamente em recurso: muitas são internas, dependem de
estado de missão ou possuem parâmetros ainda não documentados. Novas opções devem
seguir os mesmos critérios para evitar corromper scripts ou estado de sessão.

## Revisão de recompensa, sessão e diagnóstico

- O campo `bountyamount` agora aceita entrada digitada até `INT_MAX`; o limite
  antigo de 150.000 centavos era do Tenebris, não estava documentado na assinatura
  de `SET_BOUNTY`. O jogo/servidor ainda pode normalizar ou rejeitar valores.
- `NETWORK_SESSION_LEAVE_SESSION` sai da sessão atual; não mantém uma cópia solo
  da mesma sessão.
- `NETWORK_START_SOLO_TUTORIAL_SESSION` é explicitamente uma sessão solo de
  tutorial. Não há evidência suficiente de que seja uma forma segura de
  "esvaziar" uma sessão pública normal.
- Portanto, nenhuma opção de sessão solo foi adicionada nesta etapa. O Tenebris já
  possui `locklobby`, mas bloquear novas entradas não remove os jogadores que já
  estão conectados.
- O projeto já instala `SetUnhandledExceptionFilter`, cria um `StackTrace`, grava
  a exceção com nível fatal e força `Logger::FlushQueue()`. As novas ações de
  recompensa também registram no log o valor solicitado antes da native.
