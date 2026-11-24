<div align="center">

# Paldea Recomp

**Pokémon Scarlet e Violet 4.0.0 rodando nativo no PC com Windows. Sem emulador. Recompilação estática. 60 fps.**

[English](README.md) · **Português (Brasil)** · **Versão 1.0**

</div>

> [!IMPORTANT]
> Este repositório **não traz arquivos do jogo, chaves nem código da Nintendo**. Você usa os seus próprios arquivos do
> *Pokémon Scarlet* ou *Pokémon Violet* 4.0.0; o código do jogo é gerado e compilado no seu computador a partir deles.
> As ferramentas não têm trava nem verificação: o que você faz com elas é por sua conta e liberdade.

## O que é

O Paldea Recomp pega o executável de Switch do Pokémon Scarlet / Violet, traduz o código ARM64 para C antes da
execução (recompilação estática) e monta um programa normal de Windows. O sistema da Switch é trocado por um runtime
pequeno e a API gráfica (NVN) é traduzida para Vulkan. O jogo roda como qualquer jogo de PC, sem emulador.

## De onde veio (créditos)

| Parte | Origem |
|---|---|
| Base: recompilador, runtime, camada do sistema Switch, renderizador NVN→Vulkan, launcher | **ZA Recomp v0.1.27** (MIT, copyright Peiikai; a versão usada foi a do Hexveil404). Está como primeiro commit deste repositório. O README dele fica em [docs/](docs/README-ZA-Recomp.pt-BR.md). |
| Suporte ao Pokémon Violet 4.0.0, patches e serviços de memória | **PokeV-Recomp**, de [didslima](https://github.com/didslima) (com contribuições da comunidade, como Atterax). |
| Scarlet, 4K Shadows, as correções abaixo e esta organização | Este projeto. |

Mods da comunidade que o Paldea Recomp pode usar (**não estão aqui**, baixe no GameBanana): tradução para português
do Brasil, Improved Graphic Quality, 4K Shadows, Shiny Lock Removal, Dark Void Accuracy Fix e No Trade Evolution.
Cada um pertence ao seu autor.

## O que fizemos (v1.0)

1. **Juntamos as duas bases.** O Violet do PokeV-Recomp passou a rodar sobre o runtime mais novo do ZA Recomp
   (launcher, carregador de mods, renderizador melhor, upscalers DLSS/FSR/XeSS, tela de opções).
2. **Suporte ao Pokémon Scarlet.** O executável do Scarlet difere do Violet em só duas palavras, então reaproveita as
   mesmas bibliotecas do sistema já recompiladas. O ID de build dele (`421c5411b487eb4d`) é reconhecido pelo launcher,
   pelas tabelas de versão e pelo sistema de patches, e a biblioteca do jogo é gerada separada.
3. **Segundo módulo do jogo (`subsdk0`).** Nunca era carregado; agora é mapeado, relocado e iniciado, e as funções dele
   entram nas tabelas de despacho.
4. **Corrigimos um crash na inicialização** (ponteiro de thread nulo): toda thread agora recebe seu bloco local
   corretamente.
5. **Alocador e serviços do sistema.** Adicionamos o alocador que o jogo espera (família `malloc`/`free`/`memalign`,
   usada até o heap do jogo ficar pronto) e os serviços de gerenciador Bluetooth e de contexto de erro que o jogo usa
   no boot.
6. **4K Shadows funciona.** O mod pede um mapa de sombra de 128 MB, mas o pool de GPU do jogo é fixo em 268 MB e já
   tinha 145 MB em uso, então travava. Um patch novo sobe o pool para 400 MB e o runtime informa a memória extra
   (`LARECOMP_ARENA_EXTRA_MB`, padrão 192).
7. **60 fps estáveis com a velocidade certa do jogo.** Patches de relógio, duração dos slots e ritmo de quadros
   mantêm mundo, animações e caminhada na velocidade normal mesmo quando um quadro demora mais de 16,7 ms.
8. **Widescreen.** Hor+ em ultrawide e Vert+ em janelas altas.
9. **ID da biblioteca ligado aos mods.** O ID da biblioteca inclui um hash dos mods de código ligados (`.pchtxt`), então
   cada combinação de mods tem a sua biblioteca.

## O que melhorou

- Violet e Scarlet iniciam e chegam à primeira tela do próprio jogo, nativos (sem emulador).
- 60,0 fps estáveis medidos na máquina de teste, com Improved Graphic Quality e 4K
  Shadows ligados: Violet por 98 s e Scarlet por cerca de 2 min.
- O Violet com 4K Shadows não trava mais.
- A resolução acompanha a janela, com FSR 1 e SMAA disponíveis.

## Limites conhecidos (lista honesta)

- Só foram feitos testes curtos: as telas iniciais e cerca de dois minutos em cada jogo. Sessões longas, mundo aberto,
  batalhas e saves **ainda não foram verificados**. Espere bugs; relatos são bem-vindos.
- Os mods de dados do Scarlet (Shiny Lock Removal, Dark Void, No Trade Evolution) estão instalados, mas ainda não foram
  conferidos jogando.
- Só Windows. Testado em uma máquina.

## Como compilar e rodar

1. Pegue os arquivos do seu *Pokémon Scarlet* ou *Violet* 4.0.0 e coloque o `exefs/` e o `romfs.bin` extraídos numa
   pasta `extracted/` ao lado do programa (ficam no seu PC; `extracted/` está no .gitignore).
2. Compile o runtime com MSYS2 (UCRT64) e `make`, como em [docs/README-ZA-Recomp.pt-BR.md](docs/README-ZA-Recomp.pt-BR.md).
3. Gere e compile a biblioteca do jogo com `tools/gen.py` (usa o módulo main do Switch e o `tools/sv_patches.py`).
   Se o `make` não conseguir ligar o `lasgame.dll` no Windows, ligue à mão com
   `clang -shared -fuse-ld=lld ... @lasgame.dll.rsp`.
4. Rode `lasrecomp.exe --play`.

## Licença

MIT, como o projeto base (veja [LICENSE](LICENSE)). Pokémon é marca da Nintendo, Creatures Inc. e GAME FREAK inc.
Este projeto não é oficial e não tem ligação com elas.
