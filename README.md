# Source SDK 2013 [Portal]

Source SDK 2013 上に **Portal** のゲームコードを再実装したプロジェクトです。
Valve 公式の Portal ソースコードは使用せず、公開されている Source SDK 2013 と、
SDK に残されていた `#ifdef PORTAL` のフックを手がかりに実装しています。

> **状態:** 開発途中です。ポータルの配置・リンク・trace・描画・物理の基本部分は
> 実装されていますが、Portal 本編のすべての機能を再現したものではありません。
> 実装済み/未実装の一覧は [docs/status.md](docs/status.md) を参照してください。

ゲームコンテンツ(マップ、マテリアル、モデル、サウンド)は本リポジトリに含まれません。
ストック Portal(Steam AppID 400)のインストール先から `PortalData/` として
用意する必要があります。

## 必要要件

| 用途 | 必要なもの |
| --- | --- |
| ビルド (Linux) | x86_64 Linux、podman、Steam Runtime Sniper イメージ |
| ビルド (Windows) | Windows 10/11 x64、Visual Studio 2022 (C++ デスクトップ開発) |
| 実行 | Steam、**Source SDK 2013 Multiplayer (AppID 243750)**、**Portal (AppID 400)** |

実行時に SDK 2013 Multiplayer が必要なのは、ランチャーがそのインストール先にある
`hl2.sh` / エンジン本体を起動するためです。Portal (400) はゲームコンテンツ
(`PortalData/`)の提供元として必要です。片方だけでは起動できません。

## 1. Portal コンテンツの配置

Steam で Portal をインストールし、Portal のコンテンツディレクトリを
リポジトリ直下の `PortalData/` にコピーまたはシンボリックリンクします。

Linux の例:

```bash
# Steam のライブラリ位置に合わせてパスを変更してください
ln -s "$HOME/.steam/steam/steamapps/common/Portal/portal" PortalData
```

Windows の例 (`PortalData` フォルダを手動作成して中身をコピーする場合):

```bat
xcopy /E /I "C:\Program Files (x86)\Steam\steamapps\common\Portal\portal" "PortalData"
```

`PortalData/` には少なくとも `maps/`, `materials/`, `models/`, `sound/` が
含まれます。`PortalData/` は `.gitignore` の対象で、リポジトリにはコミットされません。

`game/portal/gameinfo.txt` は次の検索パスでコンテンツをマウントします。

```text
Game |gameinfo_path|.
Game |gameinfo_path|../../PortalData
```

## 2. ビルド (Linux x86_64)

`src` ディレクトリで:

```bash
cd src
./buildportal
```

`buildportal` は次の処理を行います。

1. `sdk_container` 経由で Steam Runtime Sniper のコンテナを起動
2. `vpc /portal /linux64 /ninja /define:SOURCESDK +everything` で ninja ファイルを生成
3. `ninja` でビルド

デバッグ構成は `./buildportal debug` です。

生成物:

| 生成物 | 場所 |
| --- | --- |
| client / server DLL | `game/portal/bin/linux64/` |
| ランチャー | `game/portal_linux64` |
| `compile_commands.json` | `src/compile_commands.json` |

> ビルドは x86_64 環境が前提です。aarch64 上では同梱の `devtools/bin/vpc` が
> 実行できないため、x86_64 のコンテナ/マシン上でビルドしてください。

## 3. ビルド (Windows x64)

`src` ディレクトリで `createportalprojects.bat` を実行します
(エクスプローラーからダブルクリックしても構いません)。

```bat
cd src
createportalprojects.bat
```

`src\portal.sln` が生成されるので、Visual Studio で開いて
**Build > Build Solution** を実行します。

生成物:

| 生成物 | 場所 |
| --- | --- |
| client / server DLL | `game\portal\bin\x64\` |
| ランチャー | `game\portal_win64.exe` (32bit 構成では `game\portal.exe`) |

## 4. 起動

Steam を起動し、SDK 2013 Multiplayer (243750) と Portal (400) の両方が
インストールされていることを確認してから、リポジトリの `game` ディレクトリで
ランチャーを実行します。

```bash
cd game
./portal_linux64
```

Windows:

```bat
cd game
portal_win64.exe
```

ランチャーは実行ファイル名から mod 名 (`portal`) を解決し、
`-game <リポジトリ>/game/portal` を付けて SDK 2013 Multiplayer の
エンジンを起動します。引数を明示する場合は次のようにも実行できます。

```bash
game/portal_linux64 -game "$(pwd)/game/portal"
```

ゲーム内コンソールで `map testchmb_a_00` などを実行するとテストマップを
ロードできます。

## 実装状況

主な実装:

- ポータルガンによる青/オレンジのポータル配置と、2 門のリンク
- コーナー座標からポータル平面・変換行列を構築する共有ジオメトリ
- ポータルを貫通するレイトレース(弾丸、発射ビーム、汎用 trace / エンティティ trace)
- vphysics プロップのポータル通過時のテレポート
- プレイヤーのポータル通過(位置・速度・視点角のリンク先空間への変換、
  `CPortalGameMovement`)
- +USE でのプロップ掴み/投擲(ストック `player_pickup` コントローラ)
- 設置面の簡易制限(`SURF_NODRAW` / スカイ / nodraw・noportal 系の拒否)と
  fizzle 効果
- ステンシル方式のポータル越し描画(再帰ビュー、最大 4 段)
- `_rt_Portal1` / `_rt_Portal2` へのオプション レンダーターゲット描画
- `prop_weighted_cube`, `prop_floor_button`, `prop_button`,
  `prop_portal_spawner`, `linked_portal_door`, `trigger_portal_cleanser`
- クライアントへのポータル状態(コーナー、リンク、種類)のネットワーク同期

主な未実装・簡易実装:

- `func_portal_orientation`, `env_portal_pathtrack`, `npc_security_camera` など
  (周辺エンティティは近似実装)
- Portal 本編の HUD、実績、チャレンジモード
- NPC のポータル経由ナビゲーション
- ポータル越しの完全な物理シミュレーション(現在はプロップのテレポート方式)
- 掴み中はポータルガンがホルスターされる(見た目の差異)

詳細は [docs/status.md](docs/status.md)、設計は
[docs/architecture.md](docs/architecture.md) を参照してください。

## ドキュメント

| ファイル | 内容 |
| --- | --- |
| [README.md](README.md) | セットアップ、ビルド、起動 |
| [docs/architecture.md](docs/architecture.md) | コード構成と処理の流れ |
| [docs/status.md](docs/status.md) | 実装状況、既知の制限、確認状況 |
| [README-src.md](README-src.md) | 上流 Source SDK 2013 の README (参照用) |

## ライセンス

元の Source SDK 部分は [SOURCE 1 SDK LICENSE](LICENSE) の下、非商用目的で
ユーザーにライセンスされています。再実装した Portal コードも同ライセンスに
従います。詳細は [LICENSE](LICENSE) を参照してください。
