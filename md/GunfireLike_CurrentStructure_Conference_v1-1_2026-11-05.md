# GunfireLike 現状構造・カンファレンス資料

作成日: 2026-11-05 JST  
版: 1.1（TYPE-DOC-001の合意・優先順位を反映。全コード監査の完了版ではない）  
対象: GunfireLike / C++ / DirectX 11 / DirectXTK / Dear ImGui  
リポジトリー: MizutaniKanata2003/GunfireLike  
調査基準SHA: `55f3c43f0d7d05e9dcde1d8adc4c873484644e68`


## TYPE-DOC-001：2026-11-05追加合意

この節は文書更新時点の合意であり、コード適用済みの記録ではない。
今回の優先作業はA: 型表記短縮とB: 文書現行化。完成図策定を前提条件にせず、機能追加・全体改名・無関係な不具合修正は含めない。

### 確定対応表

| 元の型 | 短縮名 | 用途・区別 |
|---|---|---|
| DirectX::XMFLOAT3 | VECTOR3 | 保存用3成分。今回承認されたUpperCamelCase規則の個別例外 |
| DirectX::XMFLOAT2 | Vector2 | 保存用2成分 |
| DirectX::XMFLOAT4 | Vector4 | 保存用4成分。色を含む |
| DirectX::XMFLOAT4X4 | Matrix4x4 | 保存用4×4行列 |
| DirectX::XMMATRIX | SimdMatrix | 演算用行列。Matrix4x4とは別名・別実体のまま保持 |
| Microsoft::WRL::ComPtr<T> | ComPtr<T> | COM所有型のテンプレート別名。所有動作を変えない |

- ユーザー指定の数学型定義先: Source/Framework/etc/H/MathTypes.h。
- リポジトリーには既存のEtc（大文字E）ディレクトリーがある。今回の指定はetc（小文字e）。大文字小文字の表記は適用前に確定し、混在したフォルダーを無断で作成しない。
- ComPtrの短縮名も承認済み。ただしMathTypes.hへ含めるか別Headerへ分けるかは未確定。数学型だけの導入を妨げず、COM別名の定義場所は別途確定する。
- VECTOR3以外の型エイリアスはUpperCamelCaseを維持する。すべてのエイリアスを大文字に変える承認ではない。
- 独自Vector実装、型変換、演算方式変更、using namespaceによる短縮、HLSLの機械置換を行わない。
- 値・参照・const、引数と戻り値、メモリ配置、所有・寿命を維持する。
- 元の完全修飾名は別名定義、型同一性検証、外部API説明など必要箇所に意図的に残してよい。
- 初回の予定範囲: 数学型Header、Transform.h/.cpp、ルートvcxprojとfiltersの必要な登録。
- 既存名m_Position等は維持する。一括の変数・関数・クラス名変更を含めない。
- ブランチmasterの確認済みSHAは55f3c43f0d7d05e9dcde1d8adc4c873484644e68。過去基準と同一。未pushのローカル差分は未確認。
- 調査・置換・コード提示・ローカル適用・検証・コミットを別の状態として報告する。
- 過去のDebug / Release成功は変更前基準線。今回の変更後の成功へ流用しない。

### 作業単位

1. 定義場所の表記、重複・名前衝突、filtersを確認し、数学型HeaderとTransformの適用案を提示する。同時に規約・文書を更新する。
2. FpsCameraと関連実装へ適用し、対応表・置換済み範囲を更新する。
3. Renderer / Constant BufferはHeader・cpp・呼出を読んでから、型同一性・サイズ・Shader契約を維持して適用する。
4. 承認範囲の残存表記を記録し、文書内の現行規則・サンプル・参照先を最終照合する。
5. ComPtrは定義場所確定後、数学型とは独立した検証単位で扱う。

### 静的チェック追加

- [ ] 元の型と短縮名が確定対応表に一致する。
- [ ] VECTOR3の命名例外を記録し、他の型へ拡大していない。
- [ ] 保存型と演算型を同じ別名へ統合していない。
- [ ] 定義の重複・名前衝突・循環includeを確認している。
- [ ] include / Project登録のパスと大文字小文字が整合している。
- [ ] 宣言・定義・呼出の型、const、参照が維持されている。
- [ ] 置換済み・未置換・意図的残置を区別している。
- [ ] 規約・サンプル・要約・参照版を対応して更新している。
- [ ] 過去SHA・当時のAPI・検証記録を履歴として保持している。


## 根拠の区分

- S（静的確認）: 指定SHAのコード本文で確認した内容。
- U（ユーザー報告）: ユーザーが実機で確認した結果。AIが実行したものではない。
- H（履歴・旧資料）: 過去の記録。現在の実装確認と同一視しない。
- Q（未確認）: 本文未読、応答省略、未試験などで断定できない内容。
- P（提案）: 将来案。ユーザー承認・実装済みとは扱わない。

基準SHAはコード調査の固定版。ユーザーはローカルpush済みと報告しているが、実行ログにコミットSHAは埋め込まれていない。

## 1. 要約

アプリケーションと主要Sceneは動作する基準線が得られた。一方、全体監査は途中である。
main.cppとSceneManagerの責務は確認済み。今回ShopSceneのC-06とデータテーブルを本文で確認した。
GameSceneにはEnemyVisualの二重Draw呼び出しが静的に残っている。正常動作報告とは別に調査する。
完成図はまだ確定していない。旧設計の全機能を無条件に実装する計画にはしない。

## 2. 調査済み範囲と限界

| 範囲 | 状態 |
|---|---|
| Source配下・Shader・ルート設定の一覧 | 取得済み（前工程） |
| main.cpp、SceneManager.h/.cpp、IScene.h | 本文確認済み（前工程） |
| FadeOverlay.h/.cpp | 本文確認済み（前工程） |
| ShopScene.h/.cpp | 本文確認済み（今回） |
| GameScene.cpp | 応答の見えている範囲を確認。DrawDebugUi途中以降が省略され、全文読了ではない |
| GameScene.h、ShopGate本文 | 現版本文の再確認は未完了 |
| Graphics、Input、Audio、Renderer、Canvas、Shader内部 | 本版の横断監査は未完了 |
| Assets・External全内部、プロジェクト設定本文 | 全件調査未完了 |

## 3. Applicationと所有関係（S）

```text
WinMain（Application層の役割）
├─ GraphicsSystem
├─ InputSystem
├─ AudioSystem
├─ WindowContext（Input / Graphicsへの非所有ポインター）
├─ GameTimer
├─ ImGuiManager
└─ SceneManager
   ├─ Systemへの非所有ポインター
   ├─ unique_ptr<IScene> Current
   ├─ unique_ptr<IScene> Next
   ├─ GameProgress
   └─ FadeOverlay
```

独立したApplicationクラスが確認されたわけではなく、WinMainがこの責務を担う。

### 起動順

COM（MTA）→Window Class→Window→Graphics→Input→Audio→WindowContext登録→Timer→ImGui→Window表示→SceneManager。
SceneManagerは既存状態をFinalizeし、System参照登録、GameProgress初期化、TitleScene予約・初期化を行う。

### フレーム順

Win32メッセージ→Timer→Input→Audio→Escape判定→Scene Update→Graphics BeginFrame→ImGui BeginFrame→Scene Draw→ImGui EndFrame→Graphics EndFrame。
WM_SIZEはGraphicsSystem::Resizeへ接続済み。内部の0サイズ・失敗処理は未監査。

### 終了順

SceneManager→ImGui→WindowContext解除→Input→Audio→Graphics→Window破棄→Window Class解除→COM終了。
各Systemのデストラクターと部分初期化失敗時の全経路は別途確認する。

## 4. SceneManagerとIScene（S）

- ISceneの仮想APIはInitialize、bool Init、Update、Draw、Finalize。UninitはISceneの必須APIではない。
- RequestSceneChange<TScene>は要求時にインスタンスを生成してNextへ保持する。
- 次SceneのInitialize / InitはFade Out後に行う。
- 成功したら旧SceneをFinalizeして破棄し、所有権を切り替える。
- 失敗したら次SceneをFinalizeし、旧Sceneを保持してFade Inする。
- Fade In / Outの双方でCurrent SceneのUpdateは止まる。描画と上位System更新は続く。
- 片方向0.35秒。FadeOverlayはImGuiを使う。
- Finalizeは予約Sceneを破棄し、CurrentをFinalizeして破棄し、System参照をnullへ戻す。

### 整理候補（不具合確定とは区別）

- 初回TitleのInit失敗をmainへ成否で返せない。
- 予約を再要求するとNextが上書きされる。要求競合の契約を明文化する。
- 未初期化・終了後のRequestSceneChangeにはSystemポインターの前提がある。
- 次SceneのInitializeによるBGM・Capture等は、旧Scene保持だけでは復元されない。
- Init内とManager側でFinalizeが呼ばれるため二重終了安全性を確認する。
- mainのAudio / ImGui初期化失敗経路でInput.Uninitの明示呼出がなく、内部とデストラクターを確認する。

## 5. GameScene（見えている本文範囲: S）

### 初期化

進捗・StageData・PlayerStats取得→Combat状態→World Object→Gate→Scene状態→Player / Camera。
Initは必須Asset確認、BasicMesh、Skyモデル、Enemyモデル、HUD、Text Rendererの順に初期化する。
失敗時にFinalizeしてfalseを返す。

### 更新

1. SceneChangeRequest消費
2. DebugUiRequest消費
3. F2要求
4. Player HP状態・死亡
5. Low HP警告
6. GameProgressとCombat / Flash Timer
7. Enemy Combat
8. Player Attack
9. Projectile、Animation
10. Enemy撃破・Stage進行
11. Mouse Capture
12. Gate
13. Player / Camera

確認できた更新済み事項:
- UpdateLowHealthWarningはUpdate内で1回。
- UpdateGameProgressAndTimersはprogress.Update(deltaTime)を呼ぶ。
- F2とGateはSceneChangeRequestを設定する。
- Player死亡は直接Warp SE・Capture解除・Shop予約を行い、要求経路とは別。

### 描画と残存事項

Sky→Opaque→Transparent→Enemy HP Bar→HUD状態構築→追加のEnemy Draw→GameHud→Debug状態構築→Debug UI。
DrawOpaqueWorldとDraw本体の双方に、生存時のm_EnemyVisual.Draw呼出がある。
これは二重の呼出が存在するという静的事実であり、画面への影響や最適な削除位置は追加検証する。
showTutorialの設定は存在する。Enemy HP BarにはGetHealthBarPositionを渡している。
Game / ShopのProjectionはRender Width / Height比を使い、0サイズではDrawをreturnする。
DrawDebugUi途中以降とExecuteNormalShot等の末尾は取得応答が省略されているため全体評価しない。

## 6. ShopScene（S）

### 所有・依存

- SceneManager、Input、Graphics、Audioは参照。
- DebugPlayer、FpsCamera、CombatSystem、Challenge / Title ShopGateを保持。
- BasicMesh、HUD、Text Rendererを保持。
- Animation、AimedTarget、PurchaseResult、Message Timerを保持。

### 処理順

Update: Mouse Capture→Animation→Message Timer→Player / Camera→照準→相互作用。
Draw: Sky→Field→Upgrade→Gate→HUD。

### 実装済み

- GetUpgradeDataのstatic const配列が4種の対象・UpgradeType・位置・色を保持。
- Upgrade描画とRaycastが同じ設定を利用する。
- 最短距離でUpgrade / Gateを選ぶ。購入と遷移の副作用はShopSceneへ残る。
- PurchaseResult enumにNONE / SUCCESS / FAILUREを保持するC-06を確認。
- 成功時にSE、成功・失敗とも表示Timerを1.5秒へ設定。終了時NONEにする。
- Hintと購入結果描画はDrawHud内。ShopHudという専用型へ移した状態ではない。
- 提案されていたShopInteractionHit / FindNearestInteractionHitはShopScene.h/.cppにない。
- Headerの相互作用補助関数順とcpp定義順に差がある。
- namespace定数、メンバー、型名、タブ等は最新の確定規約へ未移行。

C-07案は「未反映を確認した過去提案」であり、自動的に次の実装へ採用しない。

## 7. その他の構造（一覧確認・旧資料との区別）

SourceにはFrameworkの2D / 3D / Audio / DirectX / Etc / Imgui / Input / Shaders、Debug、Scene Commonと4Sceneが存在する。
HudCanvas.h/.cppは存在するため「Canvasは一切未導入」とは書かない。全Scene適用状況はQ。
ルートとGunfireLike配下にvcxprojがあり、slnxの参照先はQ。
旧資料が挙げるAsset Cache、Material、SaveData、FixedUpdate、Collider等は現版で有無・完成度を再評価する。

## 検証基準線（2026-11-05）

| 項目 | 結果 | 根拠 |
|---|---|---|
| Debug x64 Rebuild | 成功 | U |
| Release x64 Rebuild・Build | 成功 | U |
| Release実行・ゲーム内動作 | 正常 | U |
| Title・Game・Shop・Result経由 | 正常。厳密な遷移順は記録なし | U |
| Shop購入成功・失敗、SE、Message消去 | 正常 | U |
| Resize・最小化・復帰 | 正常 | U |
| Debugプロセス終了 | 終了コード0 | 提示ログ |
| D3D11 ERROR / CORRUPTION | 提示ログ範囲に表記なし | 提示ログ |
| Live Object Report | 未確認 | U |

継続診断: DXGI Warning #294、directxdatabasehelper.dllの80070002。
後者の不足ファイルや原因は未特定。終了コード0や正常動作だけでResource Leakなしとは判定しない。
ビルド警告数、性能計測、初期化失敗注入、長時間試験は未記録。

## 8. カンファレンス資料（現状レビュー用。会議実施済みの議事録ではない）

### 確定事項

- このスペースはコード実装・レビュー、別スペースは完成図・実装プロンプト作成を担当する。
- 2026-11-05確定の統合規約v1.2をコード出力前に使用する。
- 現状確認に一括改名・大規模改修を混ぜない。
- 動作済み基準線を保持し、Live Object Reportは未確認として残す。

### 議題

| 議題 | 現状 | 必要な判断 |
|---|---|---|
| 完成範囲 | 旧目標に多数の拡張候補 | 必須 / 後回し / 非対象 |
| 二重Enemy Draw | 呼出箇所2つ | 最小修正の対象・検証 |
| 失敗時復旧 | 旧Scene保持、外部状態復元はQ | 初期化・副作用の契約 |
| Shop分離 | Gate・Table・enum済み | Interactable / HUD / Fieldの必要範囲と順序 |
| 診断 | 動作基準線あり、Live Report未確認 | 確認経路と分類方針 |
| 新規約移行 | 書式・命名差異あり | 別タスクの範囲と参照整合 |

### 初版の受け入れ条件

確認済み・未確認・提案を混同しない。実装確認の追加時に版と根拠を更新する。
未監査部分を含むため「全体監査完了」とは表現しない。

## 参照資料

旧資料（今回提供されたもの）:
- GunfireLike_FrameworkTaskList_v8.md（2026-09-30）
- GunfireLike_FrameworkDesignGoals_v3.md（2026-09-30）
- GunfireLike_CurrentStructure_v3.md（2026-09-30）
- GunfireLike_GameSceneRefactor_StatusAndTasks_v3.md（2026-09-27）

新資料は旧資料の上書きコピーではない。旧IDは出典と併記し、旧不具合の残存はコードで再確認する。
規約は2026-11-05の確定指示を優先する。設計目標は設計スペースで再策定するまで候補として扱う。


## 今回の改訂記録

2026-11-05: 型エイリアス対応表、VECTOR3例外、ユーザー指定パス、優先フェーズを追記。旧基準SHA・実測記録・診断・未確認事項は保持。コード適用・Build・実機検証は未実施。
