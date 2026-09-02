# Lumen

**Lumen** é um mod menu para **Red Dead Redemption 2**, desenvolvido com foco em personalização, ferramentas de gameplay, interface moderna e recursos experimentais.

O projeto busca manter uma interface simples, organizada e leve, com suporte aos renderizadores utilizados pelo jogo.

---

## Sobre

O Lumen nasceu como um projeto de estudo e desenvolvimento em C++, com foco em aprender e experimentar diferentes áreas, como:

* Interface gráfica com ImGui
* Renderização em Vulkan
* Suporte a DirectX 12
* Hooks e integração com o jogo
* Organização de menus e submenus
* Sistema de configurações
* Desenvolvimento e manutenção de projetos em C++

O projeto ainda está em desenvolvimento, então algumas funções podem apresentar comportamentos inesperados dependendo da versão do jogo ou do hardware utilizado.

---

## Principais recursos

* Interface própria do Lumen
* Menu dividido em categorias
* Navegação simples e organizada
* Suporte a **Vulkan**
* Suporte a **DirectX 12**
* Compatibilidade melhorada com GPUs integradas
* Salvamento de configurações
* Sistema de comandos
* Diversas opções de personalização
* Ferramentas de gameplay
* Recursos experimentais
* Interface em português
* Projeto desenvolvido em C++

---

## Abrindo o menu

Por padrão, o menu pode ser aberto ou fechado utilizando:

**F5** ou **INSERT**

---

## Instalação

1. Baixe a versão mais recente do Lumen.
2. Extraia o arquivo `.zip`.
3. Leia os arquivos e instruções incluídos no pacote antes de utilizar.
4. Mantenha os arquivos necessários juntos para evitar problemas de funcionamento.

Caso esteja utilizando uma versão compilada, o arquivo principal será:

`Lumen.dll`

---

## Compilação

O projeto utiliza **CMake** e pode ser compilado utilizando Visual Studio Build Tools e Ninja.

Exemplo:

```bat
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build --config RelWithDebInfo --target Lumen
```

Após a compilação, a DLL será gerada dentro da pasta `build`.

---

## Compatibilidade

O Lumen foi desenvolvido pensando principalmente em sistemas Windows 64-bit.

Renderizadores atualmente suportados:

* Vulkan
* DirectX 12

A compatibilidade pode variar dependendo da versão do jogo, drivers de vídeo e hardware.

---

## Problemas e sugestões

Encontrou algum problema?

Antes de abrir um relatório, tente informar:

* O que aconteceu
* O que você estava fazendo quando o problema apareceu
* Sua versão do Windows
* Sua GPU
* Renderizador utilizado
* Logs ou mensagens de erro, caso existam

Quanto mais informações forem fornecidas, mais fácil será encontrar a causa.

Sugestões e melhorias também são bem-vindas.

---

## Aviso

O **Lumen** é um projeto independente criado para fins de estudo, pesquisa e desenvolvimento.

O projeto não possui vínculo, patrocínio ou afiliação com **Rockstar Games** ou **Take-Two Interactive**.

Red Dead Redemption, Red Dead Redemption 2, Red Dead Online, Rockstar Games e suas respectivas marcas pertencem aos seus proprietários.

O projeto não incentiva ações destinadas a prejudicar outros jogadores, interromper serviços online ou causar danos a terceiros.

Cada usuário é responsável pela forma como utiliza o software e pelo cumprimento das leis, regras e termos de serviço aplicáveis.

---

## Responsabilidade

O software é fornecido **sem garantia**.

O desenvolvedor não se responsabiliza por problemas, perda de dados, incompatibilidades, suspensões de conta ou quaisquer outras consequências relacionadas ao uso do projeto.

Use por sua própria conta e risco.

---

## Desenvolvimento

O Lumen continuará recebendo ajustes, correções e melhorias conforme o projeto evoluir.

A prioridade é manter o código organizado, a interface funcional e melhorar a compatibilidade sem transformar o projeto em algo desnecessariamente pesado.

---

### Lumen

**Mais controle. Menos limites.**
