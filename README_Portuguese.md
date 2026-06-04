[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)



[![Blitzkrieg Trailer](Blitzkrieg.png)](https://www.youtube.com/watch?v=zNxMvTcsJbk)



Este repositório é um projeto pessoal de Johannes Maria Frank. Eu o uso para diversão e aprendizado, especialmente para praticar codificação agentiva em aplicações brownfield. O README original está preservado em `readme_original`.

Aviso: este projeto ainda está em andamento.



# O que este repositório pode fazer hoje

- Contém o código-fonte completo do Blitzkrieg para um jogador e os dados do jogo.

- Compila limpo a partir de uma solução `A7.sln` nova com ferramentas MSVC modernas.

- Roda em uma janela real por padrão, com `-fullscreen` ainda disponível como opção de linha de comando.

- O tutorial agora funciona no build `Debug | Win32`; unidades podem ser selecionadas e movidas enquanto a depuração de runtime continua.

- Inclui relatórios nativos de exceção e suporte de depuração moderno.

- Usa submódulos Git para as bibliotecas ausentes.

- Suporta depuração nativa C++ e WinDbg no VS Code Insiders.

- Removeu o tratamento de falhas legadas do BugSlay e o substituiu por asserts padrão do C++.



# Executando o jogo com Visual Studio 2026 Insiders

1. Clone o repositório com submódulos, ou execute `git submodule update --init --recursive` em um checkout existente.

2. Instale o Visual Studio 2026 Insiders / Visual Studio 18 com o workload Desktop development with C++, a toolchain MSVC do VS 2026 e um SDK do Windows 10 ou Windows 11.

3. Abra `Sources/src/A7.sln`.

4. Selecione `Debug | Win32`.

5. Compile o projeto `Game`, ou compile a solução inteira.

6. Inicie o projeto `Game` com F5, ou execute `Sources/src/Game/Debug/Game.exe` diretamente.

7. O jogo inicia em modo janela por padrão. Adicione `-fullscreen` aos argumentos de comando do projeto Game se quiser o comportamento antigo de tela cheia.

Se uma compilação não conseguir copiar uma DLL para `Sources/src/Game/Debug`, feche qualquer processo `Game.exe` em execução e compile novamente.



# Histórico até agora

- Encontramos as bibliotecas ausentes e as adicionamos como submódulos Git.

- Criamos uma VM Windows XP SP3 com Visual Studio 6 para estudar o ambiente original.

- O VS6 era instável e caía frequentemente, então o trabalho foi movido para ferramentas modernas.

- Instalamos o Visual Studio 2010 e convertimos o projeto `.dwr` antigo em `.sln`.

- Carregamos a solução no VS 2026 Insiders e atualizamos todas as dependências.

- Duas semanas de codificação agentiva fizeram a solução compilar de um estado limpo sem erros e avisos.

- A depuração de runtime agora passou do menu inicial: o tutorial carrega e é jogável, incluindo selecionar e mover um tanque.

- O desenvolvimento mudou para o VS Code Insiders por causa de melhores ferramentas.

- Configuramos duas rotas de depuração: C++ nativo e WinDbg.

- Removemos o BugSlay porque dificultava a depuração e, em um caso, ele travou após uma falha.

- Substituímos o BugSlay por simples asserts padrão do C++.

- Com o BugSlay removido, o projeto agora está pronto para continuar a depuração focada até o jogo rodar por completo.



# Roteiro

1. Continuar removendo as exceções de runtime restantes além do caminho do tutorial que agora funciona.

2. Mover a compilação para Zig.

3. Substituir FMOD, Stingray e Bink por alternativas de código aberto.

4. Começar a substituir projetos C++ um por um com código Zig.
