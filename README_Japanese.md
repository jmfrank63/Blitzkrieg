[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)



[![Blitzkrieg Trailer](Blitzkrieg.png)](https://www.youtube.com/watch?v=zNxMvTcsJbk)



このリポジトリは Johannes Maria Frank の個人ペットプロジェクトです。私はそれを楽しみと学びのために使います。特にブラウンフィールドアプリケーションでのエージェンティックコーディングを練習しています。元の README は `readme_original` として保存されています。

警告: このプロジェクトはまだ進行中です。



# このリポジトリが現在できること

- Blitzkriegのシングルプレイヤー版の完全なソースコードとゲームデータを含んでいます。

- 最新のMSVCツールを使って、クリーンな `A7.sln` ソリューションから正常にビルドできます。

- ネイティブ例外レポートとモダンなデバッグサポートを備えています。

- 欠落していたライブラリはGitサブモジュールとして追加されています。

- VS Code Insidersで、ネイティブC++とWinDbgの両方のデバッグをサポートします。

- レガシーなBugSlayのクラッシュ処理を削除し、標準的なC++ assertに置き換えました。



# これまでの経緯

- 欠落していたライブラリを見つけ、Gitサブモジュールとして追加しました。

- Windows XP SP3とVisual Studio 6のVMを構築し、元の環境を調査しました。

- VS6は不安定で頻繁にクラッシュしたため、モダンなツールへの移行を決めました。

- Visual Studio 2010をインストールし、古い `.dwr` プロジェクトを `.sln` に変換しました。

- ソリューションをVS 2026 Insidersに読み込み、すべての依存関係を更新しました。

- 2週間のエージェントによるコーディングで、クリーンな状態からエラー・警告なしでソリューションをコンパイルできるようにしました。

- ゲームが完全に動作したわけではありませんが、チュートリアルの読み込みまで到達し、ビデオと初期メニューが既に読み込まれていました。

- より優れたツールを求めて、開発をVS Code Insidersに移しました。

- ネイティブC++とWinDbgの2つのデバッグパスを構成しました。

- BugSlayはデバッグを難しくし、あるケースではクラッシュ後にさらにクラッシュしたため、削除しました。

- BugSlayは単純な標準C++ assertに置き換えました。

- BugSlayを削除したことで、完全起動に向けた本格的なランタイムデバッグを続ける準備が整いました。



# ロードマップ

1. 例外なしでゲームを動かす。

2. コンパイルをZigに移行する。

3. FMOD、Stingray、Binkをオープンソースの代替品に置き換える。

4. C++プロジェクトを1つずつZigコードに置き換え始める。
