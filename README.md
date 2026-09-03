<div align="center">

# ✦ T E N E B R I S ✦

### Um menu de ferramentas para Red Dead Redemption 2

Interface compacta, navegação por teclado e recursos organizados para estudo, testes e personalização.

</div>

---

## O que é o Tenebris?

O **Tenebris** é um projeto independente e educacional em C++ para **Red Dead Redemption 2**. Ele reúne ferramentas de personagem, teleporte, mundo, jogadores, rede, configurações e depuração em um menu clássico, leve e acessível por teclado.

O foco do projeto é aprender sobre interfaces, renderização, organização de software e integração com o jogo sem abandonar a clareza para quem apenas deseja entender e testar o menu.

## Destaques

- menu em português, compacto e adaptável à resolução;
- navegação por teclado com **F5** ou **Insert** para abrir e fechar;
- suporte aos renderizadores **Vulkan** e **DirectX 12**;
- categorias para personagem, teleporte, rede, jogadores, mundo, recuperação, configurações e depuração;
- sistema de comandos, atalhos, configurações persistentes e logs de diagnóstico;
- proteções, ESP, notificações e ferramentas experimentais.

## Instalação

1. Baixe uma versão publicada do Tenebris.
2. Extraia o conteúdo mantendo os arquivos do pacote juntos.
3. Leia as observações da versão antes de utilizar `Tenebris.dll`.
4. Abra ou feche o menu com **F5** ou **Insert**, conforme a configuração escolhida.

## Compilação

O projeto utiliza CMake e foi preparado para Windows 64-bit:

```bat
cmake -S Tenebris -B Tenebris/build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build Tenebris/build --config RelWithDebInfo --target Tenebris
```

Dependências externas podem exigir acesso à internet durante a primeira configuração.

## Relatórios de problemas

Ao relatar um problema, informe a resolução, versão do Windows, GPU, renderer utilizado, ação executada e anexe os logs disponíveis. Isso ajuda a distinguir erros do menu, do jogo, do driver e das dependências.

## Créditos

O Tenebris existe graças ao trabalho e ao aprendizado proporcionados por projetos da comunidade:

- **YimMenu** — arquitetura, padrões de comandos, proteções e base de conhecimento;
- **HorseMenu** — referência no ecossistema de menus para Red Dead Redemption 2;
- **BigBase** — referência para a navegação vertical clássica adotada pelo frontend;
- **alloc8or RDR3 Native DB** e seus colaboradores — documentação das natives utilizadas pelo projeto.

Os créditos e avisos de terceiros também estão registrados em [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).

## Uso responsável

Este software é fornecido para **fins educacionais, pesquisa e desenvolvimento**. O Tenebris não possui vínculo com Rockstar Games ou Take-Two Interactive. Não utilize o projeto para prejudicar jogadores, interromper serviços ou violar leis e termos aplicáveis.

O software é fornecido sem garantias. O usuário é responsável por seu uso, por seus dados e por eventuais consequências.

<div align="center">

### Tenebris

**Clareza, controle e aprendizado.**

</div>

## Verificação depois de uma fusão

Para detectar blocos duplicados, chaves quebradas e o retorno do tema antigo:

```bash
python Tenebris/tools/verify_frontend_structure.py
```
