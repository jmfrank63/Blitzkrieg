[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)


[![Blitzkrieg Trailer](Blitzkrieg.png)](https://www.youtube.com/watch?v=zNxMvTcsJbk)

このリポジトリは Johannes Maria Frank の個人プロジェクトです。楽しみと学習のために使っており、特に既存アプリケーション(ブラウンフィールド)でのエージェント型コーディングの練習が目的です。元の README は `readme_original` に残してあります。

注意: このプロジェクトは開発途中です。


# このリポジトリで現在できること

- Blitzkrieg のシングルプレイヤー用ソースコード一式とゲームデータを含みます。

- `zig build`(Zig + clang)でゲーム全体を 6 つのターゲット — Windows(MSVC と MinGW)、macOS(Intel と Apple Silicon)、Linux(x86_64 と arm64)— 向けにビルドし、`zig-out/game/<os>/<arch>/<config>` に実行可能なインストール構成を作ります。Visual Studio が必要なのは MSVC ターゲットだけで、それ以外は Zig が独自のツールチェーンを提供します。

- ネイティブの **64 ビット** Game.exe としてビルド・実行できます(`zig build install-game -Dtarget=x86_64-windows-msvc`)。32 ビットターゲットは廃止しました。キャンペーンのミッションとチュートリアルは、セーブ/ロード、ビデオ、サウンドを含めて x64 でプレイできます。

- StreamIO.dll を Zig 実装に完全に置き換えて出荷します(`Sources/src/StreamIOZig`)。エンジンの永続化層すべて — セーブ/ロード用のバイナリオブジェクトグラフ、XML データツリー、オブジェクトファクトリ、グローバル変数、コンソール、オプション — が対象です。

- 従来の C++ を UBSan を有効にして clang でコンパイルし、2003 年の MSVC ビルドが黙って見過ごしていた潜在バグを数十件見つけて修正しました。戦車と砲塔の破棄時に起きていた 22 年前の純粋仮想関数呼び出しクラッシュもその一つです。

- サポートする全ターゲットを CI で検証し、クロスコンパイルだけでなく、各アーキテクチャ専用のランナー上でプラットフォームのテストスイートをネイティブに実行します — Linux x86_64 と arm64、Windows MSVC と MinGW、macOS Intel と Apple Silicon。

- 動作の基準として、元の `A7.sln` ソリューションを最新の MSVC ツールで引き続きクリーンにビルドできます(`Debug | Win32`)。

- 既定では実際のウィンドウで動作し、`-fullscreen` はコマンドラインオプションとして引き続き使えます。

- 不足しているライブラリには Git サブモジュールを使います。

- VS Code Insiders でのネイティブ C++ と WinDbg のデバッグ、および `tools/zig/` 以下のヘッドレスなクラッシュ取得スクリプトに対応します。

- `VideoAssets/` 以下に圧縮ムービーアセットをパッケージ化する補助ワークフローを備えます。

- 旧来の BugSlay クラッシュ処理を取り除き、標準の C++ assert に置き換えました。


# Blender アセットの変換と検証

Maya40 の代替パイプラインは `docs/blender-replacement.md` に記載しています。

クイックコマンド:

- 変換: `bk-map obj2mod <input.obj> <output.mod> [skeleton.txt] [animation.txt]`

- 検証のみ: `bk-map validate-obj2mod <input.obj> [skeleton.txt] [animation.txt]`


# 対応プラットフォーム

`zig build` は 6 つのターゲットを受け付けます。`-Dtarget=` で 1 つ指定します。生成されるツリーは `zig-out/game/<os>/<arch>/<config>` の形で配置されるため、複数のターゲットと両方の最適化レベルが上書きされずに並びます。

| プラットフォーム | ターゲットトリプル | 状況 |
| --- | --- | --- |
| Windows (MSVC) | `x86_64-windows-msvc` | プレイ可能 |
| macOS (Apple Silicon) | `aarch64-macos` | プレイ可能 |
| macOS (Intel) | `x86_64-macos` | ビルド可能。プレイ未検証 |
| Linux (x86_64) | `x86_64-linux-gnu` | 開発中 |
| Linux (arm64, Raspberry Pi) | `aarch64-linux-gnu` | プラットフォーム層のみ |
| Windows (MinGW) | `x86_64-windows-gnu` | プラットフォーム層のみ |

「プラットフォーム層のみ」とは、そのターゲットがビルドでき、プラットフォームのテストスイートに合格するものの、ゲーム全体のビルドはまだ立ち上げていないことを意味します。

Visual Studio のインストールが必要なのは `x86_64-windows-msvc` だけです。MinGW を含むそれ以外のターゲットでは、Zig が独自のツールチェーン、ヘッダー、インポートライブラリを提供します。

各ターゲットは、自身のアーキテクチャのランナー上で `Cross-platform validation` ワークフローによって実行されるため、スイートは単にリンクされるのではなく実際に動作します。


# Zig でゲームを実行する(推奨)

1. サブモジュール付きでリポジトリをクローンするか、既存のチェックアウトで `git submodule update --init --recursive` を実行します。

2. Zig(0.17 以降)をインストールします。`x86_64-windows-msvc` には MSVC ツールチェーンと Windows SDK も必要です(`build.zig` の既定値と異なる場合は、`-Dmsvc-include`/`-Dwindows-sdk-include` とそれぞれの `lib` 版でパスを設定できます)。他のターゲットには不要です。

3. プレイするには `zig build install-game -Dtarget=<triple> --release=fast` を実行します。デバッグビルドなら `--release=fast` を外します。`-Dtarget` を省略するとホスト向けにビルドします。

4. 生成されたバイナリを起動します。Windows では `zig-out/game/windows/x86_64/release/Game.exe`、Apple Silicon では `zig-out/game/macos/arm64/release/Game`(`--release=fast` なしなら `.../debug/...`)です。ツリーは含まれる最適化の名前で分かれているため、互いに上書きされません。

5. `zig build package` は配布用の zip パッケージを作成し、`zig build test` は Zig のユニットテストと C++ ABI のスモークテストを実行します。


# Visual Studio 2026 Insiders でゲームを実行する

1. サブモジュール付きでリポジトリをクローンするか、既存のチェックアウトで `git submodule update --init --recursive` を実行します。

2. Visual Studio 2026 Insiders / Visual Studio 18 を、「C++ によるデスクトップ開発」ワークロード、VS 2026 の MSVC ツールチェーン、Windows 10 または Windows 11 SDK とともにインストールします。

3. `Sources/src/A7.sln` を開きます。

4. `Debug | Win32` を選択します。

5. `Game` プロジェクトまたはソリューション全体をビルドします。

6. `Game` プロジェクトを F5 で起動するか、`Sources/src/Game/Debug/Game.exe` を直接実行します。

7. ゲームは既定でウィンドウモードで起動します。従来のフルスクリーン動作にしたい場合は、`Game` プロジェクトのコマンド引数に `-fullscreen` を追加します。

ビルドが `Sources/src/Game/Debug` に DLL をコピーできない場合は、実行中の `Game.exe` プロセスを閉じてからもう一度ビルドしてください。


# これまでの経緯

- 不足していたライブラリを見つけ、Git サブモジュールとして追加しました。

- 元の環境を調べるため、Visual Studio 6 を入れた Windows XP SP3 の VM を構築しました。

- VS 6 は不安定で頻繁にクラッシュしたため、最新のツールへ方針を切り替えました。

- Visual Studio 2010 をインストールし、古い `.dwr` プロジェクトを `.sln` に変換しました。

- ソリューションを VS 2026 Insiders に読み込み、すべての依存関係を更新しました。

- 2 週間のエージェント型コーディングで、クリーンな状態からエラーも警告もなくソリューションがコンパイルできるようにしました。

- 実行時デバッグは最初のメニューを越え、チュートリアルが読み込まれ、戦車の選択と移動を含めてプレイできるところまで進みました。

- より良いツールを求めて、開発を VS Code Insiders に移しました。

- ネイティブ C++ と WinDbg の 2 つのデバッグ経路を設定しました。

- BugSlay はデバッグを難しくし、ある場面ではクラッシュの後にさらにクラッシュしたため、削除しました。

- BugSlay を単純な標準 C++ assert に置き換えました。

- BugSlay を外したことで、プロジェクトは完全動作に向けた実行時デバッグに集中できる状態になりました。

- ビルドを Zig に移行しました。`build.zig` がすべてのプロジェクトを clang でコンパイルし、MSVC ツールチェーンとリンクして、実行可能なゲーム構成を配置します。

- エンジンの永続化・サービス層である StreamIO.dll を、C++ ABI ブリッジ付きの Zig 実装に置き換え、その過程でセーブ/ロードのオブジェクトグラフ形式、XML の読み書き、オプション、フレームごとの性能退行を修正しました。

- Zig ビルドでゲームを最初から最後までプレイできるようにしました。キャンペーン、チュートリアル、乗り込み、航空、セーブ/ロードの往復、映像の同期、現代の解像度での正しいミニマップが含まれます。

- ゲームを 64 ビットに移植しました。レンダラーのポインタ切り捨て、x87 時代の `_control87` 呼び出し、STL 型への `#pragma pack(1)`、x86 インラインアセンブリ(移植可能な SSE イントリンシクスに置き換え)、Lua のユニットポインタ経路、セーブ形式のオブジェクト ID を修正しました。

- x64 ビルドをチュートリアルでクラッシュごとにプレイテストし、UBSan が 20 年来の未定義動作を正確なファイルと行に変えました。ミニマップ座標、編隊のハッシュ、爆撃角の計算などです。

- Win32 から移植可能なプラットフォーム層へ移行しました。ウィンドウ・入力・オーディオには SDL3、DirectX 経路の代わりに SDL GPU レンダラー、各 OS 用の小さなネイティブアダプタを使います。

- ゲームを macOS で立ち上げ、Apple Silicon でプレイ可能になり、Linux が続きました。

- Linux arm64(Raspberry Pi クラス)と Windows GNU ABI(MinGW)をターゲットに加えました。そのためビルド全体で「Windows か?」と「Visual Studio か?」を分ける必要がありました。この作業で、Intel macOS が Cocoa アダプタを一つも含まずにコンパイルされていたこと、C ランタイムがなくて Linux ジョブが赤のままだったこと、Linux の C++ 経路が x86_64 のマルチアーチトリプルに固定されていたことが判明しました。

- 6 つのターゲットすべてを CI の対象にし、それぞれ自身のアーキテクチャのランナーで実行します。


# ロードマップ

1. 残りのチュートリアルとキャンペーンを通して 64 ビットビルドの安定化を終える。

2. Linux arm64 と MinGW でゲーム全体のビルドを立ち上げ(現在検証済みなのはプラットフォーム層のみ)、Intel macOS をプレイテストする。

3. 32 ビットのセーブデータを 64 ビット形式に変換するユーティリティを作る。

4. FMOD、Stingray、Bink をオープンソースの代替に置き換える。

5. StreamIO の移植をひな形にして、C++ プロジェクトを一つずつ Zig コードに置き換え続ける。
