[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)


[![Blitzkrieg Trailer](Blitzkrieg.png)](https://www.youtube.com/watch?v=zNxMvTcsJbk)

Este repositório é um projeto pessoal de Johannes Maria Frank. Uso-o por diversão e para aprender, em especial para praticar programação com agentes em aplicações já existentes (brownfield). O README original está preservado em `readme_original`.

Aviso: este projeto está em andamento.


# Em que pé está o projeto (novembro de 2026)

O port em si está em grande parte pronto: o jogo roda nativamente no Windows, no macOS e no Linux, fala dez idiomas e todos os editores foram portados. No momento o projeto está parado na arte, nas vistas giradas de edifícios e objetos. Veja "O muro: a arte" mais abaixo.


# O que já foi feito

**O jogo**

- Todo o jogo para um jogador e seus dados estão aqui, compilados com `zig build` (Zig + clang) como um programa nativo de 64 bits. Campanhas, tutoriais e missões aleatórias são jogáveis, com salvar/carregar, vídeo e som.

- O motor saiu do Win32 e do DirectX para uma camada portável: SDL3 para janelas, entrada e áudio, e um renderizador SDL GPU.

- As bibliotecas fechadas se foram: FMOD, Bink e Stingray foram removidos. Som e filmes agora usam Ogg Vorbis e Ogg Theora.

- A StreamIO.dll, a camada de persistência do motor, foi substituída por uma implementação em Zig (`Sources/src/StreamIOZig`).

- O C++ legado compila com clang e UBSan, o que revelou e corrigiu dezenas de bugs que o build de 2003 tolerava em silêncio.

- As missões aleatórias originais foram revividas, e cada capítulo da campanha libera a sua missão histórica, como o roteiro dele prevê.

- Os perfis dos jogadores podem acompanhá-los de uma máquina para outra por meio da sincronização na nuvem.

- O multijogador em LAN funciona. O código do GameSpy foi removido, e o jogo pela internet ainda não está conectado.

**Dez idiomas**

- O jogo, todos os editores, as ferramentas de linha de comando e seus guias, e este README estão disponíveis em inglês, russo, chinês (simplificado), híndi, espanhol, francês, alemão, português (Brasil), japonês e indonésio.

- As nove traduções são instaladas como mods de idioma, com um novo motor de texto TrueType que desenha todas essas escritas.

- As traduções são feitas por máquina e ainda precisam ser revisadas por falantes nativos. Correções são bem-vindas.

**Todos os editores**

- O Map Editor, o Resource Editor com todos os seus subeditores e o ELK, o kit de localização, foram reescritos como aplicativos em Zig com Dear ImGui (`Sources/editor`). Eles conversam com o motor em C++ por meio de uma ABI C (`Sources/src/EditorBridge`).

- Os antigos editores MFC, só para Windows, foram apagados. O jogo lê o que os novos editores gravam, e os testes confirmam isso com os leitores do próprio jogo.

- As antigas ferramentas de linha de comando foram substituídas por sete portáveis para criar mods: `bk-pak`, `bk-table`, `bk-image`, `bk-video`, `bk-font`, `bk-map` e `bk-sprite`. Veja `tools/guide/README.md`.

**A opção Remastered**

- Opções > Configurações > Gameplay > Remastered vem desligada por padrão, e desligada joga as regras originais. Ligada, acrescenta recursos que o jogo de 2003 não tinha, como girar a câmera em quatro passos, um buscador de caminhos pela rota mais rápida e mover a infantaria direto de um contêiner para outro.

- Os replays de missão estão em construção; a base deles já foi integrada.


# O muro: a arte

Girar a câmera significa que todo edifício e todo objeto precisam ser mostrados de quatro lados, mas a arte de 2003 é um conjunto de imagens de cada objeto vistas de um só lado. Os outros lados nunca foram desenhados.

Até agora tentei duas abordagens para todos os 412 objetos:

- Montar cada imagem girada a partir de pixels de 2003 (abandonada em 2026-10-29: cada problema resolvido criava um novo).

- Renderizar modelos 3D simples pintados a partir dessas mesmas imagens. É o que o projeto faz hoje. O lado que a arte de 2003 mostra fica certo, mas toda parte que ela nunca mostrou tem de ser adivinhada. Os detalhes também falham: uma janela de mansarda pintada num telhado plano não pode ficar certa vista de outro lado.

Nenhuma das duas é boa o bastante. O jeito correto é um modelo 3D de verdade para cada objeto, com detalhes como as mansardas em geometria real e uma textura em cada face plana, de modo que cada lado decorra da forma.

A questão é de onde vêm esses modelos e essas texturas:

- Modelá-los à mão ou com agentes, nos desenhos de 2003.

- Os modelos do Blitzkrieg 2, que a Nival também publicou. Uma renderização de teste mostra que eles são construídos assim e ficam limpos nas quatro rotações, mas muitos são edifícios diferentes dos de 2003 com o mesmo nome, e a licença não cobre claramente a arte deles.

- Geradores de imagem para 3D, que ainda não testei com esta arte.

Nenhuma decisão foi tomada. Se você é artista 3D ou conhece esse tipo de trabalho, gostaria de ouvir você.


# Conversão e validação de recursos do Blender

O pipeline que substitui o Maya40 está documentado em `docs/blender-replacement.md`.

Comandos rápidos:

- Converter: `bk-map obj2mod <input.obj> <output.mod> [skeleton.txt] [animation.txt]`

- Somente validar: `bk-map validate-obj2mod <input.obj> [skeleton.txt] [animation.txt]`


# Plataformas suportadas

Passe um alvo com `-Dtarget=`. A árvore gerada é organizada como `zig-out/game/<os>/<arch>/<config>`, de modo que vários alvos e os dois níveis de otimização ficam lado a lado.

| Plataforma | Triplo de destino | Situação |
| --- | --- | --- |
| Windows (MSVC) | `x86_64-windows-msvc` | O jogo e os editores rodam; testado na CI |
| macOS (Apple Silicon) | `aarch64-macos` | O jogo e os editores rodam; testado na CI |
| Linux (x86_64) | `x86_64-linux-gnu` | O jogo e os editores rodam; testado na CI com um renderizador por software |
| macOS (Intel) | `x86_64-macos` | Compila; o runner da CI não tem GPU para o jogo, então o jogo é verificado em um Mac Intel de verdade |
| Linux (arm64, Raspberry Pi) | `aarch64-linux-gnu` | Somente testes de plataforma; jogo e editores ainda não testados |
| Windows (MinGW) | `x86_64-windows-gnu` | Somente testes de plataforma; o motor ainda não compila aqui |

A CI executa cada alvo em um runner da própria arquitetura, de modo que as suítes são executadas e não apenas linkadas. Alguns jobs no Linux e no macOS estão falhando no momento.

No Linux o ícone da janela é definido no programa, o que o X11 e o KDE respeitam. O GNOME no Wayland o ignora e pega o ícone do dock e do alt-tab de uma entrada de desktop com o nome do app id do programa (`org.blitzkrieg.game`, e `org.blitzkrieg.mapeditor`, `org.blitzkrieg.resourceeditor`, `org.blitzkrieg.elk` para os editores). A instalação traz um script opcional que grava essas entradas e o ícone em `~/.local/share`; nada o executa por você. Na pasta da instalação execute `Tools/install_desktop_entry.sh` (a partir de um checkout: `tools/install_desktop_entry.sh [install-dir]`), e `Tools/install_desktop_entry.sh --remove` o desfaz.

Só o `x86_64-windows-msvc` precisa de uma instalação do Visual Studio. O Zig fornece a própria toolchain, cabeçalhos e bibliotecas de importação para todos os outros alvos, inclusive o MinGW.


# Compilar e executar

1. Clone o repositório com os submódulos ou execute `git submodule update --init --recursive` em um checkout existente.

2. Instale o Zig 0.17. Para `x86_64-windows-msvc` você também precisa da toolchain MSVC e de um SDK do Windows (os caminhos são configuráveis com `-Dmsvc-include`/`-Dwindows-sdk-include` e seus equivalentes `lib`, caso difiram dos padrões do `build.zig`).

3. Execute `zig build install-all -Dtarget=<triple> --release=fast` para obter o jogo, os nove mods de idioma e todos os editores, ou `zig build install-game` só para o jogo. Omita `-Dtarget` para compilar para o host e retire `--release=fast` para um build de depuração.

4. Inicie o binário gerado, por exemplo `zig-out/game/windows/x86_64/release/Game.exe` no Windows ou `zig-out/game/macos/arm64/release/Game` no Apple Silicon (`.../debug/...` sem `--release=fast`). O jogo roda em uma janela; `-fullscreen` está disponível como opção de linha de comando.

5. `zig build package` cria pacotes zip distribuíveis, e `zig build --help` lista todos os passos e testes.


# Histórico até agora

- Encontrei as bibliotecas que faltavam e fiz a solução de 2003 compilar do zero no Visual Studio moderno, e então joguei o primeiro tutorial.

- Passei o build para o Zig: o `build.zig` compila todos os projetos com clang e monta um layout de jogo executável.

- Substituí a StreamIO.dll por Zig e deixei o jogo jogável de ponta a ponta com o build Zig.

- Portei o jogo para 64 bits e o testei em jogo pelos tutoriais, falha por falha, com o UBSan.

- Saí do Win32 para o SDL3 e um renderizador SDL GPU, e coloquei o jogo para funcionar no macOS e no Linux.

- Portei o Map Editor, o Resource Editor e o ELK, e apaguei as versões MFC. Substituí as antigas ferramentas de linha de comando.

- Removi FMOD, Bink e Stingray.

- Acrescentei o motor de texto e os dez idiomas no jogo, nos editores e nas ferramentas.

- Acrescentei a opção Remastered e o giro da câmera, o que levou ao muro descrito acima.


# Roteiro

1. Superar o muro da arte: decidir de onde vêm os modelos 3D e as texturas e obter permissão para tudo o que a exigir.

2. Terminar os replays de missão.

3. Voltar a fazer todos os jobs da CI passarem.

4. Colocar o jogo e os editores para funcionar no Linux arm64 e no MinGW, e acrescentar o Windows arm64.

5. Conectar o multijogador pela internet por meio de um substituto do OpenSpy para o GameSpy.

6. Criar um utilitário para converter saves de 32 bits para o formato de 64 bits.

7. Continuar substituindo projetos C++ por código Zig, usando o port do StreamIO como modelo.
