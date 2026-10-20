[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)

# Тестовые проекты редактора Blitzkrieg

В этой папке для каждого редактора ресурсов лежит папка с фикстурой для дымового теста.

## Как пользоваться

1. Откройте `manifest.json` и выберите папку редактора.
2. Если в папке есть скопированный файл проекта, откройте его прямо в `editor.exe`.
3. Если папка помечена как `external-project-reference`, возьмите внешний путь GOG из `README.md` этой папки.
4. Если папка помечена как `runtime-reference`, файл проекта composer не найден; используйте указанный XML среды выполнения как ближайший доступный источник данных при проверке запуска редактора, путей импорта и отрисовки.

## Правила копирования

В репозиторий скопированы только файлы, которые в нём уже были. Файлы коммерческой установки GOG указаны лишь путём и не дублируются.
