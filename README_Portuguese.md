[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)


[![Blitzkrieg Trailer](Blitzkrieg.png)](https://www.youtube.com/watch?v=zNxMvTcsJbk)

Este repositório é um projeto pessoal de Johannes Maria Frank. Uso-o por diversão e para aprender, em especial para praticar programação com agentes em aplicações já existentes (brownfield). O README original está preservado em `readme_original`.

Aviso: este projeto está em andamento.


# O que este repositório faz hoje

- Contém o código-fonte completo do modo para um jogador de Blitzkrieg e os dados do jogo.

- Compila o jogo inteiro com `zig build` (Zig + clang) para seis alvos — Windows (MSVC e MinGW), macOS (Intel e Apple Silicon) e Linux (x86_64 e arm64) — e gera uma instalação executável em `zig-out/game/<os>/<arch>/<config>`. Só o alvo MSVC precisa do Visual Studio; o Zig traz a própria toolchain para os demais.

- Compila e executa um Game.exe nativo de **64 bits** (`zig build install-game -Dtarget=x86_64-windows-msvc`); os alvos de 32 bits foram aposentados. As missões de campanha e os tutoriais são jogáveis em x64, com salvar/carregar, vídeo e som.

- Acompanha uma StreamIO.dll totalmente substituída por uma implementação em Zig (`Sources/src/StreamIOZig`) — toda a camada de persistência do motor: grafos de objetos binários para salvar e carregar, árvores de dados XML, a fábrica de objetos, variáveis globais, console e opções.

- Compila o C++ legado com clang e UBSan ativado, o que revelou e corrigiu dezenas de bugs latentes que o build MSVC de 2003 tolerava em silêncio — incluindo uma falha de chamada virtual pura de 22 anos no desmonte do tanque e da torre.

- Valida cada alvo suportado na CI, executando as suítes de teste de plataforma nativamente em um runner próprio — Linux x86_64 e arm64, Windows MSVC e MinGW, macOS Intel e Apple Silicon — em vez de apenas compilar de forma cruzada.

- Ainda compila sem problemas a partir da solução original `A7.sln` com as ferramentas MSVC modernas (`Debug | Win32`), como referência de comportamento.

- Roda por padrão em uma janela de verdade; `-fullscreen` continua disponível como opção de linha de comando.

- Usa submódulos do Git para as bibliotecas que faltam.

- Suporta depuração nativa de C++ e com WinDbg no VS Code Insiders, além de scripts de captura de falhas sem interface em `tools/zig/`.

- Oferece um fluxo auxiliar para empacotar vídeos comprimidos em `VideoAssets/`.

- Removeu o antigo tratamento de falhas BugSlay e o substituiu por asserts padrão do C++.


# Conversão e validação de recursos do Blender

O pipeline que substitui o Maya40 está documentado em `docs/blender-replacement.md`.

Comandos rápidos:

- Converter: `bk-map obj2mod <input.obj> <output.mod> [skeleton.txt] [animation.txt]`

- Somente validar: `bk-map validate-obj2mod <input.obj> [skeleton.txt] [animation.txt]`


# Plataformas suportadas

O `zig build` aceita seis alvos. Passe um com `-Dtarget=`; a árvore gerada é organizada como `zig-out/game/<os>/<arch>/<config>`, de modo que vários alvos e os dois níveis de otimização ficam lado a lado em vez de se sobrescreverem.

| Plataforma | Triplo de destino | Situação |
| --- | --- | --- |
| Windows (MSVC) | `x86_64-windows-msvc` | Jogável |
| macOS (Apple Silicon) | `aarch64-macos` | Jogável |
| macOS (Intel) | `x86_64-macos` | Compila; não testado em jogo |
| Linux (x86_64) | `x86_64-linux-gnu` | Em andamento |
| Linux (arm64, Raspberry Pi) | `aarch64-linux-gnu` | Somente camada de plataforma |
| Windows (MinGW) | `x86_64-windows-gnu` | Somente camada de plataforma |

“Somente camada de plataforma” significa que o alvo compila e passa nas suítes de teste de plataforma, mas o build completo do jogo ainda não foi colocado para funcionar nele.

Só o `x86_64-windows-msvc` precisa de uma instalação do Visual Studio. O Zig fornece a própria toolchain, cabeçalhos e bibliotecas de importação para todos os outros alvos, inclusive o MinGW.

Cada alvo é exercitado pelo workflow `Cross-platform validation` em um runner da própria arquitetura, de modo que as suítes são executadas e não apenas linkadas.


# Executar o jogo com Zig (principal)

1. Clone o repositório com os submódulos ou execute `git submodule update --init --recursive` em um checkout existente.

2. Instale o Zig (0.17 ou superior). Para `x86_64-windows-msvc` você também precisa da toolchain MSVC e de um SDK do Windows (os caminhos são configuráveis com `-Dmsvc-include`/`-Dwindows-sdk-include` e seus equivalentes `lib`, caso difiram dos padrões do `build.zig`). Nenhum outro alvo precisa deles.

3. Execute `zig build install-game -Dtarget=<triple> --release=fast` para jogar, ou omita `--release=fast` para um build de depuração. Sem `-Dtarget`, o build é para o host.

4. Inicie o binário gerado — `zig-out/game/windows/x86_64/release/Game.exe` no Windows, `zig-out/game/macos/arm64/release/Game` no Apple Silicon (`.../debug/...` sem `--release=fast`). A árvore gerada leva o nome da otimização que contém, então as duas nunca se sobrescrevem.

5. `zig build package` cria pacotes zip distribuíveis; `zig build test` executa os testes unitários do Zig e o teste de fumaça da ABI do C++.


# Executar o jogo com o Visual Studio 2026 Insiders

1. Clone o repositório com os submódulos ou execute `git submodule update --init --recursive` em um checkout existente.

2. Instale o Visual Studio 2026 Insiders / Visual Studio 18 com a carga de trabalho “Desenvolvimento para desktop com C++”, a toolchain MSVC do VS 2026 e um SDK do Windows 10 ou Windows 11.

3. Abra `Sources/src/A7.sln`.

4. Selecione `Debug | Win32`.

5. Compile o projeto `Game` ou a solução inteira.

6. Inicie o projeto `Game` com F5 ou execute `Sources/src/Game/Debug/Game.exe` diretamente.

7. O jogo inicia em janela por padrão. Adicione `-fullscreen` aos argumentos de linha de comando do projeto `Game` se quiser o antigo comportamento em tela cheia.

Se um build não conseguir copiar uma DLL para `Sources/src/Game/Debug`, feche qualquer processo `Game.exe` em execução e compile de novo.


# Histórico até agora

- As bibliotecas que faltavam foram encontradas e adicionadas como submódulos do Git.

- Uma VM com Windows XP SP3 e Visual Studio 6 foi montada para estudar o ambiente original.

- O VS 6 era instável e travava com frequência, então o esforço passou para ferramentas modernas.

- O Visual Studio 2010 foi instalado e o antigo projeto `.dwr` convertido em `.sln`.

- A solução foi carregada no VS 2026 Insiders e todas as dependências foram atualizadas.

- Duas semanas de programação com agentes fizeram a solução compilar do zero, sem erros nem avisos.

- A depuração em tempo de execução já passou do menu inicial: o tutorial carrega e é jogável, inclusive selecionar e mover um tanque.

- O desenvolvimento passou para o VS Code Insiders, por ter ferramentas melhores.

- Dois caminhos de depuração foram configurados: C++ nativo e WinDbg.

- O BugSlay foi removido porque atrapalhava a depuração e, em um caso, falhou depois de uma falha.

- O BugSlay foi substituído por simples asserts padrão do C++.

- Sem o BugSlay, o projeto está pronto para uma depuração focada em tempo de execução até uma execução completa.

- O build passou para o Zig: o `build.zig` compila todos os projetos com clang, linka com a toolchain MSVC e monta um layout de jogo executável.

- A StreamIO.dll — a camada de persistência e serviços do motor — foi substituída por uma implementação em Zig com uma ponte de ABI C++, corrigindo pelo caminho o formato do grafo de objetos de salvar/carregar, a leitura e escrita de XML, as opções e regressões de desempenho por quadro.

- O jogo tornou-se jogável de ponta a ponta com o build Zig: campanhas, tutoriais, abordagem, aviação, ciclos de salvar/carregar, vídeo sincronizado e minimapa correto em resoluções modernas.

- O jogo foi portado para 64 bits: foram corrigidos truncamentos de ponteiros no renderizador, chamadas `_control87` da era x87, `#pragma pack(1)` em tipos STL, assembly x86 inline (substituído por intrínsecos SSE portáveis), o canal de ponteiros de unidades do Lua e os IDs de objeto do formato de save.

- O build x64 foi testado em jogo pelos tutoriais, falha por falha, com o UBSan transformando cada comportamento indefinido de 20 anos em arquivo e linha exatos: coordenadas do minimapa, hash de formações, cálculo do ângulo de bombardeio e mais.

- O projeto saiu do Win32 para uma camada de plataforma portável: SDL3 para janelas, entrada e áudio, um renderizador SDL GPU no lugar do caminho DirectX e um pequeno adaptador nativo por sistema operacional.

- O jogo foi colocado para funcionar no macOS, onde é jogável no Apple Silicon, com o Linux logo depois.

- Linux arm64 (classe Raspberry Pi) e a ABI GNU do Windows (MinGW) foram adicionados como alvos, o que exigiu separar “é Windows?” de “é Visual Studio?” em todo o build. Isso revelou que o macOS Intel era compilado sem nenhum dos seus adaptadores Cocoa, que o job do Linux estava vermelho por falta de um runtime C e que os caminhos C++ do Linux estavam presos ao triplo multiarch x86_64.

- Os seis alvos foram colocados sob CI, cada um em um runner da própria arquitetura.


# Roteiro

1. Terminar de estabilizar o build de 64 bits nos tutoriais e campanhas restantes.

2. Colocar o build completo do jogo para funcionar no Linux arm64 e no MinGW — hoje só as camadas de plataforma deles são validadas — e testar em jogo o macOS Intel.

3. Criar um utilitário para converter saves de 32 bits para o formato de 64 bits.

4. Substituir FMOD, Stingray e Bink por alternativas de código aberto.

5. Continuar substituindo os projetos C++ um a um por código Zig, usando o port do StreamIO como modelo.
