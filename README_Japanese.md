[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)



[![Blitzkrieg Trailer](Blitzkrieg.png)](https://www.youtube.com/watch?v=zNxMvTcsJbk)



このリポジトリは Johannes Maria Frank の個人ペットプロジェクトです。私はそれを楽しみと学びのために使います。特にブラウンフィールドアプリケーションでのエージェンティックコーディングを練習しています。元の README は `readme_original` として保存されています。

警告: このプロジェクトはまだ進行中です。



# このリポジトリが現在できること

- Blitzkriegのシングルプレイヤー版の完全なソースコードとゲームデータを含んでいます。

- 最新のMSVCツールを使って、クリーンな `A7.sln` ソリューションから正常にビルドできます。

- デフォルトで本物のウィンドウモードで起動し、`-fullscreen` もコマンドラインオプションとして引き続き利用できます。

- `Debug | Win32` ビルドではチュートリアルが動作するようになりました。ランタイムデバッグを続けながら、ユニットの選択と移動ができます。

- ネイティブ例外レポートとモダンなデバッグサポートを備えています。

- 欠落していたライブラリはGitサブモジュールとして追加されています。

- VS Code Insidersで、ネイティブC++とWinDbgの両方のデバッグをサポートします。

- レガシーなBugSlayのクラッシュ処理を削除し、標準的なC++ assertに置き換えました。



# Visual Studio 2026 Insidersでゲームを実行する

1. サブモジュール付きでリポジトリをクローンするか、既存のcheckoutで `git submodule update --init --recursive` を実行します。

2. Visual Studio 2026 Insiders / Visual Studio 18 をインストールし、Desktop development with C++ workload、VS 2026 MSVC toolchain、Windows 10 または Windows 11 SDK を含めます。

3. `Sources/src/A7.sln` を開きます。

4. `Debug | Win32` を選択します。

5. `Game` プロジェクト、またはソリューション全体をビルドします。

6. F5で `Game` プロジェクトを起動するか、`Sources/src/Game/Debug/Game.exe` を直接実行します。

7. ゲームはデフォルトでウィンドウモードで起動します。以前のフルスクリーン動作にしたい場合は、Gameプロジェクトのコマンド引数に `-fullscreen` を追加します。

ビルド時にDLLを `Sources/src/Game/Debug` にコピーできない場合は、実行中の `Game.exe` プロセスを閉じてから再ビルドしてください。



# これまでの経緯

- 欠落していたライブラリを見つけ、Gitサブモジュールとして追加しました。

- Windows XP SP3とVisual Studio 6のVMを構築し、元の環境を調査しました。

- VS6は不安定で頻繁にクラッシュしたため、モダンなツールへの移行を決めました。

- Visual Studio 2010をインストールし、古い `.dwr` プロジェクトを `.sln` に変換しました。

- ソリューションをVS 2026 Insidersに読み込み、すべての依存関係を更新しました。

- 2週間のエージェントによるコーディングで、クリーンな状態からエラー・警告なしでソリューションをコンパイルできるようにしました。

- ランタイムデバッグは初期メニューを越えて進み、チュートリアルが読み込まれてプレイ可能になりました。戦車の選択と移動もできます。

- より優れたツールを求めて、開発をVS Code Insidersに移しました。

- ネイティブC++とWinDbgの2つのデバッグパスを構成しました。

- BugSlayはデバッグを難しくし、あるケースではクラッシュ後にさらにクラッシュしたため、削除しました。

- BugSlayは単純な標準C++ assertに置き換えました。

- BugSlayを削除したことで、完全起動に向けた本格的なランタイムデバッグを続ける準備が整いました。



# ロードマップ

1. 現在動作しているチュートリアル経路の先に残っているランタイム例外を引き続き取り除く。

2. コンパイルをZigに移行する。

3. FMOD、Stingray、Binkをオープンソースの代替品に置き換える。

4. C++プロジェクトを1つずつZigコードに置き換え始める。
