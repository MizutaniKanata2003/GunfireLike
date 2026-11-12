# GunfireLike 完成図の策定依頼

作成日: 2026-11-05 JST  
版: 1.0（現状確認の初版。全コード監査の完了版ではない）  
対象: GunfireLike / C++ / DirectX 11 / DirectXTK / Dear ImGui  
リポジトリー: MizutaniKanata2003/GunfireLike  
調査基準SHA: `55f3c43f0d7d05e9dcde1d8adc4c873484644e68`

## 1. この文書の使い方

設計・プロンプト作成スペースへ、関連資料と一緒に渡す依頼文。
本書は完成仕様そのものではない。旧設計の全機能を必須として実装する指示でもない。
設計スペースは以下の依頼に答え、未決事項を残した初稿からユーザー承認へ進める。

## 2. 依頼本文

GunfireLikeを今後どこまで完成させるか、具体的な完成図を策定してください。
現行の動作を維持し、実装スペースへ一つずつ検証できる依頼を渡せるようにしてください。

### 入力資料

- 現状構造・カンファレンス資料
- 現行タスクリスト
- 統合コーディング規約v1.1
- 別スペースへの引継ぎ資料
- スペース間受け渡し手順・テンプレート
- 旧FrameworkDesignGoals v3、TaskList v8、CurrentStructure v3、GameScene改修記録v3

### 最初に確認する事項

- 最終的に作りたいゲーム・想定利用者・再利用の範囲。
- 完成期限と作業可能な規模。
- 学習目的と機能完成のどちらを優先するか。
- 対象環境・入力機器・解像度等の必要範囲。
- 今回必須にする描画、UI、データ、保存、物理機能。

未回答の事項は仮定としてラベルを付け、確定条件にしない。

## 3. 旧目標から引き継ぐ設計候補

| 領域 | 旧目標 | 設計で決めること |
|---|---|---|
| Application | Window / Loop / System寿命 | WinMain維持かクラス化か。必要性と責務 |
| Scene | 構成・順序・副作用の調停 | ゲーム進捗と汎用SceneManagerの境界、失敗時復旧 |
| Controller / Visual | 判定結果と見た目を分離 | 依存方向、状態の所有者、Request / Result契約 |
| Shop | Field / Interactable / HUD分離 | 分離で何を改善するか、過剰抽象化を避ける範囲 |
| Rendering | Pass / Queue / Command | 状態設定の所有者、透明順、導入対象と移行経路 |
| Asset | CPU Loader / GPU Mesh / Cache | Key、寿命、重複読込、失敗、Unload |
| Material | Shader / Texture / Sampler | Passとの重複、Bind契約、Instance上書き |
| UI | Canvas / Text / Panel / Button | 現Canvasの再利用、Mouse / Keyboardと入力優先 |
| Input / Camera | Action Map、Transform整合 | UIとの競合、Capture、Focus、同期 |
| Data / Save | 外部設定、Validator、Version | フォーマット選定、互換性、不正データ処理 |
| Physics | FixedUpdate / Collider / Projectile | HitScanを維持するか、Damage所有者、二重適用防止 |
| Audio | BGM / SE識別、音量・Path設定 | 初期化失敗、切替、終了、Device変更の範囲 |
| Diagnostics | InfoQueue / Live Report / Flip / VSync | 検証できる完成条件、既知Warningの扱い |
| Model | ModelAsset / ModelInstance | 共有とInstance状態、必要なモデル仕様 |

既存CanvasやControllerを未導入と決めつけない。必要ならコード監査を実装側へ依頼する。

## 4. 必須成果物

1. 完成の定義: できること・できないこと・想定利用例。
2. 必須 / 発展 / 今回非対象の機能表と選定理由。
3. 層構造図: Application / Framework / Scene / Game Rule / Visual等。
4. 依存方向と所有関係図: 所有・非所有・共有・終了順を区別。
5. 起動・失敗・Update・Draw・遷移・終了のフロー。
6. Rendering / Asset / Input / UI / Dataの主要契約。
7. 現状とのGap表: 実装済み・部分的・未確認・要追加を区別。
8. 依存付きロードマップ: 小タスク、優先、非対象、検証、移行方法。
9. 各段階の受入条件: Build、操作、失敗系、診断、最小デモ。
10. 未決事項と意思決定記録。
11. 承認後、最初の実装依頼を一件だけ作成。

## 5. 完成条件の書き方

「拡張しやすい」「安全」とだけ書かず、操作・結果・失敗処理・証拠で表現する。

例（設計候補）:
- 二つのSceneで同一UI部品を利用し、Resize後もクリック座標と描画が一致する。
- 次SceneのAsset読込失敗後も旧Sceneへ戻り、BGM・Captureが定義どおりになる。
- 同じMeshを二つのInstanceで使い、共有Resourceと個別Transformが混ざらない。
- Projectileを実体化するならHitScanとの二重Damageが起きない。

例は確定要件ではない。ユーザーが必要とする範囲へ絞る。

## 6. 制約

- いきなりECS化、全描画Queue化、物理Engine化を要求しない。
- クラス数を増やすことを完成条件にしない。
- 旧SceneやRendererを一度に捨てない。必要ならAdapterで段階移行する。
- 不明APIや未確認の性能値・納期見積りを事実として指定しない。
- 正常動作基準線を維持し、Live Report未確認をLeakなしと書かない。
- 今回は設計と実装プロンプト作成が主目的。完成コードの大量出力を先行しない。

## 7. 返却形式

```text
設計版・日付:
基準コードSHA:
完成像の要約:
必須 / 発展 / 非対象:
層・依存・所有関係:
主要フローと契約:
現状との差分・未確認:
ロードマップ:
段階ごとの受入条件:
ユーザー判断が必要な点:
承認状態:
承認後の最初の実装依頼:
```

## コード出力の共通基準

正式な詳細規約は「GunfireLike 統合コーディング規約・命名規則・コード出力前レビュー」版1.1（2026-11-05）を共有して参照する。
以下は取り違え防止の要約であり、統合規約を置き換えない。

- スペース2個、タブ禁止。インデント・引数整列を含めて180文字以内。
- 長い引数は意味グループで分け、継続行を最初の引数位置へそろえる。
- 同じ目的の処理はまとめ、異なる責務の間は空行1行。
- include順: 対応Header（cppのみ）→Windows→C++標準→外部ライブラリ→Debug→Framework→Scene。
- クラス・メソッド・型エイリアス・ファイル名: UpperCamelCase。
- struct / enum型名: UPPER_SNAKE_CASE。列挙子: e_UPPER_SNAKE_CASE。
- クラス通常メンバー: m_lowerCamelCase。構造体フィールド・通常引数: lowerCamelCase。
- ローカル: snake_case。略語HP / UI / GPU / FPSなどは大文字を維持。
- namespace名: UPPER_SNAKE_CASE（ns_を付けない）。namespace内の定数: ns_UPPER_SNAKE_CASE。
- クラスの定数メンバー: m_UPPER_SNAKE_CASE。
- 型テンプレート仮引数: TValue形式。数値等: ELEMENT_COUNT形式。
- Header先頭、グループ、メソッド、各引数、メンバー、enum / structと各要素へ説明を書く。
- Header宣言順とcpp定義順を対応させ、出力前にチェックシートで確認する。
- 未変更の既存API名や外部APIを勝手に改名しない。一括改名は別タスク。

## 参照資料

旧資料（今回提供されたもの）:
- GunfireLike_FrameworkTaskList_v8.md（2026-09-30）
- GunfireLike_FrameworkDesignGoals_v3.md（2026-09-30）
- GunfireLike_CurrentStructure_v3.md（2026-09-30）
- GunfireLike_GameSceneRefactor_StatusAndTasks_v3.md（2026-09-27）

新資料は旧資料の上書きコピーではない。旧IDは出典と併記し、旧不具合の残存はコードで再確認する。
規約は2026-11-05の確定指示を優先する。設計目標は設計スペースで再策定するまで候補として扱う。
