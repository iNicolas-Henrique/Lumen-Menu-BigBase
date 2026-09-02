# Avisos de terceiros

## BigBase

O frontend do Lumen usa a organização visual da **BigBase** como referência de
experiência: cabeçalho com identidade clara, indicação de contexto, hierarquia
entre navegação e opções, destaque inequívoco do item ativo e rodapé com estado.
Esses conceitos foram reinterpretados para a arquitetura ImGui existente do
Lumen e para o contexto de Red Dead Redemption 2.

- Projeto de referência: [Pocakking/BigBaseV1](https://github.com/Pocakking/BigBaseV1)
- Autor e copyright informados no pacote: Copyright (c) 2018 Pocakking
- Licença do projeto de referência: MIT

Nenhum arquivo, asset, widget ou trecho de código da BigBase foi incorporado ao
novo frontend. Em particular, não foram importados o backend DirectX 11, a
swapchain, hooks de apresentação, inicialização gráfica ou código específico de
Grand Theft Auto V. A implementação usa somente ImGui e as abstrações já
existentes no Lumen, mantendo seus backends Vulkan e DirectX 12.

Uma cópia da licença original da BigBase continua disponível junto ao material
de referência em `bigbase-master/bigbase-master/LICENSE`.
