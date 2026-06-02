[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)



[![Blitzkrieg Trailer](Blitzkrieg.png)](https://www.youtube.com/watch?v=zNxMvTcsJbk)



Ce dépôt est un projet personnel de Johannes Maria Frank. Je l'utilise pour le plaisir et l'apprentissage, notamment pour m'entraîner à la programmation agentive sur des applications brownfield. Le README d'origine est conservé dans `readme_original`.

Avertissement : ce projet est encore en cours de réalisation.



# Ce que ce dépôt peut faire aujourd'hui

- Contient le code source complet de Blitzkrieg en solo et les données du jeu.

- Se compile proprement à partir d'une solution `A7.sln` fraîche avec des outils MSVC modernes.

- Inclut un rapport d'exceptions natif et un support de débogage moderne.

- Utilise des sous-modules Git pour les bibliothèques manquantes.

- Prend en charge le débogage natif C++ et WinDbg dans VS Code Insiders.

- A supprimé le gestionnaire de crash BugSlay hérité et l'a remplacé par des asserts C++ standard.



# Historique jusqu'à présent

- Les bibliothèques manquantes ont été trouvées et ajoutées comme sous-modules Git.

- Une VM Windows XP SP3 avec Visual Studio 6 a été créée pour étudier l'environnement original.

- VS6 était instable et se plantait souvent, donc le travail s'est déplacé vers des outils modernes.

- Visual Studio 2010 a été installé et l'ancien projet `.dwr` a été converti en `.sln`.

- La solution a été chargée dans VS 2026 Insiders et toutes les dépendances ont été mises à jour.

- Deux semaines de codage agentif ont permis à la solution de se compiler à partir d'un état propre sans erreurs ni avertissements.

- Cela ne voulait pas dire que le jeu fonctionnait complètement, mais il a atteint le chargement du tutoriel une fois, et la vidéo ainsi que le menu initial se chargeaient déjà.

- Le développement a été déplacé vers VS Code Insiders pour de meilleurs outils.

- Deux voies de débogage ont été configurées : C++ natif et WinDbg.

- BugSlay a été supprimé car il compliquait le débogage et, dans un cas, il a planté après un plantage.

- BugSlay a été remplacé par de simples asserts C++ standard.

- Avec BugSlay supprimé, le projet est désormais prêt à poursuivre le débogage ciblé vers une exécution complète.



# Feuille de route

1. Faire fonctionner le jeu sans exceptions.

2. Migrer la compilation vers Zig.

3. Remplacer FMOD, Stingray et Bink par des alternatives open source.

4. Commencer à remplacer projet C++ après projet par du code Zig.
