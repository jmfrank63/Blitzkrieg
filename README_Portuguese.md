[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)



[![Blitzkrieg Trailer](Blitzkrieg.png)](https://www.youtube.com/watch?v=zNxMvTcsJbk)



Este repositório é um projeto pessoal de Johannes Maria Frank. Eu o uso para diversão e aprendizado, especialmente para praticar codificação agentiva em aplicações brownfield. O README original está preservado em `readme_original`.

Aviso: este projeto ainda está em andamento.



# O que este repositório pode fazer hoje

- Contém o código-fonte completo do Blitzkrieg para um jogador e os dados do jogo.

- Compila limpo a partir de uma solução `A7.sln` nova com ferramentas MSVC modernas.

- Inclui relatórios nativos de exceção e suporte de depuração moderno.

- Usa submódulos Git para as bibliotecas ausentes.

- Suporta depuração nativa C++ e WinDbg no VS Code Insiders.

- Removeu o tratamento de falhas legadas do BugSlay e o substituiu por asserts padrão do C++.



# Histórico até agora

- Encontramos as bibliotecas ausentes e as adicionamos como submódulos Git.

- Criamos uma VM Windows XP SP3 com Visual Studio 6 para estudar o ambiente original.

- O VS6 era instável e caía frequentemente, então o trabalho foi movido para ferramentas modernas.

- Instalamos o Visual Studio 2010 e convertimos o projeto `.dwr` antigo em `.sln`.

- Carregamos a solução no VS 2026 Insiders e atualizamos todas as dependências.

- Duas semanas de codificação agentiva fizeram a solução compilar de um estado limpo sem erros e avisos.

- Isso não significou que o jogo estivesse totalmente rodando, mas chegou a carregar o tutorial uma vez, e o vídeo e o menu inicial já estavam carregando.

- O desenvolvimento mudou para o VS Code Insiders por causa de melhores ferramentas.

- Configuramos duas rotas de depuração: C++ nativo e WinDbg.

- Removemos o BugSlay porque dificultava a depuração e, em um caso, ele travou após uma falha.

- Substituímos o BugSlay por simples asserts padrão do C++.

- Com o BugSlay removido, o projeto agora está pronto para continuar a depuração focada até o jogo rodar por completo.



# Roteiro

1. Fazer o jogo rodar sem exceções.

2. Mover a compilação para Zig.

3. Substituir FMOD, Stingray e Bink por alternativas de código aberto.

4. Começar a substituir projetos C++ um por um com código Zig.
