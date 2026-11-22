# コード指標レポート（I1）

対象: ファイル 144 個、関数 596 個、クラス・構造体 150 個

## 1. 基準を超えた件数

| 項目 | 基準 | 超過の件数 |
|---|---|---|
| 関数の本体の行数 | 50行 | 14 |
| ネストの深さ | 3段 | 1 |
| 関数の引数の数 | 4個 | 19 |
| クラスの行数・メンバー数 | 400行・15個 | 2 |
| ファイルの物理行数 | 500行 | 1 |
| 重複（同じ並び） | 10行以上 | 26 |


## 2. 関数の本体の行数（上位30）

| ファイル | 関数 | 行 | 本体の行数 |
|---|---|---|---|
| Source/Framework/3D/CPP/BasicMeshRenderer.cpp | BasicMeshRenderer::Initialize | 76 | 138 |
| Source/Framework/3D/CPP/ObjModelRenderer.cpp | ObjModelRenderer::Initialize | 116 | 108 |
| Source/Framework/2D/CPP/HudSpriteRenderer.cpp | HudSpriteRenderer::Initialize | 60 | 104 |
| Source/Scene/Common/Player/CPP/PlayerController.cpp | PlayerController::Update | 49 | 104 |
| Source/main.cpp | WinMain | 108 | 102 |
| Source/Framework/DirectX/CPP/GraphicsSystem.cpp | GraphicsSystem::Init | 95 | 96 |
| Source/Framework/DirectX/CPP/TextureLoader.cpp | TextureLoader::LoadWicTexture | 81 | 85 |
| Source/Framework/2D/CPP/HudRenderer.cpp | HudRenderer::Initialize | 98 | 75 |
| Source/Framework/3D/CPP/DebugLineRenderer.cpp | DebugLineRenderer::Initialize | 64 | 69 |
| Source/Scene/Common/Player/CPP/PlayerController.cpp | PlayerController::UpdateActionState | 272 | 69 |
| Source/Framework/3D/CPP/ObjModelRenderer.cpp | ObjModelRenderer::LoadOBJFile | 382 | 62 |
| Source/Framework/3D/CPP/BasicMeshRenderer.cpp | BasicMeshRenderer::DrawCube | 306 | 58 |
| Source/Scene/GameScene/CPP/GameHud.cpp | GameHud::Draw | 273 | 55 |
| Source/Framework/3D/CPP/DebugLineRenderer.cpp | DebugLineRenderer::DrawLines | 176 | 53 |


## 3. ネストの深さ（上位30）

| ファイル | 関数 | 行 | 深さ |
|---|---|---|---|
| Source/Framework/3D/CPP/ObjModelRenderer.cpp | ObjModelRenderer::LoadOBJFile | 382 | 4 |


## 4. 引数の数（上位30）

| ファイル | 関数 | 行 | 引数 |
|---|---|---|---|
| Source/Framework/Collision/CPP/CollisionMath.cpp | ClipSlab | 31 | 9 |
| Source/Scene/Common/Enemy/CPP/EnemyProjectile.cpp | EnemyProjectileManager::AdvanceProjectile | 112 | 8 |
| Source/Framework/3D/CPP/BasicMeshRenderer.cpp | BasicMeshRenderer::DrawCube | 306 | 7 |
| Source/Framework/DirectX/CPP/GraphicsSystem.cpp | CreateDeviceAndSwapChain | 58 | 7 |
| Source/Framework/DirectX/CPP/GraphicsSystem.cpp | CreateDebugDeviceAndSwapChain | 76 | 7 |
| Source/Scene/Common/Enemy/CPP/EnemyProjectile.cpp | EnemyProjectileManager::Update | 72 | 7 |
| Source/Scene/Common/Player/CPP/PlayerController.cpp | PlayerController::UpdateActionState | 272 | 7 |
| Source/Framework/2D/CPP/HudRenderer.cpp | HudRenderer::DrawQuad | 211 | 6 |
| Source/Framework/2D/CPP/HudSpriteRenderer.cpp | HudSpriteRenderer::DrawSprite | 237 | 6 |
| Source/Scene/Common/Weapon/CPP/WeaponSystem.cpp | WeaponSystem::Update | 50 | 6 |
| Source/Debug/CPP/CollisionDebugView.cpp | CollisionDebugView::Draw | 103 | 5 |
| Source/Framework/3D/CPP/DebugLineRenderer.cpp | DebugLineRenderer::DrawLines | 176 | 5 |
| Source/Framework/3D/CPP/ObjModelRenderer.cpp | ObjModelRenderer::Draw | 286 | 5 |
| Source/Framework/Collision/CPP/CollisionWorld.cpp | CollisionWorld::Raycast | 96 | 5 |
| Source/Scene/Common/Enemy/CPP/Enemy.cpp | Enemy::Initialize | 42 | 5 |
| Source/Scene/Common/Enemy/H/EnemyRangedBehavior.h | Move | 46 | 5 |
| Source/Scene/Common/Object/CPP/SceneObjectFactory.cpp | SceneObjectFactory::Build | 39 | 5 |
| Source/Scene/Common/Player/CPP/PlayerController.cpp | PlayerController::Update | 49 | 5 |
| Source/Scene/GameScene/CPP/GameHud.cpp | GameHud::DrawFittedText | 379 | 5 |


## 5. クラス・構造体

| ファイル | 名前 | 行 | 本体の行数 | メンバー数 |
|---|---|---|---|---|
| Source/Debug/H/GameDebugUi.h | GAME_DEBUG_INFO | 16 | 28 | 28 |
| Source/Scene/GameScene/H/GameHud.h | GAME_HUD_STATE | 23 | 23 | 23 |


## 6. ファイルの物理行数

| ファイル | 物理行数 | コードの行数 |
|---|---|---|
| Source/Scene/GameScene/CPP/GameHud.cpp | 524 | 323 |


## 7. 重複（上位30）

| 行数 | 場所（ファイル:開始行） |
|---|---|
| 49 | Source/Scene/ResultScene/CPP/ResultScene.cpp:27<br>Source/Scene/TitleScene/CPP/TitleScene.cpp:27 |
| 43 | Source/Framework/3D/CPP/BasicMeshRenderer.cpp:186<br>Source/Framework/3D/CPP/DebugLineRenderer.cpp:93 |
| 24 | Source/Framework/2D/CPP/HudSpriteRenderer.cpp:149<br>Source/Framework/3D/CPP/ObjModelRenderer.cpp:210 |
| 24 | Source/Framework/2D/CPP/HudSpriteRenderer.cpp:300<br>Source/Framework/3D/CPP/ObjModelRenderer.cpp:336 |
| 22 | Source/Debug/CPP/DebugPlayer.cpp:33<br>Source/Scene/Common/Player/CPP/PlayerController.cpp:51 |
| 22 | Source/Framework/2D/CPP/HudSpriteRenderer.cpp:99<br>Source/Framework/3D/CPP/ObjModelRenderer.cpp:157 |
| 21 | Source/Framework/2D/CPP/HudRenderer.cpp:163<br>Source/Framework/2D/CPP/HudSpriteRenderer.cpp:81 |
| 19 | Source/Framework/3D/CPP/BasicMeshRenderer.cpp:370<br>Source/Framework/3D/CPP/ObjModelRenderer.cpp:322 |
| 19 | Source/Scene/ResultScene/H/ResultScene.h:23<br>Source/Scene/TitleScene/H/TitleScene.h:23 |
| 17 | Source/Debug/CPP/GameDebugController.cpp:45<br>Source/Scene/Common/Combat/CPP/PlayerCombatController.cpp:38 |
| 17 | Source/Framework/2D/CPP/HudSpriteRenderer.cpp:186<br>Source/Framework/3D/CPP/ObjModelRenderer.cpp:255 |
| 15 | Source/Framework/2D/CPP/HudSpriteRenderer.cpp:28<br>Source/Framework/3D/CPP/BasicMeshRenderer.cpp:44 |
| 14 | Source/Framework/2D/CPP/HudSpriteRenderer.cpp:140<br>Source/Framework/3D/CPP/BasicMeshRenderer.cpp:177<br>Source/Framework/3D/CPP/BasicMeshRenderer.cpp:215<br>Source/Framework/3D/CPP/DebugLineRenderer.cpp:84<br>Source/Framework/3D/CPP/DebugLineRenderer.cpp:119<br>Source/Framework/3D/CPP/ObjModelRenderer.cpp:201 |
| 14 | Source/Scene/Common/Enemy/CPP/EnemyController.cpp:177<br>Source/Scene/Common/Enemy/CPP/EnemyController.cpp:246 |
| 14 | Source/Scene/GameScene/H/GameScene.h:11<br>Source/Scene/ResultScene/H/ResultScene.h:15<br>Source/Scene/TitleScene/H/TitleScene.h:15 |
| 12 | Source/Framework/2D/H/HudSpriteRenderer.h:61<br>Source/Framework/3D/H/BasicMeshRenderer.h:58<br>Source/Framework/3D/H/DebugLineRenderer.h:43 |
| 11 | Source/Framework/2D/CPP/HudRenderer.cpp:161<br>Source/Framework/2D/CPP/HudSpriteRenderer.cpp:79<br>Source/Framework/3D/CPP/ObjModelRenderer.cpp:137 |
| 11 | Source/Framework/3D/CPP/BasicMeshRenderer.cpp:282<br>Source/Framework/DirectX/CPP/GraphicsSystem.cpp:153 |
| 11 | Source/Framework/2D/CPP/HudSpriteRenderer.cpp:298<br>Source/Framework/3D/CPP/BasicMeshRenderer.cpp:385<br>Source/Framework/3D/CPP/ObjModelRenderer.cpp:334 |
| 11 | Source/Framework/3D/CPP/BasicMeshRenderer.cpp:145<br>Source/Framework/3D/CPP/BasicMeshRenderer.cpp:165 |
| 11 | Source/Scene/Common/Enemy/CPP/EnemyController.cpp:236<br>Source/Scene/Common/Enemy/CPP/EnemyController.cpp:297 |
| 10 | Source/Debug/CPP/GameDebugUi.cpp:15<br>Source/Debug/CPP/GameDebugUi.cpp:55<br>Source/Debug/CPP/GameDebugUi.cpp:83 |
| 10 | Source/Framework/2D/CPP/HudRenderer.cpp:74<br>Source/Framework/2D/CPP/HudSpriteRenderer.cpp:35<br>Source/Framework/3D/CPP/BasicMeshRenderer.cpp:51<br>Source/Framework/3D/CPP/DebugLineRenderer.cpp:39 |
| 10 | Source/Framework/2D/CPP/HudRenderer.cpp:182<br>Source/Framework/3D/CPP/ObjModelRenderer.cpp:139 |
| 10 | Source/Framework/2D/CPP/HudSpriteRenderer.cpp:280<br>Source/Framework/3D/CPP/DebugLineRenderer.cpp:201 |
| 10 | Source/Framework/3D/CPP/BasicMeshRenderer.cpp:387<br>Source/Framework/3D/CPP/DebugLineRenderer.cpp:221 |


## 8. 数え方の限界

- 字句解析による簡易的な測定。マクロや特殊な書き方では、関数・クラスを取り違えることがある。
- 関数の本体の行数は、{ と } を含む行を除き、コメントと空行を除いて数える。
- ネストは、波括弧を持つ if・else・for・while・do・switch・Lambda を数える。波括弧のない制御文は数えない。
- 引数は、最上位のカンマの数から求める。仮想関数のoverride・外部APIの形に合わせた宣言も含むため、例外にするかは目視で決める。
- メンバー数は、クラス直下の、括弧を含まない文を数える（static・定数メンバーを含む）。
- 重複は、識別子と数値を無視して比較する。表の行・失敗検出の並びも検出するため、例外にするかは目視で決める。
