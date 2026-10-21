# アーキテクチャ

このドキュメントは、Source SDK 2013 上に実装した Portal ゲームコードの
構成とデータの流れをまとめたものです。実装状況は
[status.md](status.md) を参照してください。

## 1. 全体構成

```text
src/
├── game/
│   ├── shared/portal/     ... クライアント/サーバー共通コード
│   │   ├── portal_shareddefs.h
│   │   ├── portal_internal.h
│   │   ├── portal_util_shared.{h,cpp}
│   │   ├── prop_portal_shared.{h,cpp}
│   │   └── portal_gamemovement.{h,cpp}
│   ├── server/portal/     ... サーバー専用コード
│   │   ├── portal_player.{h,cpp}
│   │   ├── portal_client.cpp
│   │   ├── portal_gamerules.{h,cpp}
│   │   ├── prop_portal.cpp
│   │   ├── weapon_portalgun.{h,cpp}
│   │   ├── PortalSimulator.{h,cpp}
│   │   ├── portal_physics_collisionevent.{h,cpp}
│   │   ├── trigger_portal_cleanser.cpp
│   │   ├── prop_weighted_cube.cpp
│   │   └── prop_floor_button.cpp
│   └── client/portal/     ... クライアント専用コード
│       ├── c_portal_player.{h,cpp}
│       ├── c_prop_portal.{h,cpp}
│       ├── c_weapon_portalgun.{h,cpp}
│       ├── PortalRender.{h,cpp}
│       └── portal_render_targets.{h,cpp}
├── game/server/server_portal.vpc
├── game/client/client_portal.vpc
├── launcher_main/launcher_main_portal.{vpc,rc}
├── buildportal
└── createportalprojects.bat
```

`portal_shareddefs.h` は両 DLL が共有する定数・enum を定義します。
`prop_portal_shared.h` はポータルの共有データとサーバー側 `CProp_Portal` の
宣言を含み、クライアント側 `C_Prop_Portal` は `c_prop_portal.h` で宣言します。

## 2. ポータルのジオメトリ

### 2.1 コーナーと平面

ポータルは 4 つのコーナーで表します。順序は **BL, BR, TR, TL**
(左下、右下、右上、左上)です。

```text
        TL ───────── TR
        │             │
        │   normal    │
        │     ↑       │
        BL ───────── BR
```

`CProp_Portal_Shared::m_vPortalCorners` は `CNetworkArray( Vector, ..., 4 )`
としてネットワーク同期されます。

`UpdatePortalPlaneFromCorners()` は次を計算します。

```text
right  = normalize(corner[1] - corner[0])   // BR - BL
up     = normalize(corner[3] - corner[0])   // TL - BL
normal = normalize(cross(right, up))        // 壁から部屋側へ向く
dist   = dot(normal, corner[0])
```

`m_plane_Origin` は壁面の平面を表し、法線はポータルが開く部屋側を向きます。
平面の `type` / `signbits` はエンジンの平面判定用に設定しています。

### 2.2 リンク変換

2 つのポータル A, B のリンク変換は、各ポータルの直交基底
`(right, up, normal)` を使って次のように構成します。

```text
matBasisA = [ rightA upA normalA originA ]
matBasisB = [ rightB upB normalB originB ]

matFlip = diag(-1, 1, -1, 1)   // 共有 up 軸まわりの 180 度回転

A -> B : matBasisB * matFlip * inverse(matBasisA)
B -> A : matBasisA * matFlip * inverse(matBasisB)
```

`matFlip` により、ポータルに入る方向と出る方向が反転します。
`inverse()` は直交基底専用の `PortalOrthoBasisInverse()` で、
回転部分の転置と `-R^T t` を計算します。

変換行列はサーバーがリンク時に、クライアントがネットワーク更新時に
`RecalculatePortalTransforms()` で再計算します。リンクが無効な場合は
`MatrixThisToLinked()` / `MatrixLinkedToThis()` は単位行列を返します。

### 2.3 有効判定

`IsActivatedAndLinked()` は次をすべて満たす場合のみ true です。

- 自分とリンク先の `m_bActivated` が true
- `m_hLinkedPortal` が有効
- 自分とリンク先のコーナーが非退化 (`HasValidCorners()`)

クライアントではネットワーク更新が届くまでコーナーがゼロのため、
この判定で描画・trace を安全にスキップできます。

## 3. ポータルを貫通する trace

`portal_util_shared.cpp` の `UTIL_Portal_TraceRay()` が入口です。
実装は BSP に穴を開けるものではなく、レイをポータル平面で分割して
リンク先の空間で trace を続行するソフトウェア方式です。

### 3.1 処理の流れ

```text
UTIL_Portal_TraceRay(ray)
  └─ PortalTraceRayInternal()
       1. 通常の enginetrace->TraceRay()
       2. ray 上で最初に交差する有効なポータルを探す
          (UTIL_Portal_FirstAlongRay)
       3. 通常 trace がポータル手前の別オブジェクトで止まっていれば終了
       4. ワールドヒットがポータル開口部の平面上でなければ終了
       5. A -> B 変換でレイの始点・終点を変換
       6. リンク先の壁から少し離した位置から再帰 trace
       7. 戻ってきた fraction を元のレイのパラメータ空間へ写像
```

再帰上限は `PORTAL_TRACE_MAX_RECURSION = 6`、各種 epsilon は
`PORTAL_TRACE_EPSILON = 0.25` です。

主な API:

| API | 用途 |
| --- | --- |
| `UTIL_Portal_TraceRay` | 汎用のポータル対応 ray trace |
| `UTIL_Portal_TraceRay_Bullets` | 弾丸用。入口ポータルを指定 |
| `UTIL_Portal_TraceEntity` | エンティティのスイープ |
| `UTIL_Portal_Trace_Beam` | ビームの制御点変換 |
| `UTIL_Portal_FirstAlongRay` | レイ上の最初のポータル |
| `UTIL_Portal_DistanceThroughPortalSqr` | ポータル経由の距離 |
| `UTIL_IsBoxIntersectingPortal` | 箱とポータル開口部の交差 |
| `UTIL_Portal_GetSurfaceBasis` | 表面法線から right/up を生成 |

### 3.2 既知の近似

- trace はポータル平面を跨ぐたびに 1 回分の epsilon を消費します。
- ポータル越しのヒット位置 (`endpos`) はリンク先空間の座標のまま返します。
  fraction は元のレイの空間に写像されます。
- ポータル開口部の内外判定は 4 コーナーの長方形で行います。
- BSP の可視性 (PVS) はポータル越しに完全には統合されません。
  `CPortal_Player::UpdatePortalViewAreaBits()` がリンク先の原点を PVS に
  追加する簡易対応のみです。

## 4. サーバー側

### 4.1 プレイヤーとゲームルール


- `CPortal_Player` は `CHL2_Player` を継承し、`player` クラスとして登録されます。
- `CPortalGameRules::PlayerSpawn()` がスポーン時に `weapon_portalgun` を付与します。
- `portal_client.cpp` は Portal 用の `ClientPutInServer` / `InstallGameRules`
  などを提供し、HL2 の `hl2_client.cpp` の代わりにリンクされます。
- `logic_playerproxy` の `SuppressCrosshair` 入力は
  `CPortal_Player::SuppressCrosshair()` に接続されます。

### 4.2 プレイヤー移動 (Portal)

`CPortalGameMovement` は `CHL2GameMovement` を継承し、`ProcessMovement()` の
冒頭でポータル通過を検出・変換してから、通常の移動処理に引き継ぎます。

処理の流れ:

1. 共有関数 `PickPortalEntry()` で、開口部(origin 基準の近接判定)に含まれ、
   かつ表面に向かって移動しているプレイヤーがいるかを判定する。
   進入後 0.25 秒は同じペアから守る cooldown があるため、出口平面で
   再進入しません。
2. 進入した場合は `MatrixThisToLinked()` で origin/速度/視点角をリンク先
   空間へ変換する。変換位置は出口壁の内側になるため、出口平面へ投影し、
   法線方向へ hull が干渉しない分だけ押し出す。
3. `ApplyPortalTraversal()` で `mv->SetAbsOrigin()` / `mv->m_vecVelocity` /
   視点角を書き戻す。サーバーは `player->pl.v_angle`、クライアントは
   `engine->SetViewAngles()` で視点へ反映する。
4. 以降、ストックの `CHL2GameMovement::ProcessMovement()` が出口ポータルの
   法線方向の速度(最小 50 u/s)で移動を完了させる。

以前の実装 (`TracePlayerBBox` + `PortalReachableMove`) は、移動 trace が
ポータル平面に到達した時点でレイをリンク先空間へ延長するだけで位置
テレポートがなく壁抜けを許すため削除しました。現実装は移動の実行前に
位置そのものをリンク先へ運ぶため、trace を延長しません。

開口部の進入判定は origin ベースの近似であり、hull 全体と開口部の
厳密な交差は見ていません。詳細な既知の制限は
[status.md](status.md) §3.3 を参照してください。

### 4.3 ポータルガン

`weapon_portalgun.cpp`:

- 左クリック: 青 (`PORTAL_TYPE_BLUE`)
- 右クリック: オレンジ (`PORTAL_TYPE_ORANGE`)
- `UTIL_Portal_TraceRay()` で照準先を取得し、ワールド面にポータルを配置
- 同じ色の既存ポータルは `Fizzle()` で削除
- 反対色のポータルが存在すれば、アクティブ/非アクティブに関係なくリンク

設置時は `FindPlacementExtents()` が平面上の 4 方向へ trace し、
開口部の半幅・半高を周囲の壁に合わせて縮めます。
表面の材質による設置可否(白い面のみ等)は判定していません。

### 4.3 ポータルエンティティ

`prop_portal.cpp` の `CProp_Portal`:

| 処理 | 内容 |
| --- | --- |
| `Spawn()` | 非固体・非移動に設定し、`AllPortals` に登録 |
| `SetupPortalCorners()` | コーナーを保存し、平面と変換を再計算 |
| `LinkPortal()` | 相互リンク、両方の `m_bActivated` を true に |
| `UnLinkPortal()` | リンク解除、両方の `m_bActivated` を false に |
| `Fizzle()` | 効果音/エフェクト、リンク解除、`UTIL_Remove()` |
| `OnTakeDamage()` | `DMG_DISSOLVE` で `Fizzle( PORTAL_FIZZLE_KILLED )` |
| `IsTouchingPortal()` | コーナーから作った AABB と開口部の交差判定 |

マップに配置済みのポータルは `m_strLinkedPortalName` でリンク先を
名前解決できます。

### 4.4 物理

`PortalSimulator.cpp` は `PortalPhysFrame()` から呼ばれます。

```text
PortalPhysFrame()
  ├─ CPortalSimulator::PrePhysFrame()
  │    - 動いている vphysics オブジェクトの前フレーム位置を記録
  ├─ PhysFrame()
  ├─ g_Collisions.PortalPostSimulationFrame()
  │    - 保留中の衝突イベントを処理
  ├─ g_Collisions.FrameUpdate()
  └─ CPortalSimulator::PostPhysFrame()
       - 前フレーム位置から現在位置への線分がポータルを跨いだか判定
       - 跨いだ場合、位置・速度・角速度をリンク先空間へ変換して Teleport
```

ポータルを跨いだプロップは「テレポート」方式で移動します。
物理エンジン内にポータル越しの連続した空間を構築する方式ではありません。

`portal_physics_collisionevent.cpp` の `CPortal_CollisionEvent` は
`CCollisionEvent` を継承し、`PortalPostSimulationFrame()` で
`PostSimulationFrame()` を呼び出します。

### 4.5 その他のエンティティ

| クラス名 | 実装 |
| --- | --- |
| `prop_weighted_cube` | `CPhysicsProp` ベース。モデル未指定時に既定モデルを設定 |
| `prop_floor_button` | 球内のプレイヤー/キューブを検出し `OnPowered` / `OnUnpowered` を発火 |
| `trigger_portal_cleanser` | 範囲内のポータルを `Fizzle()`、重み付きキューブを削除 |

## 5. クライアント側

### 5.1 ネットワーク同期

サーバー (`prop_portal.cpp`):

```cpp
SendPropBool( SENDINFO( m_bActivated ) ),
SendPropInt( SENDINFO( m_iPortalType ), 3, SPROP_UNSIGNED ),
SendPropEHandle( SENDINFO( m_hLinkedPortal ) ),
SendPropArray3( SENDINFO_ARRAY3( m_vPortalCorners ),
    SendPropVector( SENDINFO_ARRAY( m_vPortalCorners ), -1, SPROP_COORD ) ),
```

クライアント (`c_prop_portal.cpp`):

```cpp
RecvPropBool( RECVINFO( m_bActivated ) ),
RecvPropInt( RECVINFO( m_iPortalType ) ),
RecvPropEHandle( RECVINFO( m_hLinkedPortal ) ),
RecvPropArray3( RECVINFO_ARRAY( m_vPortalCorners ),
    RecvPropVector( RECVINFO( m_vPortalCorners[0] ) ) ),
```

`C_Prop_Portal` はコンストラクタで `CProp_Portal_Shared::AllPortals` に
自身を登録し、`OnDataChanged()` で以下を実行します。

1. `UpdatePortalPlaneFromCorners()` で平面を再構築
2. `RecalculatePortalTransforms()` でリンク変換を再計算

リンク先のデータがまだ届いていない場合、`HasValidCorners()` が false に
なるため変換は単位行列のままになり、リンク先の更新時に再計算されます。

### 5.2 描画

`PortalRender.cpp` の `CPortalRender` が描画を担当します。
`g_pPortalRender` はファイルスコープの静的インスタンスを指すため、
`viewrender.cpp` などから常に非 NULL で参照できます。

#### RT 方式(既定)

既定 (`portal_rt_mode 1`) では、`DrawPortalsUsingStencils()` は可視ポータルを
収集した後、`DrawPortalViewTargets()` に処理を委譲します。

`DrawPortalViewTargets()`:

1. 各可視ポータルの仮想出口ビューを `BuildPortalView()` で構築。
   出口ビューは「このカメラをポータル変換したもので、fov / アスペクト比は
   メインビューと同一」。RT はメインビューのアスペクトに揃えた解像度
   (幅 256 高さ 256/aspect)で `_rt_Portal1` / `_rt_Portal2` に作成
2. `Push3DView()` で出口シーンを RT に描画 (`ViewDrawScene(VIEW_MONITOR)`)。
   ビューポートは RT の実寸 (`GetActualWidth/Height`) に合わせ、
   frustum は DrawMonitors と同様にビュー毎にローカルに再構築
3. 合成 (`DrawPortalRTThroughQuad()`): 出口ビューはメインカメラの
   正確なテレポートなので、**穴のある画面位置と RT の同じ UV 座標には
   同じレイが映る** (ステンシル方式の画素対応と同一原理)。
   よって開口部 4 頂点の UV は、`ScreenTransform()` による
   このビューでの投影そのものを用いる (出口カメラで再投影しない)

出口シーンが独立レンダーターゲット内に閉じるため、メインシーンとの
深度・ステンシルのフレーム毎の競合が無くなり、カメラ移動でも安定します。
RT をサンプリングした結果が上下反転に見える場合は `portal_rt_vflip`
(既定 1=v 反転して標準規約) を 0 に変えます。

`portal_render_targets_enabled` (既定 0) はモニター用のデバッグ経路
(`DrawPortalsToTextures()`)で、RT 方式の合成とは別です。

#### ステンシル方式(代替)

`portal_rt_mode 0` のときの従来経路:

1. `AllPortals` から視界内の有効なポータルを収集
2. ポータル開口部をステンシルバッファに描画 (`INCR`)
3. リンク先へ変換したカメラで `ViewDrawScene_PortalStencil()` を呼ぶ
4. ステンシル値が一致するピクセルだけにリンク先のsceneを描画
5. 再帰レベルを上げて繰り返す (`MAX_PORTAL_RECURSIVE_VIEWS = 4`)

ポータルビュー用の view id は通常の view id の後ろに割り当てられます
(`PORTAL_VIEW_ID_START`)。

#### 未リンクのポータル

再帰レベル 0 では、リンクされていないポータルも
`DrawDormantPortalSurface()` で半透明の板として描画されます。
現在は `debug/debugvertexcolor` マテリアルを使う簡易表示です。

#### レンダーターゲット

`portal_render_targets.cpp` は水面リフレクション/リフラクション用の
追加レンダーターゲットを再帰レベルごとに管理します。
`portalrendertargets` は静的インスタンスを指すため、NULL にはなりません。

#### 水面・フォグ

`viewrender.cpp` のフックから次が呼ばれます。

- `WaterRenderingHandler_PreReflection/PostReflection`
- `WaterRenderingHandler_PreRefraction/PostRefraction`
- `ShouldForceCheaperWaterLevel`
- `DoesExitPortalViewIntersectWaterPlane`
- `ShiftFogForExitPortalView`
- `GetExitPortalFogOrigin`
- `IsSkyboxVisibleFromExitPortal`

## 6. SDK 側のフック

`PORTAL` 定義時に有効になる主な変更点:

| ファイル | 内容 |
| --- | --- |
| `game/server/physics.cpp` | `PortalPhysFrame()`、`CPortal_CollisionEvent` |
| `game/server/util.cpp` | (ポータル用の削除フックは使用しない) |
| `game/server/basecombatcharacter.cpp` | ポータル越しの視線判定 |
| `game/server/hl2/hl2_player.cpp` | `player` クラス登録の無効化、クロスヘア入力 |
| `game/client/viewrender.cpp` | RT/ステンシル描画フック、水面/フォグ連携 |
| `game/client/view_beams.cpp` | ビームのポータル貫通 |
| `game/client/hud_crosshair.cpp` | クロスヘア抑制 |
| `game/shared/baseentity_shared.cpp` | `FireBullets` のポータル trace |
| `game/client/entity_client_tools.cpp` | ツール連携 |
| `vpc_scripts/projects.vgc` | Portal プロジェクトの登録 |

## 7. ビルドシステム

- `src/buildportal` は Linux 用の入口です。
  `sdk_container` を source し、Sniper コンテナ内で VPC と ninja を実行します。
- `src/createportalprojects.bat` は Windows 用の VPC 入口です。
  `cd /d "%~dp0"` で `src` に移動してから `portal.sln` を生成します。
- `src/vpc_scripts/projects.vgc` に `client_portal.vpc` /
  `server_portal.vpc` / `launcher_main_portal.vpc` を `$PORTAL` 条件で追加しています。
- 出力先は `GAMENAME=portal` により `game/portal/bin/<platform>/` です。

## 8. 設計上の制約

- ポータルは BSP の穴ではなく、trace・描画・テレポートで表現します。
- ポータルは 1 ペア(青/オレンジ)のみを前提としています。
- ポータル表面の専用マテリアル・シェーダーは未使用です(材質判定は
  SURF フラグによる簡易拒否のみ)。
- ポータル越しの NPC ナビゲーション、AI、スクリプト API は未実装です。
- ポータル越しの物理はテレポート方式のため、連続的な衝突応答ではありません。
- ポータルの fizzle はサウンドと `PortalFizzle` エフェクトディスパッチのみで、
  クライアント側の専用エフェクト実装はありません。
