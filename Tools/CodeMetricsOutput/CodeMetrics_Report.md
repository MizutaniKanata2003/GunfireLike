# コード指標レポート（I1）

対象: ファイル 156 個、関数 655 個、クラス・構造体 176 個

## 1. 基準を超えた件数

| 項目 | 基準 | 超過の件数 |
|---|---|---|
| 関数の本体の行数 | 50行 | 1 |
| ネストの深さ | 3段 | 0 |
| 関数の引数の数 | 4個 | 1 |
| クラスの行数・メンバー数 | 400行・15個 | 1 |
| ファイルの物理行数 | 500行 | 0 |
| 重複（同じ並び） | 10行以上 | 9 |


## 2. 関数の本体の行数（上位30）

| ファイル | 関数 | 行 | 本体の行数 |
|---|---|---|---|
| Source/main.cpp | WinMain | 110 | 102 |


## 3. ネストの深さ（上位30）

（なし）


## 4. 引数の数（上位30）

| ファイル | 関数 | 行 | 引数 |
|---|---|---|---|
| Source/Scene/Common/Player/CPP/PlayerController.cpp | PlayerController::Update | 70 | 5 |


## 5. クラス・構造体

| ファイル | 名前 | 行 | 本体の行数 | メンバー数 |
|---|---|---|---|---|
| Source/Debug/H/GameDebugUi.h | GAME_DEBUG_INFO | 17 | 28 | 28 |


## 6. ファイルの物理行数

（なし）


## 7. 重複（上位30）

| 行数 | 場所（ファイル:開始行） |
|---|---|
| 23 | Source/Scene/ResultScene/CPP/ResultScene.cpp:18<br>Source/Scene/TitleScene/CPP/TitleScene.cpp:18 |
| 17 | Source/Debug/CPP/GameDebugController.cpp:47<br>Source/Scene/Common/Combat/CPP/PlayerCombatController.cpp:40 |
| 15 | Source/Framework/2D/CPP/HudSpriteRenderer.cpp:154<br>Source/Framework/3D/CPP/ObjModelRenderer.cpp:354 |
| 14 | Source/Framework/DirectX/CPP/RenderStateSet.cpp:113<br>Source/Framework/DirectX/CPP/RenderStateSet.cpp:146 |
| 14 | Source/Scene/GameScene/H/GameScene.h:11<br>Source/Scene/ResultScene/H/ResultScene.h:11<br>Source/Scene/TitleScene/H/TitleScene.h:11 |
| 13 | Source/Scene/ResultScene/H/ResultScene.h:19<br>Source/Scene/TitleScene/H/TitleScene.h:19 |
| 11 | Source/Framework/3D/CPP/BasicMeshRenderer.cpp:113<br>Source/Framework/3D/CPP/ObjModelRenderer.cpp:261 |
| 11 | Source/Framework/DirectX/CPP/RenderStateSet.cpp:133<br>Source/Framework/DirectX/CPP/RenderStateSet.cpp:165 |
| 10 | Source/Debug/CPP/GameDebugUi.cpp:15<br>Source/Debug/CPP/GameDebugUi.cpp:55<br>Source/Debug/CPP/GameDebugUi.cpp:83 |


## 8. 数え方の限界

- 字句解析による簡易的な測定。マクロや特殊な書き方では、関数・クラスを取り違えることがある。
- 関数の本体の行数は、{ と } を含む行を除き、コメントと空行を除いて数える。
- ネストは、波括弧を持つ if・else・for・while・do・switch・Lambda を数える。波括弧のない制御文は数えない。
- 引数は、最上位のカンマの数から求める。仮想関数のoverride・外部APIの形に合わせた宣言も含むため、例外にするかは目視で決める。
- メンバー数は、クラス直下の、括弧を含まない文を数える（static・定数メンバーを含む）。
- 重複は、識別子と数値を無視して比較する。表の行・失敗検出の並びも検出するため、例外にするかは目視で決める。
