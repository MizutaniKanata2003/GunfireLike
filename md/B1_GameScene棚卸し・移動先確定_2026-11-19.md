# GameScene 棚卸し・移動先確定（B1）

作成日: 2026-11-19 JST
工程: B1（`D7以降_細分化タスク一覧_2026-11-18.md` 第4章）
基準コミット: `c9b6ffe9023ecb6fc69b5c99ad7912f5ae9f25d2`（「mdにルール追加、タスク追加」）
目的: `GameScene`に集まった処理と保持状態を責務別に棚卸しし、後続B2〜B9の移動順・移動先・依存を具体化する。**本書は設計案で、コード変更・ファイル移動は行わない。**

## 1. 現状と確認範囲

αブランチで現行の`GameScene.h/.cpp`、`Source/Scene/GameScene/H・CPP`、`Source/Scene/Common/H`、`Source/Debug/H`と、RendererのHeaderを確認した。

- `GameScene`は少なくとも次の責務を同時に持つ：Scene初期化・終了、キー操作、時間（ヒットストップ・スロー）、部屋・ウェーブ、敵行動、プレイヤー戦闘、射撃、視野角、HUD値の組立、3D/UI描画、デバッグ情報の組立。
- GameSceneには、敵種類別の更新・予告、敵弾、近接・接触・回復・射撃、部屋・出口、リソース初期化・描画・デバッグ値作成がある。
- `Source/Debug/`は既に存在し、`DebugPlayer.h/.cpp`がある。
- `Source/Scene/Common/`は既に存在し、`Transform`、`SceneObject`、`ModelRegistry`、`SceneObjectFactory`、`StageLayout`、`UiLayout`などの共通型がある。
- `Framework/3D`には`BasicMeshRenderer`、`DebugLineRenderer`、`ObjModelRenderer`がある。Rendererの共通化（`BasicColorPipeline`）は、B9後のフェーズIで行う。B1〜B9ではRendererの内部を変えない。
- 現行GameScene関連ファイルはタブ字下げのものが残っている。移動工程では、機能変更・全面整形を混ぜず、移動対象と変更呼出元だけを規約適合させる。
- D7（A1〜A8）は、計画書上の完了条件。A8の3部屋通し確認が未報告なら、B2へ進む前にユーザー実測で確定する。

## 2. 責務別棚卸し

| 現行コード・状態 | 現行責務 | 移動先・所有者案 | GameSceneに残すもの |
|---|---|---|---|
| `PlayerController`、`PlayerActionTable` | 入力による移動・動作・視点高さ・構え・蹴り/殴打の開始、壁押戻し | `Source/Scene/Common/H`。プレイヤー操作Controllerが状態を所有 | 入力・Camera・Collisionを渡す更新呼出し、状態参照 |
| `PlayerHealth`、`HealItems` | HP、回復アイテム数・クールダウン | `Source/Scene/Common/H`。独立した値・所持状態として維持 | 初期化と更新呼出し、HUDへ渡す値 |
| `Enemy`、`EnemyTable`、`EnemyManager`、`EnemyWaveTable` | 敵1体・性能表・ウェーブ枠・Collider所有・複数敵検索 | `Source/Scene/Common/H`。EnemyManagerが敵を所有 | 編成表からのウェーブ生成呼出し、更新・描画呼出し、部屋との仲介 |
| `EnemyApproach`、`EnemyRangedBehavior`、`EnemyAreaBehavior` | 敵種類ごとの移動パラメータ・水平移動 | `Source/Scene/Common/H`。B6で`EnemyController`へ責務を統合 | 敵Controllerの更新呼出しだけ |
| `EnemyProjectileManager` | 敵弾の生成・移動・壁消滅・プレイヤー命中結果 | `Source/Scene/Common/H`。`EnemyController`が所有する案 | 敵Controllerの更新・描画呼出し |
| `AreaTelegraph` | 敵枠ごとの範囲予告位置と描画Resource | `Source/Scene/Common/H`。`EnemyController`が所有する案 | 敵Controllerの描画呼出し |
| `RoomController`、`ExitDoor` | 部屋/ウェーブ状態、出口Collider・表示・通過判定 | `Source/Scene/Common/H`。`RunController`またはRoom Controllerが進行状態を所有する案 | 部屋・出口の進行呼出し、Scene遷移予約 |
| `WeaponSystem`、`WeaponTable` | 弾薬・リロード・射撃HitScan・反動要求 | `Source/Scene/Common/H`。Weapon/Shot Controllerが所有 | Weapon/Shot Controllerの更新と結果反映 |
| `CombatResolver`、`CombatTable`、`JustDodgeJudge` | 判定式・戦闘数値・予告タイマー | `Source/Scene/Common/H`。Player/Enemy Controllerから利用 | 検証用F7とScene進行の仲介（後にControllerへ移すかは段階判断） |
| `GameHud`、`UiLayout` | 画像HUDとCanvas上の配置 | UIは今のScene/Common配置を基本に維持。D7整理では位置を変えない | 描画時に状態値をまとめて渡す |
| `GameDebugUi`、`CollisionDebugView` | ImGuiデバッグ表示、Collision/Ray可視化 | B2で`Source/Debug/H・CPP`へ移す | Debug Viewの更新・描画呼出し、データ受渡し |
| `ModelRegistry`、`SceneObjectFactory`、`SceneObject`、`CollisionWorld`、`GraphicsSystem`、`FpsCamera` | Framework/CommonのResource・生成・汎用処理 | 現在のFramework/Common所有関係を維持 | 非所有参照を渡し、生成・描画・問い合わせを仲介 |
| FOV補間・ヒットストップ・スロー | Scene内に混在した時間・視野角 | B8で`TimeScaleController`、`CameraFovController`へ抽出 | Controller更新の呼出しとScene全体の時間配布 |
| `UpdateMelee`、`UpdateContact`、`UpdateHeal`、`ApplyPlayerHitReaction` | プレイヤーの戦闘反応と敵対象検索 | B7で`PlayerCombatController`へ抽出 | プレイヤー戦闘の更新呼出し |

## 3. 移動先・依存の確定案

### B2: Debug移動

- 対象：`GameDebugUi.h/.cpp`、`CollisionDebugView.h/.cpp`と、デバッグ専用の表示データ型。
- 移動先：`Source/Debug/H・CPP/`。
- `DebugPlayer`は既に同ディレクトリにある。`GameDebugUi`と`CollisionDebugView`もここへ統一する。
- `GameScene`はDebugデータの作成とDebug Viewの呼出しだけを続ける。
- ユーザーがVSのSolution Explorer上で物理ファイルを移動し、その後にinclude・Project/Filters参照を直した全体置換ファイルを出力する。

### B3〜B5: 共通型の場所移動

- `PlayerController`、`PlayerActionTable`、`PlayerHealth`、`HealItems` → `Source/Scene/Common/H・CPP/`（B3）。
- `Enemy`、`EnemyManager`、`EnemyTable`、`EnemyWaveTable`、敵行動・弾・予告、`RoomController`、`ExitDoor` → `Source/Scene/Common/H・CPP/`（B4）。
- `WeaponSystem`、`WeaponTable`、`CombatTable`、`CombatResolver`、`JustDodgeJudge` → `Source/Scene/Common/H・CPP/`（B5）。
- 各工程は場所・include・Project/Filters登録のみを変え、挙動や責務を同時に変更しない。
- 既存の型名・公開API名は維持し、移動に必要なincludeだけ更新する。

### B6〜B9: GameSceneの責務分割

| 抽出先 | 移す責務 | 依存（借用して利用するもの） | 所有状態案 |
|---|---|---|---|
| `EnemyController` | ウェーブ生成、敵種類別更新、攻撃予告、敵弾、範囲予告、敵更新・描画 | Playerの位置/当たり/回避状態、ModelRegistry、CollisionWorld、RoomController、Combat数値 | EnemyManager、敵攻撃状態、敵弾、AreaTelegraph |
| `PlayerCombatController` | 蹴り・殴打対象選択、接触反応、Q回復、被弾・ヒットストップの要求 | PlayerController、PlayerHealth、HealItems、EnemyControllerの対象検索、CollisionWorld | 接触クールダウンとスライディング済み敵の記録 |
| `ShotController` | 射撃・リロード更新、命中敵の検索・ダメージ、弾道/HIT表示 | WeaponSystem、EnemyController、CollisionWorld、CollisionDebugView | 武器（移行範囲は実コード確認後に決定） |
| `TimeScaleController` | ヒットストップ・スローの残り時間と、各系統へ渡すDeltaTime | EnemyController、PlayerController、敵弾 | ヒットストップ・スロー時間 |
| `CameraFovController` | 動作状態に応じたFOV目標と補間 | PlayerController、GraphicsSystemの描画サイズ | FOV値 |
| `RoomController`（既存） | 部屋/ウェーブの状態 | EnemyControllerの生成成功・全滅結果、ExitDoorの通過通知 | 既存の部屋/ウェーブ状態を維持 |

分割後の`GameScene`に残す責務:

- Sceneの初期化・終了、Systemの非所有参照、Scene遷移予約。
- F1〜F11のSceneレベルの入力振分け（機能入力の所有先を変える場合は個別工程）。
- 各Controllerへ、合意済みの順に更新・描画を呼び出す。
- HUD用データとDebug用スナップショットを組み立てて渡す。
- Controller間の順序はコメントで明示する。挙動変更を伴う順序変更は行わない。

## 4. B1以降の1工程ずつの進め方

- B1は本書の承認で完了とする。B2はユーザーがファイルをVS上で移動した後、その状態をGitHubに反映し、次の依頼で実施する。
- 各B工程の出力は、原則全体置換ファイル4件以内にする。4件を超える場合は、ファイルグループを分割し、依存を保つ。
- 移動工程は「場所・include・Project/Filtersのみ」、抽出工程は「責務分割のみ」とし、ゲーム機能・数値調整・整形一括適用を混ぜない。
- 各工程の完了条件は、Debug x64 Rebuildと該当機能の動作確認。B9完了時にRelease x64も確認する。
- αブランチの実ファイルと本文を各工程の開始時に再取得する。前回出力したコードを現行HEADとみなさない。
- Project登録変更（`.vcxproj`と`.filters`）が必要な場合、対応する登録内容を同じ工程でまとめて提示する。ユーザーが手作業でコード断片を複数箇所に貼る工程にしない。

## 5. 未確認事項・次の判断

| 項目 | 状態・判断 |
|---|---|
| A8（D7通し） | 計画書ではA8の正常動作確認待ち。B2着手前に、3部屋×2ウェーブのユーザー確認済みを記録する |
| B1の移動先 | 上表は提案。ユーザーが場所と責務分割を承認後、B2へ進む |
| Debug用の実ファイル | αブランチの現行`Source/Debug`には`DebugPlayer`がある。`GameDebugUi`・`CollisionDebugView`の移動前に、B2開始時に全参照・Project登録を再確認する |
| 既存Renderer/Common | `ModelRegistry`と共通RendererはResourceを所有する。B6以降のControllerは、依存の寿命を越えて借用参照を保持しない設計にする |
| タブ/スペース | 現行ファイルにタブ字下げが残る。移動・抽出工程で対象ファイルの全体置換を行うときは規約の2スペースにするが、リポジトリ全体の一括整形は別タスクにする |
| D8以降との時間配分 | ユーザー合意によりフェーズIはMust完了後。BはD8の追加実装前に実施する目標だが、遅延時はD8〜D10を優先し、Bの未完了分はユーザーと延期を合意する |

## 6. B1完了条件

- GameSceneの責務一覧と依存関係を可視化した。
- Debug・Commonへの移動候補と順序を提示した。
- Enemy・Player・Weapon・Time/FOV・Debugの抽出案と所有状態を示した。
- 機能変更と移動・整理を別工程に分離した。
- 次工程B2を、移動対象・配置先・参照更新・登録変更の単位で開始できる。

## 7. 更新履歴

| 日付 | 内容 |
|---|---|
| 2026-11-19 | B1の棚卸し。GameSceneの現行責務、Common/Debug移動案、Controller抽出案、B2〜B9の依存と確認方針を確定用に整理 |
