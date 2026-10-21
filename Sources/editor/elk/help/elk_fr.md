# Blitzkrieg ELK

ELK, le kit de localisation de Blitzkrieg, sert à traduire les textes du jeu. Il montre chaque texte du jeu avec son original, vous saisissez la traduction, vous indiquez où en est chaque texte, puis vous placez votre traduction dans le jeu sous forme de mod de langue.

## Démarrer ELK

- Lancé seul, ELK ouvre la base de données que vous avez utilisée en dernier.
- ELK path/to/elk.xml ouvre cette base de données.
- ELK --short est la disposition du traducteur : il ouvre l'elk.xml à côté du programme, le met d'abord à jour avec le fichier de mise à jour .upd le plus récent qui s'y trouve, et affiche une seule barre d'outils avec les cinq filtres fixes. Il démarre toujours de la même façon.
- ELK --developer, ou aucune option, est la disposition étendue avec tous les menus, les filtres nommés que vous créez vous-même et la liste des bases de données récentes.
- La langue des menus, des messages et de cette aide d'ELK se choisit sous Outils > Langue. ELK la retient ; Système suit la langue de votre système d'exploitation.

## Ouvrir et enregistrer

Une base de données est un dossier contenant un fichier elk.xml et un dossier par partie du jeu, tel que game_data_base. Ouvrez-en une avec Fichier > Ouvrir (Ctrl+O) ou choisissez-la sous Fichier > Récents.

Vous ne perdez pas votre travail : ELK enregistre le texte que vous modifiiez quand vous passez à un autre texte, avec Fichier > Enregistrer (Ctrl+S), avec Fichier > Fermer, en quittant, et toutes les 30 secondes tant que quelque chose n'est pas enregistré. La ligne d'état en bas indique quand le texte a des modifications non enregistrées.

## L'arbre des textes

La partie gauche est l'arbre de tous les textes du jeu, rangés en dossiers comme le jeu les conserve. Cliquez sur un texte pour l'afficher : son original en haut, sa description (une note pour les traducteurs, si le texte en a une) en dessous, et sa traduction plus bas. L'original et la description sont en lecture seule, mais vous pouvez y sélectionner et copier. Ouvrez un dossier avec sa flèche ou en double-cliquant dessus.

Chaque ligne a la couleur de son état, et un dossier a la couleur du texte le moins avancé qu'il contient : un dossier rouge contient donc encore du travail.

## États

Chaque texte a l'un de quatre états, choisi avec les boutons entre l'original et la traduction :

- Non traduit (rouge) : rien n'est fait.
- Périmé (jaune) : l'original a changé dans une mise à jour après sa traduction. Vérifiez la traduction et réappliquez-la. Seule une mise à jour ou une importation de tableur définit cet état.
- Traduit (vert) : traduit, pas encore vérifié. Saisir du texte dans une traduction définit cet état.
- Approuvé (bleu) : traduit et vérifié.

## Filtres et navigation

Le filtre choisit les textes que les commandes Parcourir parcourent. L'arbre atténue les textes que le filtre écarte, mais les affiche toujours.

- Les cinq filtres fixes sont Tous, Non traduits, Périmés, Traduits et Approuvés.
- Parcourir > Suivant (Ctrl+N) et Précédent (Ctrl+P) vont au texte suivant ou précédent que le filtre retient, et le curseur se place directement dans la traduction. Dans la disposition étendue, Premier et Dernier vont aux extrémités.
- Réduire l'élément ferme les dossiers que vous quittez en parcourant, de sorte que seul le chemin vers le texte courant reste ouvert.
- Dans la disposition étendue, Parcourir > Créer un filtre... crée vos propres filtres : un nom, les états qu'ils retiennent, Modifiés seulement, et des conditions de dossier comme medals&german (chaque partie doit figurer dans le chemin du dossier).

## Rechercher

Édition > Rechercher... (Ctrl+F, sous Outils dans la disposition courte) cherche dans les originaux, les descriptions et les traductions de tous les textes, dans l'ordre de l'arbre. Suivant va à l'endroit suivant ; Respecter la casse, Mot entier uniquement et Vers le haut ou Vers le bas fonctionnent comme dans les autres programmes. La ligne d'état indique dans quel champ et où le texte a été trouvé.

## Statistiques

Affichage > Statistiques ELK compte les textes, les mots et les caractères des originaux et des traductions, au total et par état, pour voir ce qu'il reste à faire. À côté figurent les nombres tels qu'ils étaient à la dernière fermeture de la base après un comptage, pour voir ce qui a changé depuis.

Affichage > Arbre ELK masque ou affiche la liste des textes, et Affichage > Barre d'état (disposition étendue) la ligne d'état ; ELK retient les deux.

## Vérification orthographique

Outils > Orthographe (F7) vérifie depuis le début la traduction que vous modifiez. Elle s'arrête à chaque mot mal orthographié et place sa meilleure suggestion dans la case Remplacer par ; cliquez sur une autre suggestion ou saisissez la vôtre. Ignorer passe le mot cette fois, Tout ignorer le passe jusqu'à la fermeture d'ELK, Remplacer le remplace par le texte de Remplacer par, et Tout remplacer le remplace partout dans la traduction. Revérifier recommence. Un remplacement est une modification comme du texte saisi et s'enregistre de la même façon. Plus bas, chaque mot mal orthographié est listé avec ses suggestions ; Ajouter au dictionnaire garde un mot, comme un nom, dans votre propre fichier de dictionnaire custom.dic, pour qu'il ne soit plus jamais signalé.

La vérification orthographique a besoin d'un dictionnaire Hunspell, un fichier .dic et un fichier .aff de votre langue, par exemple ceux de LibreOffice. Placez-les dans le dossier dictionaries à côté d'ELK, ou choisissez un autre dossier et la langue sous Outils > Options d'orthographe.... Tant qu'aucun dictionnaire n'est trouvé, l'orthographe est désactivée et en indique la raison.

## Tableurs

Vous pouvez traduire dans un tableur et rapporter le résultat. Dans la disposition courte, ces commandes, l'exportation et Lancer le jeu se trouvent sous Outils et dans la barre d'outils.

- Fichier > Exporter vers un tableur... écrit chaque texte, avec son chemin, son original, sa traduction, son état et sa description, dans un classeur .xlsx (par défaut) ou un fichier .csv UTF-8.
- Fichier > Importer depuis un tableur... lit une telle feuille, ou une ancienne feuille .xls de l'ELK d'origine. Chaque ligne avec une traduction définit la traduction de ce texte. Une ligne dont l'original ne correspond plus à la base marque le texte Périmé. Les lignes sans traduction sont ignorées. La ligne d'état indique combien de lignes ont été lues et modifiées.
- Gardez la colonne Path telle quelle : elle indique à ELK à quel texte appartient une ligne.

## Mettre à jour les originaux

Quand les textes du jeu changent, apportez les nouveaux originaux dans la base ouverte. Un texte dont l'original a changé devient Périmé s'il était traduit ou approuvé, sinon Non traduit ; vos traductions restent.

- Fichier > Importer depuis le jeu... lit les textes du dossier de langue d'un jeu (le dossier qui contient Textes). Les nouveaux textes sont ajoutés ; rien n'est supprimé.
- Fichier > Importer depuis un PAK/UPD... lit un fichier .pak ou .upd. Son nom désigne la partie de la base qu'il met à jour : game_12.upd met à jour game. Un .upd est une mise à jour complète et supprime aussi les textes qu'il n'a plus.
- La ligne d'état indique combien de textes ont été importés. Importer de nouveau le même fichier ne change rien.

## Placer la traduction dans le jeu

- Fichier > Exporter vers un mod de langue... écrit votre traduction comme un mod : saisissez un nom, et ELK écrit mods/<nom>/data/texts.pak dans le dossier du jeu. C'est la méthode sûre : le jeu lit un mod avant ses propres fichiers de texte, et le jeu d'origine reste intact.
- Dans la disposition étendue, quand le filtre courant écarte des textes, la boîte d'exportation propose de n'écrire que les textes que le filtre retient, par exemple seulement les Approuvés. Les textes écartés ne sont pas du tout dans ce mod.
- Dans le jeu, choisissez le mod sous Options > Paramètres > Jeu > Langue. Le jeu change de langue aussitôt et retient le choix. Choisissez-y la langue d'origine pour revenir.
- Fichier > Installer dans le jeu... écrit Data/Texts.pak dans le jeu lui-même, comme le faisait l'ELK d'origine. Si le jeu a des fichiers de texte libres qui le masqueraient, ELK vous avertit et propose le mod à la place.
- Fichier > Lancer le jeu exporte de nouveau le mod avec vos dernières modifications et lance le jeu avec lui, dans un profil de test séparé, de sorte que vos propres sauvegardes et paramètres restent intacts.
- ELK cherche le jeu dans le dossier d'où il est lancé.

## Opérations longues

Les exportations, les importations et une recherche qui doit lire tous les textes s'exécutent en arrière-plan. Une fenêtre Travail en cours montre ce que fait ELK et où il en est, et le reste d'ELK attend la fin. Annuler l'arrête sans risque : une exportation annulée n'écrit rien, et une importation annulée garde les lignes déjà lues, chaque fichier étant intact, de sorte que vous pourrez importer le tableur plus tard.

## Touches

- Ctrl+O ouvrir, Ctrl+S enregistrer
- Ctrl+N ou Alt+N texte suivant, Ctrl+P ou Alt+P texte précédent (Alt seulement quand le curseur n'est pas dans la traduction)
- Ctrl+F rechercher
- F7 orthographe
- F1 cette aide
