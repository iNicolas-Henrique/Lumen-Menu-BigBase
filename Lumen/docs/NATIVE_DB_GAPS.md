# Auditoria da RDR2 Native DB

Esta auditoria compara os recursos expostos no frontend do Lumen com o snapshot
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
| Lei | `GET_WANTED_SCORE`, `SET_WANTED_SCORE`, `CLEAR_WANTED_SCORE` | Hostilidade ajustável de 0 a 5 |
| Lei | `_FORCE_LAW_ON_LOCAL_PLAYER_IMMEDIATELY`, `_SET_BOUNTY_HUNTER_PURSUIT_CLEARED` | Hostilidade máxima e fim da perseguição |

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
