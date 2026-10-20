[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)


[![Blitzkrieg Trailer](Blitzkrieg.png)](https://www.youtube.com/watch?v=zNxMvTcsJbk)

Ce dépôt est un projet personnel de Johannes Maria Frank. Je l'utilise pour le plaisir et pour apprendre, en particulier pour m'exercer au développement avec des agents sur une application existante (brownfield). Le README d'origine est conservé dans `readme_original`.

Attention : ce projet est en cours de développement.


# Ce que ce dépôt sait faire aujourd'hui

- Contient le code source complet du mode solo de Blitzkrieg et les données du jeu.

- Compile tout le jeu avec `zig build` (Zig + clang) pour six cibles — Windows (MSVC et MinGW), macOS (Intel et Apple Silicon) et Linux (x86_64 et arm64) — et produit une installation exécutable sous `zig-out/game/<os>/<arch>/<config>`. Seule la cible MSVC nécessite Visual Studio ; Zig fournit sa propre chaîne d'outils pour les autres.

- Se compile et s'exécute en Game.exe natif **64 bits** (`zig build install-game -Dtarget=x86_64-windows-msvc`) ; les cibles 32 bits ont été abandonnées. Les missions de campagne et les tutoriels sont jouables en x64, avec sauvegarde/chargement, vidéo et son.

- Est livré avec StreamIO.dll entièrement remplacée par une implémentation Zig (`Sources/src/StreamIOZig`) — toute la couche de persistance du moteur : graphes d'objets binaires pour la sauvegarde et le chargement, arbres de données XML, la fabrique d'objets, variables globales, console et options.

- Compile le C++ historique avec clang et UBSan activé, ce qui a révélé et corrigé des dizaines de bogues latents que la compilation MSVC de 2003 tolérait en silence, dont un plantage d'appel virtuel pur vieux de 22 ans lors de la destruction du char et de sa tourelle.

- Valide chaque cible prise en charge en CI, en exécutant les suites de tests de plateforme nativement sur son propre runner — Linux x86_64 et arm64, Windows MSVC et MinGW, macOS Intel et Apple Silicon — plutôt que de se contenter de compiler en croisé.

- Se compile toujours proprement depuis la solution d'origine `A7.sln` avec les outils MSVC actuels (`Debug | Win32`), comme référence de comportement.

- S'exécute par défaut dans une vraie fenêtre ; `-fullscreen` reste disponible en option de ligne de commande.

- Utilise des sous-modules Git pour les bibliothèques manquantes.

- Prend en charge le débogage C++ natif et WinDbg dans VS Code Insiders, ainsi que des scripts de capture de plantage sans interface dans `tools/zig/`.

- Fournit un flux de travail d'aide pour empaqueter les films compressés dans `VideoAssets/`.

- A supprimé l'ancienne gestion des plantages BugSlay au profit d'asserts C++ standard.


# Conversion et validation des ressources Blender

Le pipeline qui remplace Maya40 est documenté dans `docs/blender-replacement.md`.

Commandes rapides :

- Convertir : `bk-map obj2mod <input.obj> <output.mod> [skeleton.txt] [animation.txt]`

- Valider seulement : `bk-map validate-obj2mod <input.obj> [skeleton.txt] [animation.txt]`


# Plateformes prises en charge

`zig build` accepte six cibles. On en passe une avec `-Dtarget=` ; l'arborescence produite est organisée en `zig-out/game/<os>/<arch>/<config>`, de sorte que plusieurs cibles et les deux niveaux d'optimisation coexistent sans s'écraser.

| Plateforme | Triplet cible | État |
| --- | --- | --- |
| Windows (MSVC) | `x86_64-windows-msvc` | Jouable |
| macOS (Apple Silicon) | `aarch64-macos` | Jouable |
| macOS (Intel) | `x86_64-macos` | Compile ; pas testé en jeu |
| Linux (x86_64) | `x86_64-linux-gnu` | En cours de développement |
| Linux (arm64, Raspberry Pi) | `aarch64-linux-gnu` | Couche plateforme seulement |
| Windows (MinGW) | `x86_64-windows-gnu` | Couche plateforme seulement |

« Couche plateforme seulement » signifie que la cible se compile et réussit les suites de tests de plateforme, mais que la compilation complète du jeu n'y a pas encore été mise en route.

Seul `x86_64-windows-msvc` nécessite une installation de Visual Studio. Zig fournit sa propre chaîne d'outils, ses en-têtes et ses bibliothèques d'import pour toutes les autres cibles, MinGW compris.

Chaque cible est exercée par le workflow `Cross-platform validation` sur un runner de sa propre architecture, de sorte que les suites s'exécutent au lieu d'être simplement liées.


# Lancer le jeu avec Zig (principal)

1. Clonez le dépôt avec ses sous-modules, ou exécutez `git submodule update --init --recursive` dans une copie existante.

2. Installez Zig (0.17 ou plus récent). Pour `x86_64-windows-msvc`, il faut aussi la chaîne d'outils MSVC et un SDK Windows (les chemins se configurent avec `-Dmsvc-include`/`-Dwindows-sdk-include` et leurs équivalents `lib` s'ils diffèrent des valeurs par défaut de `build.zig`). Aucune autre cible n'en a besoin.

3. Lancez `zig build install-game -Dtarget=<triple> --release=fast` pour jouer, ou retirez `--release=fast` pour une compilation de débogage. Sans `-Dtarget`, la compilation vise l'hôte.

4. Démarrez le binaire produit — `zig-out/game/windows/x86_64/release/Game.exe` sous Windows, `zig-out/game/macos/arm64/release/Game` sur Apple Silicon (`.../debug/...` sans `--release=fast`). L'arborescence porte le nom de l'optimisation qu'elle contient, si bien que les deux ne s'écrasent jamais.

5. `zig build package` crée des paquets zip distribuables ; `zig build test` exécute les tests unitaires Zig et le test de fumée de l'ABI C++.


# Lancer le jeu avec Visual Studio 2026 Insiders

1. Clonez le dépôt avec ses sous-modules, ou exécutez `git submodule update --init --recursive` dans une copie existante.

2. Installez Visual Studio 2026 Insiders / Visual Studio 18 avec la charge de travail « Développement Desktop en C++ », la chaîne d'outils MSVC de VS 2026 et un SDK Windows 10 ou Windows 11.

3. Ouvrez `Sources/src/A7.sln`.

4. Sélectionnez `Debug | Win32`.

5. Compilez le projet `Game` ou la solution entière.

6. Lancez le projet `Game` avec F5, ou exécutez directement `Sources/src/Game/Debug/Game.exe`.

7. Le jeu démarre en fenêtre par défaut. Ajoutez `-fullscreen` aux arguments de ligne de commande du projet `Game` pour retrouver l'ancien plein écran.

Si une compilation ne peut pas copier une DLL dans `Sources/src/Game/Debug`, fermez tout processus `Game.exe` en cours et recompilez.


# Historique jusqu'ici

- Les bibliothèques manquantes ont été retrouvées et ajoutées comme sous-modules Git.

- Une machine virtuelle Windows XP SP3 avec Visual Studio 6 a été montée pour étudier l'environnement d'origine.

- VS 6 était instable et plantait souvent, l'effort s'est donc déplacé vers des outils modernes.

- Visual Studio 2010 a été installé et l'ancien projet `.dwr` converti en `.sln`.

- La solution a été chargée dans VS 2026 Insiders et toutes les dépendances mises à jour.

- Deux semaines de programmation avec agents ont permis de compiler la solution à partir de zéro sans erreur ni avertissement.

- Le débogage à l'exécution a dépassé le menu initial : le tutoriel se charge et se joue, y compris la sélection et le déplacement d'un char.

- Le développement est passé à VS Code Insiders pour de meilleurs outils.

- Deux chemins de débogage ont été configurés : C++ natif et WinDbg.

- BugSlay a été retiré parce qu'il compliquait le débogage et, dans un cas, a planté après un plantage.

- BugSlay a été remplacé par de simples asserts C++ standard.

- Sans BugSlay, le projet est prêt pour un débogage à l'exécution ciblé en vue d'un fonctionnement complet.

- La compilation est passée à Zig : `build.zig` compile chaque projet avec clang, lie avec la chaîne d'outils MSVC et produit une arborescence de jeu exécutable.

- StreamIO.dll — la couche de persistance et de services du moteur — a été remplacée par une implémentation Zig avec un pont ABI C++, ce qui a corrigé au passage le format du graphe d'objets de sauvegarde/chargement, la lecture et l'écriture XML, les options et des régressions de performance par image.

- Le jeu est devenu jouable de bout en bout avec la compilation Zig : campagnes, tutoriels, abordage, aviation, allers-retours de sauvegarde, vidéo synchronisée, minicarte correcte aux résolutions modernes.

- Le jeu a été porté en 64 bits : troncatures de pointeurs dans le moteur de rendu, appels `_control87` de l'ère x87, `#pragma pack(1)` sur les types STL, assembleur x86 en ligne (remplacé par des intrinsèques SSE portables), canal de pointeurs d'unités Lua et identifiants d'objets du format de sauvegarde, tout a été corrigé.

- La compilation x64 a été testée en jeu à travers les tutoriels, plantage après plantage, UBSan transformant chaque comportement indéfini vieux de 20 ans en fichier et ligne exacts : coordonnées de la minicarte, hachage des formations, calcul d'angle de bombardement, et plus encore.

- Le projet a quitté Win32 pour une couche de plateforme portable : SDL3 pour les fenêtres, l'entrée et l'audio, un moteur de rendu SDL GPU à la place du chemin DirectX, et un petit adaptateur natif par système d'exploitation.

- Le jeu a été mis en route sur macOS, où il est jouable sur Apple Silicon, Linux suivant.

- Linux arm64 (classe Raspberry Pi) et l'ABI GNU de Windows (MinGW) ont été ajoutés comme cibles, ce qui a imposé de séparer « est-ce Windows ? » de « est-ce Visual Studio ? » dans toute la compilation. On a ainsi découvert que macOS Intel se compilait sans aucun de ses adaptateurs Cocoa, que la tâche Linux était rouge faute de runtime C et que les chemins C++ de Linux étaient figés sur le triplet multiarch x86_64.

- Les six cibles ont été placées sous CI, chacune sur un runner de sa propre architecture.


# Feuille de route

1. Finir de stabiliser la compilation 64 bits sur les tutoriels et campagnes restants.

2. Mettre en route la compilation complète du jeu sur Linux arm64 et MinGW — seules leurs couches de plateforme sont validées aujourd'hui — et tester en jeu macOS Intel.

3. Créer un utilitaire pour convertir les sauvegardes 32 bits au format 64 bits.

4. Remplacer FMOD, Stingray et Bink par des alternatives open source.

5. Continuer à remplacer les projets C++ un par un par du code Zig, en prenant le portage de StreamIO comme modèle.
