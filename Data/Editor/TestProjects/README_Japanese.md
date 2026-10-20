[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)

# Blitzkrieg エディター テストプロジェクト

このフォルダーには、リソースエディターごとにスモークテスト用のフィクスチャフォルダーが一つずつあります。

## 使い方

1. `manifest.json` を開き、エディターのフォルダーを選びます。
2. フォルダーにコピーしたプロジェクトファイルがあれば、そのファイルを `editor.exe` で直接開きます。
3. フォルダーが `external-project-reference` の場合は、そのフォルダーの `README.md` に載っている GOG の外部パスを使います。
4. フォルダーが `runtime-reference` の場合、composer のプロジェクトファイルは見つかっていません。エディターの起動、インポートパス、描画の動作をテストするときは、記載されたランタイム XML を最も近いデータ参照として使ってください。

## コピーの方針

ここにコピーするのは、このリポジトリにすでにあるファイルだけです。市販の GOG インストールのファイルはパスで参照するだけで、複製しません。
