# 最小Sceneの適用手順

対象：MizutaniKanata2003/GunfireLike の α ブランチ。GitHubには書き込んでいない。
確認基準：2026-11-12にαから取得したHeader/実装。GameScene.cppのツール応答末尾は省略があるため、先にユーザーが貼り付けた全文も参照した。ブランチのHEAD固定SHAは未取得。

## 置換（4ファイル全体）
- TitleScene.h → Source/Scene/TitleScene/H/TitleScene.h
- TitleScene.cpp → Source/Scene/TitleScene/CPP/TitleScene.cpp
- GameScene.h → Source/Scene/GameScene/H/GameScene.h
- GameScene.cpp → Source/Scene/GameScene/CPP/GameScene.cpp
追加ファイルやプロジェクト登録変更は不要。既存4ファイルを全体置換する。UTF-8 BOM付き。

## この段階の範囲
- TitleはEnterでGameへ。旧進捗初期化とShop依存を除去。
- GameはWASD移動、視点、床壁、F1 Capture切替、F2でTitle、ImGuiの情報表示。
- 敵、弾、HP、通貨、強化、旧Q特殊、Gate、Sky、旧GameHudをSceneから切り離す。
- Escape終了はmainの既存処理。
- F2のTitle復帰は遷移確認用。最終ゲーム仕様ではない。
- マウス解放中はGameのWASD移動を停止。
- DebugPlayer初期位置を0秒Updateで既存範囲内へclamp。既存DebugPlayer本体は変更しない。
- 全体整形・型名省略は次工程。新しい4ファイルのみ規約に合わせ、外部APIの名前は維持。

## 重要：まだ旧ファイルを削除しない
αのSceneManagerは依然GameProgressを保持し、InitializeでStartNewGameを呼ぶ。
ShopScene/ResultSceneはvcxprojに登録されたままで、これらもGameProgress等に依存する。
今回は4ファイルの置換だけなのでGameProgress/StageData/旧Scene/旧Controllerは物理削除しない。
この4ファイルのビルド確認後、SceneManagerとvcxproj/filtersを含めた削除を別のまとまった変更で行う。

## 必要Resourceは残す
- BasicColorVS/PSのCSO、Floor/Wall/Objectの画像：BasicMeshRendererの初期化に必要。
- GunfireFont.spritefont：TitleのTextに必要。
- AudioSystemが初期ロードする全音声：mainでの初期化に必要。
- ImGui本体とWin32/DX11 backend、DirectXTK：維持。

## 静的確認
- 既存ISceneの5APIとconstructor契約を維持。
- 4ファイルには旧GameProgress/ShopScene/ResultScene/敵/弾/旧HUDへの依存なし。
- 公開APIと呼出、宣言/定義順、2スペース・タブなし・180文字制限を確認。
- Init成功後にCapture/BGMを変更し、Finalizeは共有Systemを操作しない。
- 既存FrameworkをWindowsでコンパイル/実行してはいない。ビルド成功保証ではない。
- FpsCameraの既存::XMLoadFloat3等、既存Frameworkの懸案はこの変更で勝手に修正しない。

## ユーザー確認
1. αでバックアップ/差分を保持し、4ファイルを置換。
2. Debug x64 Rebuild。失敗時は旧実行ファイルを起動して成功扱いしない。
3. Title表示→Enter→床壁の表示→WASD/視点。
4. F1で解放/再固定、解放中に移動が止まること。
5. F2でTitle→Enterで再度Game、位置が初期化されること。
6. Resize、最小化/復帰、Escape終了。
7. Release x64でRebuild・主要操作。
エラーは先頭のコンパイルエラーと対象ファイル/行を共有。確認後に旧ファイル削除へ進む。
