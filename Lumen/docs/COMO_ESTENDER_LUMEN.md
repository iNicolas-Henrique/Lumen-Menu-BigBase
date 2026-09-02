# Como estender o Lumen

Este guia explica, de forma direta, como adicionar uma nova página ao menu e como ligar uma nova função ao sistema de comandos. Antes de começar, crie uma branch e faça um commit de segurança.

## Estrutura básica

- `src/game/frontend/Menu.cpp`: cria a janela principal e registra os submenus.
- `src/core/frontend/manager/`: desenha navegação, categorias e itens.
- `src/game/frontend/submenus/`: define as páginas visíveis.
- `src/game/frontend/items/`: adapta Commands para widgets ImGui.
- `src/game/commands/`: contém comandos específicos do jogo.
- `src/game/features/`: executa funções contínuas por tick.

O frontend apenas apresenta opções. A lógica deve permanecer em um Command, Feature ou serviço de backend; não duplique lógica de jogo dentro de um botão ImGui.

## Adicionar uma categoria a um submenu existente

1. Abra o `.cpp` do submenu desejado em `src/game/frontend/submenus/`.
2. Crie uma categoria com nome curto em português.
3. Adicione itens existentes à categoria.
4. Mova a categoria para `AddCategory`.

Exemplo simplificado:

```cpp
auto ferramentas = std::make_shared<Category>("Ferramentas");
ferramentas->AddItem(std::make_shared<BoolCommandItem>("meucomando"_J));
AddCategory(std::move(ferramentas));
```

Use um identificador único e estável para o Command. Alterar esse identificador depois pode quebrar configurações salvas e hotkeys.

## Adicionar um submenu novo

1. Crie `MinhaPagina.hpp` e `MinhaPagina.cpp` em `src/game/frontend/submenus/`.
2. Faça a classe herdar de `Submenu`.
3. No construtor, use `Submenu("Minha pagina")`.
4. Crie e adicione pelo menos uma `Category`.
5. Inclua o header em `src/game/frontend/Menu.cpp`.
6. Registre a página em `Menu::Init()`:

```cpp
UIManager::AddSubmenu(std::make_shared<Submenus::MinhaPagina>());
```

O `UIManager` colocará a nova página automaticamente na navegação lateral.

## Adicionar uma função simples

Escolha o tipo correto:

- Ação única: `Command` ou um item `Button`.
- Liga/desliga contínuo: `LoopedCommand`.
- Número inteiro: `IntCommand`.
- Número decimal: `FloatCommand`.
- Lista de escolhas: `ListCommand`.
- Texto: `StringCommand`.

Exemplo conceitual de ação única:

```cpp
class MinhaAcao : public Command
{
    using Command::Command;

    void OnCall() override
    {
        // Execute aqui apenas uma ação local e segura.
    }
};
```

Depois:

1. Registre o Command junto aos demais comandos do projeto.
2. Adicione um `CommandItem` ou o item tipado adequado à categoria.
3. Se a chamada exigir o script thread, envie a ação ao `FiberPool` em vez de executar diretamente durante o desenho.
4. Para uma opção contínua, mantenha a lógica no loop de Features e deixe o menu apenas alterar o estado do Command.

## Regras para não quebrar o mod

- Não chame natives pesados no frame de renderização.
- Não guarde ponteiros ImGui para objetos temporários.
- Não crie um segundo contexto ImGui.
- Não altere Vulkan/DX12 para adicionar uma opção de menu.
- Não remova Commands ou submenus existentes ao reorganizar a interface.
- Verifique ponteiros e entidades antes de usá-los.
- Evite trabalho de rede em callbacks de desenho.
- Preserve os IDs internos mesmo ao traduzir o texto mostrado ao usuário.

## Traduzir textos

Traduza somente labels, descrições, notificações e mensagens mostradas ao usuário. Não traduza:

- nomes de classes e namespaces;
- identificadores de Commands;
- hashes;
- nomes de natives;
- nomes exigidos por arquivos de configuração;
- IDs ocultos do ImGui usados para manter estado.

Quando uma tradução ficar grande demais, use uma frase curta que preserve a ação e o risco da opção.

## Aparência e imagem de fundo

O fundo visível é desenhado em `src/game/frontend/Menu.cpp` com transparência pelo draw list do ImGui. Isso mantém a aparência idêntica em Vulkan e DirectX 12, evita arquivos binários no Pull Request e dispensa duas rotas diferentes de upload de textura.

## Compilar pelo GitHub

1. Envie a branch e abra um Pull Request.
2. Aguarde o workflow **Build Lumen DLL**.
3. Corrija qualquer erro até a execução ficar verde.
4. Abra a execução e baixe o artifact `Lumen-Windows-x64-...`.
5. Dentro do ZIP estará `Lumen.dll`.
