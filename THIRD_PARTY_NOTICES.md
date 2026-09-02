# Avisos de terceiros

## BigBase

O menu principal do Lumen porta para sua arquitetura o sistema clássico de
navegação vertical da **BigBase**: pilha hierárquica de menu, barra de submenu,
lista paginada de opções, item selecionado, rodapé, descrição e ações de teclado.
As opções foram adaptadas aos `UIItem` e comandos existentes para preservar as
funcionalidades do Lumen e o contexto de Red Dead Redemption 2.

- Projeto de referência: [Pocakking/BigBaseV1](https://github.com/Pocakking/BigBaseV1)
- Autor e copyright informados no pacote: Copyright (c) 2018 Pocakking
- Licença do projeto de referência: MIT

O comportamento e a composição do sistema em `Code/UI` foram usados na
implementação do menu clássico. Nenhum asset ou backend gráfico da BigBase foi
incorporado. Em particular, não foram importados DirectX 11, swapchain, hooks de
apresentação, inicialização gráfica, natives ou código específico de Grand Theft
Auto V. O desenho usa apenas `ImDrawList`, fornecido pelos backends Vulkan e
DirectX 12 já existentes no Lumen. Dear ImGui permanece disponível somente para
editores complexos e componentes independentes como ESP, notificações e overlay.

Uma cópia da licença original da BigBase continua disponível junto ao material
de referência em `bigbase-master/bigbase-master/LICENSE`.
