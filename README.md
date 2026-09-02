# Lumen

> **Controle direto. Sem excesso.**

**Lumen** é um mod menu experimental para **Red Dead Redemption 2**, desenvolvido em C++ com foco em desempenho, organização e uma interface simples de navegar.

## Recursos

- Menu clássico e compacto
- Navegação por teclado
- Suporte a **Vulkan** e **DirectX 12**
- Sistema de comandos e configurações
- Ferramentas de personagem, mundo, teleporte, rede e recuperação
- Recursos experimentais baseados nas natives do jogo
- Interface em português

## Controles

Abra ou feche o menu com:

**F5** ou **INSERT**

## Compilação

Requisitos principais: **CMake**, **Ninja** e **Visual Studio Build Tools x64**.

```bat
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build -j 2
```

A DLL gerada será `Lumen.dll` dentro da pasta `build`.

## Compatibilidade

- Windows 64-bit
- Vulkan
- DirectX 12

A compatibilidade pode variar conforme a versão do jogo, drivers e hardware.

## Aviso

Lumen é um projeto independente criado para fins de estudo, pesquisa e desenvolvimento. Não possui vínculo com Rockstar Games ou Take-Two Interactive.

O software é fornecido sem garantia. O usuário é responsável pelo uso do projeto e pelo cumprimento das regras e termos aplicáveis.

## Licença

Consulte o arquivo `LICENSE` e `THIRD_PARTY_NOTICES.md` para informações sobre licenciamento e componentes de terceiros.

---

### Lumen

**Controle direto. Sem excesso.**
