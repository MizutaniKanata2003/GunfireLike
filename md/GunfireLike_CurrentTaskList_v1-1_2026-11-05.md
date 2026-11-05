# GunfireLike 現行タスクリスト

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

## 1. 管理方法

旧資料には同じC-06が「Shop購入結果enum化」と「Swap Chain移行」の別用途で存在する。
本書では新IDを使用し、旧IDには出典名を付ける。過去のC-07はAI提案であり正式採用前の候補とする。

状態: 完了 / 部分確認 / 未着手 / 要再確認 / 提案保留。
P0: Build不能・Crash・実証されたLeak等。P1: 既存不整合・安全性。P2: 基盤改善。P3: 拡張候補。
優先度は初版の整理案。完成図策定後に更新する。

## 2. 完了・部分確認の記録

| 新ID | 項目 | 状態・根拠 | 旧項目 |
|---|---|---|---|
| BASE-001 | 固定SHAとSource一覧 | 完了・前工程 | 再開調査 |
| BASE-002 | main / SceneManager構造 | 完了・S（関連System内部は別） | 構造確認 |
| BASE-003 | Debug / Release基準線 | 完了・U＋Debugログ | 旧Framework A-05の一部 |
| DOC-001 | 整形・命名・出力前チェック統合 | 完了・会話合意v1.2 | 旧規約の更新 |
| SHOP-BASE-001 | Update / Draw分割 | 確認済み・S | 旧監査C-01 / C-02 |
| SHOP-BASE-002 | CombatSystem Raycast利用 | 呼出確認済み・S、判定内部未監査 | 旧監査C-03 |
| SHOP-BASE-003 | ShopGate利用 | 確認済み・S、Gate本文未監査 | 旧監査C-04 |
| SHOP-BASE-004 | Upgrade Table | 確認済み・S | 旧監査C-05 |
| SHOP-BASE-005 | PurchaseResult enum化 | 確認済み・S、動作U | 旧監査C-06 |
| GAME-BASE-001 | Low HP警告重複呼出 | Update内1回をS確認 | 旧CurrentStructureの懸念 |
| GAME-BASE-002 | progress.Update復帰 | 呼出をS確認 | 旧CurrentStructureの懸念 |
| GAME-BASE-003 | Tutorial状態 | 設定をS確認 | 旧Framework A-02の一部 |
| GRAPHICS-BASE-001 | WM_SIZE接続 | mainでS確認、Resize動作U | 旧Framework C-03の一部 |
| GRAPHICS-BASE-002 | Game / Shop動的Aspect | 使用をS確認、Resize動作U | 旧Framework C-04の一部 |

局所的な呼出確認は、その機能全体の品質保証や旧タスク全条件の完了を意味しない。

## 3. 直近のタスク

今回最優先: TYPE-DOC-001（型表記短縮＋文書現行化）。状態は合意済み・適用前。

| ID | 優先度 | 状態 | 依存 | 内容・完了条件 |
|---|---|---|---|---|
| AUDIT-001 | P1 | 部分確認 | BASE-001 | GameScene.h、cpp省略部分、ShopGate、GameProgressを読み、処理・依存・契約を記録 |
| AUDIT-002 | P1 | 未着手 | BASE-002 | Graphics / Input / Audio / ImGuiの失敗終了を読み、全経路と二重終了の安全性を記録 |
| AUDIT-003 | P2 | 未着手 | BASE-001 | slnxと2つのvcxproj参照、Build対象、Shader出力、Asset配置を特定 |
| GAME-FIX-001 | P1 | 未着手（残存S確認） | 関連Renderer確認 | Enemy Draw二重呼出を調査し、必要なら1経路へ整理。Pass・見た目・Debug / Releaseを検証 |
| SCENE-001 | P1 | 要再確認 | AUDIT-001/002 | 死亡直接遷移・要求競合・初回Init失敗・BGM/Capture復元を整理。変更必要性と最小タスクを判断 |
| QA-001 | P2 | 未確認 | Graphics本文確認 | Live Object Reportが既存実装で可能か調査し、可能なら実測。なければ最小導入案と承認を得る |
| GRAPHICS-001 | P2 | 要再確認 | AUDIT-002/003 | CanvasとProjectionの全Scene適用範囲・0サイズ・Viewportを確認。欠落だけ追加タスク化 |
| STYLE-001 | P2 | 提案保留 | DOC-001、対象合意 | 新規約移行範囲を選定。参照・ファイル名・プロジェクト登録まで整合し、ロジック変更と分離 |
| DESIGN-001 | P1 | 今回保留 | TYPE-DOC-001後 | 別スペースで完成図・必須範囲・非対象・完成条件を決め、ユーザー承認を得る |

今回の優先作業: TYPE-DOC-001のA（型短縮）とB（文書現行化）を同一フェーズで進める。DESIGN-001は後回しとし、監査は今回必要な範囲に限定する。
GAME-FIX-001を含むコード変更は、別途範囲を提示し承認されたタスクとして扱う。

## 4. 旧計画からの設計候補

以下は「未実装確定」でも「採用済み実装順」でもない。完成図の必須範囲を選び、現状監査後に着手する。

| 候補ID | 候補 | 旧出典 | 選定後の完了条件例 |
|---|---|---|---|
| PLAN-SHOP-001 | Interactable / Hit Result | Framework B-04、D-04、会話C-07案 | 入出力・選択規則を定義し、照準・購入・Gateを維持 |
| PLAN-SHOP-002 | ShopHud | Framework B-05 | 値型表示状態で描画し、進捗・Audioへ直接依存しない |
| PLAN-SHOP-003 | ShopField | DesignGoals 11.3 | 配置・描画の分離が実際に責務を減らし、見た目不変 |
| PLAN-RENDER-001 | RenderPass所有者整理 | Framework C-01/C-02 | 全呼出とState契約を列挙し、復帰と順序を統一 |
| PLAN-RENDER-002 | Flip-model | Framework C-06（Shop C-06と別） | Resize / Present / Alt+Tab / 終了とWarning状態を検証 |
| PLAN-RENDER-003 | VSync / InfoQueue | DesignGoals 13、Framework C-05 | 切替・診断・Fallbackの契約を定義し試験 |
| PLAN-UI-001 | 共通Widget / Button | Framework D-02 | Keyboard / Mouse / Capture競合を検証 |
| PLAN-INPUT-001 | Input Action Map | Framework D-03 | Game / UIの優先と既存操作を維持 |
| PLAN-ASSET-001 | OBJ CPU / GPU分離 | Framework E-01 | 不正入力、UV等の仕様と既存モデル描画を確認 |
| PLAN-ASSET-002 | Cache / Material / Sampler | Framework E-02/E-03 | 所有・Key・解放・Bind契約を定義し1対象へ先行導入 |
| PLAN-DATA-001 | Stage / Upgrade外部化 | Framework E-04 | Schema / Validator、範囲・ID・読込失敗を先に定義 |
| PLAN-SAVE-001 | SaveData | Framework E-05 | Version・破損・範囲・失敗経路を定義 |
| PLAN-PHYSICS-001 | FixedUpdate / Collider / 実Projectile | Framework E-06 | HitScanとの棲み分け・二重Damage防止・Pool / Eventを定義 |
| PLAN-QUEUE-001 | RenderCommand / Queue | Framework F-01/F-02 | 1描画対象で導入し、透明順と同距離の安定性を検証 |

旧Framework後半やGameScene旧記録には、ModelAsset / ModelInstance、Scene UI統合、Camera共有、Audioデータ化等もある。
設計スペースで採用・保留・非対象に分類し、項目を落としたまま「完成図確定」としない。

## 5. タスクの共通完了条件

- 目的・範囲・非対象・基準SHA・出力形式・維持仕様が明確。
- 統合規約v1.2の出力前チェックを実施。
- Header / cpp / 呼出 / Project登録が整合。
- 静的確認と実測を区別。Debug / Release結果、回帰、診断を記録。
- 未確認と例外、既知Warning、残課題を記録。
- 完了後に新SHAと資料差分を設計側へ返す。
- 未完了の検証がある場合は条件付き・未確認とし、全完了へ変更しない。

## 6. 次回更新欄

| ID | 変更前 | 変更後 | 根拠・コミット | 確認日 |
|---|---|---|---|---|
| | | | | |

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
