[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)


[![Blitzkrieg Trailer](Blitzkrieg.png)](https://www.youtube.com/watch?v=zNxMvTcsJbk)

Ce dépôt est un projet personnel de Johannes Maria Frank. Je l'utilise pour le plaisir et pour apprendre, en particulier pour m'exercer au développement avec des agents sur une application existante (brownfield). Le README d'origine est conservé dans `readme_original`.

Attention : ce projet est en cours de développement.


# Où en est le projet (novembre 2026)

Le portage lui-même est en grande partie terminé : le jeu tourne nativement sur Windows, macOS et Linux, il parle dix langues, et tous les éditeurs sont portés. Le projet est actuellement bloqué sur les illustrations, sur les vues tournées des bâtiments et des accessoires. Voir « Le mur : les illustrations » plus bas.


# Ce qui a été fait

**Le jeu**

- Tout le jeu solo et ses données sont ici, compilés avec `zig build` (Zig + clang) en programme natif 64 bits. Les campagnes, les tutoriels et les missions aléatoires sont jouables, avec sauvegarde/chargement, vidéo et son.

- Le moteur a quitté Win32 et DirectX pour une couche portable : SDL3 pour les fenêtres, l'entrée et l'audio, et un moteur de rendu SDL GPU.

- Les bibliothèques fermées ont disparu : FMOD, Bink et Stingray ont été retirés. Le son et les films utilisent désormais Ogg Vorbis et Ogg Theora.

- StreamIO.dll, la couche de persistance du moteur, est remplacée par une implémentation Zig (`Sources/src/StreamIOZig`).

- Le C++ historique se compile avec clang et UBSan, ce qui a fait apparaître et corriger des dizaines de bogues que la compilation de 2003 tolérait en silence.

- Les missions aléatoires d'origine sont de retour, chaque chapitre de campagne débloquant sa mission historique comme son script le prévoit.

- Les profils de joueur peuvent suivre un joueur d'une machine à l'autre grâce à la synchronisation dans le cloud.

- Le multijoueur en réseau local fonctionne. Le code GameSpy a été retiré, et le jeu sur Internet n'est pas encore connecté.

**Dix langues**

- Le jeu, tous les éditeurs, les outils en ligne de commande et leurs guides, ainsi que ce README sont disponibles en anglais, russe, chinois (simplifié), hindi, espagnol, français, allemand, portugais (Brésil), japonais et indonésien.

- Les neuf traductions s'installent comme des mods de langue, avec un nouveau moteur de texte TrueType qui dessine chacune de ces écritures.

- Les traductions sont faites par machine et doivent encore être relues par des locuteurs natifs. Les corrections sont les bienvenues.

**Tous les éditeurs**

- L'éditeur de cartes, l'éditeur de ressources avec tous ses sous-éditeurs, et ELK, le kit de localisation, sont réécrits en applications Zig avec Dear ImGui (`Sources/editor`). Ils dialoguent avec le moteur C++ par une ABI C (`Sources/src/EditorBridge`).

- Les anciens éditeurs MFC réservés à Windows ont été supprimés. Le jeu lit ce que les nouveaux éditeurs écrivent, et des tests le vérifient avec les propres lecteurs du jeu.

- Les anciens outils en ligne de commande sont remplacés par sept outils portables pour créer des mods : `bk-pak`, `bk-table`, `bk-image`, `bk-video`, `bk-font`, `bk-map` et `bk-sprite`. Voir `tools/guide/README.md`.

**L'option Remastered**

- Options > Paramètres > Gameplay > Remastered est désactivée par défaut, et désactivée, elle joue selon les règles d'origine. Activée, elle ajoute des fonctions que le jeu de 2003 n'avait pas, comme la rotation de la caméra en quatre pas, un calcul d'itinéraire le plus rapide, et le passage direct de l'infanterie d'un conteneur à un autre.

- Les rediffusions de mission sont en cours de construction ; leurs fondations sont fusionnées.


# Le mur : les illustrations

Tourner la caméra impose de montrer chaque bâtiment et chaque accessoire sous quatre côtés, mais l'art de 2003 est un ensemble d'images de chaque objet vu d'un seul côté. Les autres côtés n'ont jamais été dessinés.

Jusqu'ici, j'ai essayé deux approches pour les 412 objets :

- Construire chaque image tournée à partir des pixels de 2003 (abandonné le 2026-10-29 : chaque problème résolu en créait un nouveau).

- Rendre de simples modèles 3D peints à partir de ces mêmes images. C'est ce que fait le projet aujourd'hui. Le côté que montre l'art de 2003 a l'air juste, mais chaque partie qu'il n'a jamais montrée doit être devinée. Les détails échouent aussi : une lucarne peinte sur un toit plat ne peut pas être juste vue d'un autre côté.

Aucune des deux n'est assez bonne. La bonne méthode est un vrai modèle 3D de chaque objet, avec des détails comme les lucarnes en vraie géométrie et une texture sur chaque face plane, de sorte que chaque côté découle de la forme.

La question est de savoir d'où viennent ces modèles et ces textures :

- Les modéliser à la main ou avec des agents, d'après les dessins de 2003.

- Les modèles de Blitzkrieg 2, que Nival a aussi publiés. Un rendu de test montre qu'ils sont construits ainsi et restent propres aux quatre rotations, mais beaucoup sont des bâtiments différents de ceux de 2003 portant le même nom, et la licence ne couvre pas clairement leur art.

- Les générateurs d'image vers 3D, que je n'ai pas essayés sur cet art.

Aucune décision n'a été prise. Si vous êtes artiste 3D ou connaissez ce type de travail, j'aimerais avoir de vos nouvelles.


# Conversion et validation des ressources Blender

Le pipeline qui remplace Maya40 est documenté dans `docs/blender-replacement.md`.

Commandes rapides :

- Convertir : `bk-map obj2mod <input.obj> <output.mod> [skeleton.txt] [animation.txt]`

- Valider seulement : `bk-map validate-obj2mod <input.obj> [skeleton.txt] [animation.txt]`


# Plateformes prises en charge

Passez une cible avec `-Dtarget=`. L'arborescence produite est organisée en `zig-out/game/<os>/<arch>/<config>`, de sorte que plusieurs cibles et les deux niveaux d'optimisation coexistent côte à côte.

| Plateforme | Triplet cible | État |
| --- | --- | --- |
| Windows (MSVC) | `x86_64-windows-msvc` | Le jeu et les éditeurs fonctionnent ; testé en CI |
| macOS (Apple Silicon) | `aarch64-macos` | Le jeu et les éditeurs fonctionnent ; testé en CI |
| Linux (x86_64) | `x86_64-linux-gnu` | Le jeu et les éditeurs fonctionnent ; testé en CI avec un moteur de rendu logiciel |
| macOS (Intel) | `x86_64-macos` | Compile ; le runner de CI n'a pas de GPU pour le jeu, donc le jeu est vérifié sur un vrai Mac Intel |
| Linux (arm64, Raspberry Pi) | `aarch64-linux-gnu` | Tests de plateforme seulement ; jeu et éditeurs pas encore testés |
| Windows (MinGW) | `x86_64-windows-gnu` | Tests de plateforme seulement ; le moteur ne s'y compile pas encore |

La CI exécute chaque cible sur un runner de sa propre architecture, de sorte que les suites s'exécutent au lieu d'être simplement liées. Certaines tâches sous Linux et macOS échouent actuellement.

Sous Linux, l'icône de la fenêtre est définie dans le programme, ce que respectent X11 et KDE. GNOME sous Wayland l'ignore et prend l'icône du dock et d'alt-tab dans une entrée de bureau nommée d'après l'identifiant d'application du programme (`org.blitzkrieg.game`, et `org.blitzkrieg.mapeditor`, `org.blitzkrieg.resourceeditor`, `org.blitzkrieg.elk` pour les éditeurs). L'installation fournit un script facultatif qui écrit ces entrées et l'icône dans `~/.local/share` ; rien ne l'exécute à votre place. Depuis le dossier d'installation, lancez `Tools/install_desktop_entry.sh` (depuis une copie du dépôt : `tools/install_desktop_entry.sh [install-dir]`), et `Tools/install_desktop_entry.sh --remove` l'annule.

Seul `x86_64-windows-msvc` nécessite une installation de Visual Studio. Zig fournit sa propre chaîne d'outils, ses en-têtes et ses bibliothèques d'import pour toutes les autres cibles, MinGW compris.


# Compilation et exécution

1. Clonez le dépôt avec ses sous-modules, ou exécutez `git submodule update --init --recursive` dans une copie existante.

2. Installez Zig 0.17. Pour `x86_64-windows-msvc`, il faut aussi la chaîne d'outils MSVC et un SDK Windows (les chemins se configurent avec `-Dmsvc-include`/`-Dwindows-sdk-include` et leurs équivalents `lib` s'ils diffèrent des valeurs par défaut de `build.zig`).

3. Lancez `zig build install-all -Dtarget=<triple> --release=fast` pour le jeu, les neuf mods de langue et tous les éditeurs, ou `zig build install-game` pour le jeu seul. Sans `-Dtarget`, la compilation vise l'hôte ; retirez `--release=fast` pour une compilation de débogage.

4. Démarrez le binaire produit, par exemple `zig-out/game/windows/x86_64/release/Game.exe` sous Windows ou `zig-out/game/macos/arm64/release/Game` sur Apple Silicon (`.../debug/...` sans `--release=fast`). Le jeu s'exécute dans une fenêtre ; `-fullscreen` est disponible en option de ligne de commande.

5. `zig build package` crée des paquets zip distribuables, et `zig build --help` liste chaque étape et chaque test.


# Historique jusqu'ici

- J'ai retrouvé les bibliothèques manquantes et fait compiler la solution de 2003 depuis un état propre avec un Visual Studio moderne, puis j'ai joué le premier tutoriel.

- J'ai passé la compilation à Zig : `build.zig` compile chaque projet avec clang et produit une arborescence de jeu exécutable.

- J'ai remplacé StreamIO.dll par du Zig, et rendu le jeu jouable de bout en bout avec la compilation Zig.

- J'ai porté le jeu en 64 bits, et je l'ai testé en jouant à travers les tutoriels, plantage après plantage, avec UBSan.

- J'ai quitté Win32 pour SDL3 et un moteur de rendu SDL GPU, et mis le jeu en route sur macOS et Linux.

- J'ai porté l'éditeur de cartes, l'éditeur de ressources et ELK, et supprimé les versions MFC. J'ai remplacé les anciens outils en ligne de commande.

- J'ai retiré FMOD, Bink et Stingray.

- J'ai ajouté le moteur de texte et les dix langues dans le jeu, les éditeurs et les outils.

- J'ai ajouté l'option Remastered et la rotation de la caméra, ce qui a mené au mur décrit plus haut.


# Feuille de route

1. Franchir le mur des illustrations : décider d'où viennent les modèles 3D et les textures, et obtenir la permission pour tout ce qui en a besoin.

2. Terminer les rediffusions de mission.

3. Refaire passer toutes les tâches de CI.

4. Mettre en route le jeu et les éditeurs sur Linux arm64 et MinGW, et ajouter Windows arm64.

5. Connecter le multijoueur sur Internet grâce à un remplacement de GameSpy par OpenSpy.

6. Créer un utilitaire pour convertir les sauvegardes 32 bits au format 64 bits.

7. Continuer à remplacer les projets C++ par du code Zig, en prenant le portage de StreamIO comme modèle.
