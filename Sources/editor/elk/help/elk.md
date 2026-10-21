# Blitzkrieg ELK

ELK, the Blitzkrieg localisation kit, is where you translate the game's texts. It shows every text of the game with its original, you type the translation, mark how far each text is, and then put your translation into the game as a language mod.

## Starting ELK

- Started on its own, ELK opens the database you used last.
- ELK path/to/elk.xml opens that database.
- ELK --short is the translator layout: it opens the elk.xml beside the program, first brings it up to date with the newest .upd update file there, and shows one toolbar with the five fixed filters. It starts the same way every time.
- ELK --developer, or no option at all, is the extended layout with every menu, the named filters you make yourself and the list of recent databases.
- The language of ELK's menus, messages and this help is chosen under Tools > Language. ELK remembers it; System follows the language of your operating system.

## Opening and saving

A database is a folder with an elk.xml file and one folder per part of the game, such as game_data_base. Open one with File > Open (Ctrl+O) or pick it under File > Recent.

You do not lose work: ELK saves the text you were editing when you move to another text, on File > Save (Ctrl+S), on File > Close, on Exit, and every 30 seconds while something is unsaved. The status line at the bottom says when the text has unsaved changes.

## The text tree

The left side is the tree of all the game's texts, in folders as the game keeps them. Click a text to show it: its original at the top, its description (a note for translators, when the text has one) under it, and its translation below. The original and the description are read-only, but you can select and copy from them. Open a folder with its arrow or by double-clicking it.

Each row has the colour of its state, and a folder has the colour of the least finished text in it, so a red folder still holds work.

## States

Every text has one of four states, chosen with the buttons between the original and the translation:

- Not translated (red): nothing done yet.
- Outdated (yellow): the original changed in an update after it was translated. Check the translation and set it again. Only an update or a spreadsheet import sets this state.
- Translated (green): translated, not yet checked. Typing into a translation sets this state.
- Approved (blue): translated and checked.

## Filters and browsing

The filter chooses which texts the Browse commands walk through. The tree dims the texts the filter leaves out but still shows them.

- The five fixed filters are All, Not Translated, Outdated, Translated and Approved.
- Browse > Next (Ctrl+N) and Previous (Ctrl+P) go to the next or previous text the filter keeps, and the cursor goes straight into the translation. In the extended layout First and Last go to the ends.
- Collapse Item closes the folders you leave as you browse, so only the way to the current text stays open.
- In the extended layout, Browse > Create Filter... makes your own filters: a name, the states they keep, Changed only, and folder conditions such as medals&german (every part must be in the folder's path).

## Find

Edit > Find... (Ctrl+F, under Tools in the short layout) searches the originals, descriptions and translations of all texts in the tree's order. Find Next goes to the next place; Match case, Match whole word only and Up or Down work as in other programs. The status line says in which field and where the text was found.

## Statistics

View > ELK Statistic counts the texts, words and characters of the originals and translations, in total and per state, so you can see how much is left. Beside them are the counts as they were when the database was last closed after counting, so you can see how much changed since.

View > ELK Tree hides or shows the list of texts, and View > Status Bar (extended layout) the status line; ELK remembers both.

## Spell check

Tools > Spelling (F7) checks the translation you are editing from its start. It stops at each misspelt word in turn and puts its best suggestion in the Change to box; click another suggestion or type your own. Ignore skips the word this time, Ignore All skips it until you close ELK, Change replaces it with the Change to text, and Change All replaces it everywhere in the translation. Check Again starts over. A change is an edit like a typed one and is saved like one. Below, every misspelt word is listed with its suggestions; Add to dictionary keeps a word, such as a name, in your own dictionary file custom.dic, so it is never flagged again.

Spell check needs a Hunspell dictionary, a .dic and a .aff file of your language, for example from LibreOffice. Put them in the dictionaries folder beside ELK, or choose another folder and the language under Tools > Spelling Options.... Until a dictionary is found, Spelling is off and says why.

## Spreadsheets

You can translate in a spreadsheet program and bring the result back. In the short layout these commands, the export and Run Game are under Tools and on the toolbar.

- File > Export to spreadsheet... writes every text, with its path, original, translation, state and description, to an .xlsx workbook (the default) or a UTF-8 .csv file.
- File > Import from spreadsheet... reads such a sheet, or an old .xls one from the original ELK. Each row with a translation sets that text's translation. A row whose original no longer matches the database marks the text Outdated. Rows without a translation are skipped. The status line says how many rows were read and changed.
- Keep the Path column as it is: it tells ELK which text a row belongs to.

## Updating the originals

When the game's texts change, bring the new originals into the open database. A text whose original changed becomes Outdated if it was translated or approved, otherwise Not translated; your translations stay.

- File > Import from Game... reads the texts of a game's language folder (the folder that holds Textes). New texts are added; nothing is deleted.
- File > Import from PAK/UPD... reads a .pak or .upd file. Its name picks the part of the database it updates: game_12.upd updates game. A .upd is a complete update and also removes the texts it no longer has.
- The status line says how many texts were imported. Importing the same file again changes nothing.

## Putting the translation into the game

- File > Export to language mod... writes your translation as a mod: type a name, and ELK writes mods/<name>/data/texts.pak in the game folder. This is the safe way: the game reads a mod before its own text files, and the original game stays untouched.
- In the extended layout, when the current filter leaves texts out, the export dialog offers to write only the texts the filter keeps, for example only the Approved ones. The texts it leaves out are not in that mod at all.
- In the game, choose the mod under Options > Settings > Gameplay > Language. The game switches language at once and remembers the choice. Choose the original language there to switch back.
- File > Install to game... writes Data/Texts.pak into the game itself, as the original ELK did. If the game has loose text files that would hide it, ELK warns you and offers the mod instead.
- File > Run Game exports the mod again with your latest edits and starts the game with it, in a separate test profile, so your own saves and settings are left alone.
- ELK looks for the game in the folder it runs from.

## Long operations

The exports, the imports and a Find that has to read every text run in the background. A Working window shows what ELK is doing and how far it is, and the rest of ELK waits until it is done. Cancel stops it safely: an export that is cancelled writes nothing, and a cancelled import keeps the rows it had read, every file whole, so you can import the sheet again later.

## Keys

- Ctrl+O open, Ctrl+S save
- Ctrl+N or Alt+N next text, Ctrl+P or Alt+P previous text (Alt only while the cursor is not in the translation)
- Ctrl+F find
- F7 spelling
- F1 this help
