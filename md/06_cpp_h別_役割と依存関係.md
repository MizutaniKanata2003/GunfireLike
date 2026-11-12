# cpp・h別 役割・依存関係一覧

作成日：2026-11-12 / 版：1.0（整形前スナップショット）

## 根拠・適用範囲
- 本会話に添付・貼り付けられたコードと、最後に提示されたslnx/vcxproj/filtersを基準とする。GitHubのHEADと完全一致する保証はない。コード基準コミットSHAは未確定。
- 再送されたStageGateController.hとGameProgress.hは、訂正後の本文を採用。DebugPlayerは最後の貼付を採用。
- 旧引継ぎ・旧タスク資料を根拠にせず、今回受領コードを説明する。現在の型名・API名を維持する。
- 独自コードはSource配下の38 cpp・47 h、計85ファイル。別にHLSL 5件、設定3件（slnx/vcxproj/filters）。数は提示された登録一覧との照合による。
- 外部ImGuiの6 cpp・3登録Headerは外部依存として扱い、本文の全監査・改変対象には含めない。DirectXTK内部も同様。
- 正常動作はユーザー報告。AIによるWindowsビルド・実機検証・リーク/性能測定は未実施。
- packages.configの本文、Asset実ファイル、NuGet復元結果、個人propsの内容は未確認。登録コードの受領完了と、全依存/全実行経路の検証完了は別。
- 本文は現状の説明資料。企画への移行・整形・削除・GitHub変更は行っていない。

## 読み方
- 「依存先」は直接includeに加え、引数・呼び出しで必要な主要な意味上の依存を含む。厳密な全include一覧ではない。
- 「参照元」は受領コードから分かる主な利用元。網羅的な外部利用を保証しない。
- hとcppを別項目として記載。cppには対応Headerへの依存がある。
- 値メンバー/unique_ptr/ComPtrの所有と、引数・参照・生ポインターの非所有を区別する。

## アプリケーション
### Source/main.cpp
- 役割：WinMain、WindowProcedure、WindowContext。COM・Window・Systemの寿命、フレーム進行、終了要求を統合。
- 依存先：Win32/COM、AudioSystem、GraphicsSystem、InputSystem、AppConfig、GameTimer、ImGuiManager、SceneManager、ImGui Win32 backend。
- 所有：System・Timer・ManagerはWinMainのローカル値。WindowContextはInput/Graphicsへの非所有ポインター。
- 外部からの入口：WindowsのWinMainとWindowProcedure。
- 注意：WM_SIZEでResize。EscapeはScene更新より前に全体終了要求。初期化失敗経路と通常終了の差はリファレンスの確認事項を参照。

## 1. GraphicsSystem

### Source/Framework/DirectX/H/GraphicsSystem.h
- 役割：D3D11のDevice・Context・SwapChain、描画先と描画Stateの管理の公開契約と状態宣言。
- 主なAPI：Init / BeginFrame / EndFrame / Resize / SetRenderPass / Uninit。
- 主な依存先：D3D11、DXGI、WRL、Logger。
- 主な参照元：main、各Scene、各Renderer。
- 所有・保持：COMリソース、描画先サイズ。

### Source/Framework/DirectX/CPP/GraphicsSystem.cpp
- 役割：上記APIの実装。Header内inline定義を除く処理を担当。
- 依存先：対応Header、および上記の実装に必要な依存先。
- 呼び出しの位置：main、各Scene、各Rendererから公開API経由で使用。cpp自体をincludeする構成ではない。
- 挙動・制約：Present(1,0)。DISCARD方式。0サイズResizeは無視。RenderPassはBlend・Depth・Rasterizerを設定し、Shader等は設定しない。

## 2. ShaderBinaryLoader

### Source/Framework/DirectX/H/ShaderBinaryLoader.h
- 役割：CSOをバイト配列へ読む静的Loaderの公開契約と状態宣言。
- 主なAPI：Load。
- 主な依存先：標準ファイル入出力、Logger。
- 主な参照元：BasicMeshRenderer、ObjModelRenderer、HudRenderer。
- 所有・保持：永続状態なし。

### Source/Framework/DirectX/CPP/ShaderBinaryLoader.cpp
- 役割：上記APIの実装。Header内inline定義を除く処理を担当。
- 依存先：対応Header、および上記の実装に必要な依存先。
- 呼び出しの位置：BasicMeshRenderer、ObjModelRenderer、HudRendererから公開API経由で使用。cpp自体をincludeする構成ではない。
- 挙動・制約：失敗時false。出力配列をクリア。

## 3. TextureLoader

### Source/Framework/DirectX/H/TextureLoader.h
- 役割：WIC画像をデコードしTextureとSRVを生成の公開契約と状態宣言。
- 主なAPI：LoadWicTexture。
- 主な依存先：D3D11、WRL、WIC、COM、Logger。
- 主な参照元：BasicMeshRenderer、ObjModelRenderer。
- 所有・保持：呼出元のComPtrへSRVを返す。

### Source/Framework/DirectX/CPP/TextureLoader.cpp
- 役割：上記APIの実装。Header内inline定義を除く処理を担当。
- 依存先：対応Header、および上記の実装に必要な依存先。
- 呼び出しの位置：BasicMeshRenderer、ObjModelRendererから公開API経由で使用。cpp自体をincludeする構成ではない。
- 挙動・制約：COM初期化は外側。BGRA・1 mip。GPU生成とCPU読込を同じ関数で行う。

## 4. BasicMeshRenderer

### Source/Framework/3D/H/BasicMeshRenderer.h
- 役割：Cube描画と必要なGPUリソースの管理の公開契約と状態宣言。
- 主なAPI：Initialize / DrawCube / Uninit。
- 主な依存先：GraphicsSystem、ConstantBuffer、GraphicsConstants、VertexFormat、ShaderBinaryLoader、TextureLoader、Logger。
- 主な参照元：StageField、StageGate、ShopGate、GunViewModel、EnemyHealthBar、GameScene、ShopScene。
- 所有・保持：Shader、VB/IB、定数バッファ、Sampler、床・壁・ObjectのSRV。

### Source/Framework/3D/CPP/BasicMeshRenderer.cpp
- 役割：上記APIの実装。Header内inline定義を除く処理を担当。
- 依存先：対応Header、および上記の実装に必要な依存先。
- 呼び出しの位置：StageField、StageGate、ShopGate、GunViewModel、EnemyHealthBar、GameScene、ShopSceneから公開API経由で使用。cpp自体をincludeする構成ではない。
- 挙動・制約：24頂点・36 index。位置＋UV。RenderPassは呼出側が管理。

## 5. ObjModelRenderer

### Source/Framework/3D/H/ObjModelRenderer.h
- 役割：OBJ読込からGPU生成・描画まで担当の公開契約と状態宣言。
- 主なAPI：Initialize / Draw / Uninit / LoadObjFile / LoadTextureFromFile。
- 主な依存先：GraphicsSystem、ConstantBuffer、GraphicsConstants、VertexFormat、ShaderBinaryLoader、TextureLoader、Logger。
- 主な参照元：EnemyVisual、SkyDome、GameScene。
- 所有・保持：Shader、VB/IB、定数バッファ、Sampler、任意のSRV。

### Source/Framework/3D/CPP/ObjModelRenderer.cpp
- 役割：上記APIの実装。Header内inline定義を除く処理を担当。
- 依存先：対応Header、および上記の実装に必要な依存先。
- 呼び出しの位置：EnemyVisual、SkyDome、GameSceneから公開API経由で使用。cpp自体をincludeする構成ではない。
- 挙動・制約：v/vt/f、扇形三角形化、UVのY反転。法線indexはKeyに含むが法線描画なし。MTL・負index・骨アニメ対応は実装されていない。stoi例外の全経路は実測未確認。

## 6. HudCanvas

### Source/Framework/2D/H/HudCanvas.h
- 役割：1280×720の仮想座標を描画先へ変換の公開契約と状態宣言。
- 主なAPI：コンストラクター / ToScreenPosition / ToScreenSize / ToScreenScale。
- 主な依存先：DirectXMath、algorithm。
- 主な参照元：HudRenderer、HudTextRenderer。
- 所有・保持：均一Scaleと中央寄せOffset。

### Source/Framework/2D/CPP/HudCanvas.cpp
- 役割：上記APIの実装。Header内inline定義を除く処理を担当。
- 依存先：対応Header、および上記の実装に必要な依存先。
- 呼び出しの位置：HudRenderer、HudTextRendererから公開API経由で使用。cpp自体をincludeする構成ではない。
- 挙動・制約：幅高さ0ならScale=0。文字計測結果は仮想座標基準。

## 7. HudRenderer

### Source/Framework/2D/H/HudRenderer.h
- 役割：Quad、照準、HPバー、低HP枠をD3D11で描くの公開契約と状態宣言。
- 主なAPI：Initialize / DrawQuad / DrawCrosshair / DrawPlayerHealthBar / DrawLowHealthWarning / Uninit。
- 主な依存先：GraphicsSystem、HudCanvas、ConstantBuffer、VertexFormat、ShaderBinaryLoader、Logger。
- 主な参照元：GameHud、TitleScene、ShopScene、ResultScene。
- 所有・保持：HUD用Shader、InputLayout、VB/IB、定数バッファ。

### Source/Framework/2D/CPP/HudRenderer.cpp
- 役割：上記APIの実装。Header内inline定義を除く処理を担当。
- 依存先：対応Header、および上記の実装に必要な依存先。
- 呼び出しの位置：GameHud、TitleScene、ShopScene、ResultSceneから公開API経由で使用。cpp自体をincludeする構成ではない。
- 挙動・制約：DrawQuad入力は実装上仮想座標。Alpha Blendは色のAlphaで変更。Depth等は外側のPassに依存。

## 8. HudTextRenderer

### Source/Framework/2D/H/HudTextRenderer.h
- 役割：SpriteBatchとSpriteFontによる文字描画・計測の公開契約と状態宣言。
- 主なAPI：Initialize / Begin / DrawText / MeasureText / End / Uninit。
- 主な依存先：GraphicsSystem、HudCanvas、DirectXTK SpriteBatch/SpriteFont。
- 主な参照元：GameHud、TitleScene、ShopScene、ResultScene。
- 所有・保持：SpriteBatchとSpriteFontをunique_ptr所有、GraphicsSystemは非所有。

### Source/Framework/2D/CPP/HudTextRenderer.cpp
- 役割：上記APIの実装。Header内inline定義を除く処理を担当。
- 依存先：対応Header、および上記の実装に必要な依存先。
- 呼び出しの位置：GameHud、TitleScene、ShopScene、ResultSceneから公開API経由で使用。cpp自体をincludeする構成ではない。
- 挙動・制約：Assets/Fonts/GunfireFont.spritefont。DrawTextは仮想座標を変換、MeasureTextは仮想サイズを返す。Begin/Endの対が必要。

## 9. FadeOverlay

### Source/Framework/2D/H/FadeOverlay.h
- 役割：ImGui全画面フェードの進行・描画の公開契約と状態宣言。
- 主なAPI：StartFadeOut / StartFadeIn / Update / Draw / IsPlaying / IsFadeOutFinished。
- 主な依存先：ImGui、algorithm。
- 主な参照元：SceneManager。
- 所有・保持：Alpha、経過時間、継続時間、Mode。

### Source/Framework/2D/CPP/FadeOverlay.cpp
- 役割：上記APIの実装。Header内inline定義を除く処理を担当。
- 依存先：対応Header、および上記の実装に必要な依存先。
- 呼び出しの位置：SceneManagerから公開API経由で使用。cpp自体をincludeする構成ではない。
- 挙動・制約：最小時間0.01秒。ゲーム固有進捗を持たない。

## 10. InputSystem

### Source/Framework/Input/H/InputSystem.h
- 役割：キー状態とマウスキャプチャ・差分を管理の公開契約と状態宣言。
- 主なAPI：Initialize / Update / OnWindowMessage / ConsumeMouseDelta / SetMouseCaptureEnabled / Uninit。
- 主な依存先：Win32、windowsx、cstring。
- 主な参照元：main、FpsCamera、DebugPlayer、全Scene。
- 所有・保持：現/前キー256、HWND、差分、中心、Captureフラグ。

### Source/Framework/Input/CPP/InputSystem.cpp
- 役割：上記APIの実装。Header内inline定義を除く処理を担当。
- 依存先：対応Header、および上記の実装に必要な依存先。
- 呼び出しの位置：main、FpsCamera、DebugPlayer、全Sceneから公開API経由で使用。cpp自体をincludeする構成ではない。
- 挙動・制約：GetKeyboardStateとWM_MOUSEMOVEを使用。フォーカス喪失でCapture解除。ホイール取得APIは受領版にない。

## 11. FpsCamera

### Source/Framework/Input/H/FpsCamera.h
- 役割：位置・Yaw・Pitchと視線・View行列を管理の公開契約と状態宣言。
- 主なAPI：Update / GetForward / GetRight / GetViewMatrix / SetPosition。
- 主な依存先：MathTypes、InputSystem、Win32、DirectXMath、cmath。
- 主な参照元：GameScene、ShopScene、DebugPlayer。
- 所有・保持：位置、Yaw、Pitch、感度。

### Source/Framework/Input/CPP/FpsCamera.cpp
- 役割：上記APIの実装。Header内inline定義を除く処理を担当。
- 依存先：対応Header、および上記の実装に必要な依存先。
- 呼び出しの位置：GameScene、ShopScene、DebugPlayerから公開API経由で使用。cpp自体をincludeする構成ではない。
- 挙動・制約：Capture中だけ回転。Pitch±89度。位置追従はScene側。::XMLoadFloat3とMathTypesの対応は確認事項。

## 12. GameTimer

### Source/Framework/Etc/H/GameTimer.h
- 役割：実時間測定・ゲームDeltaTime・FPSの公開契約と状態宣言。
- 主なAPI：Initialize / Update / SetTimeScale / GetDeltaTime / GetTotalTime / GetFrameRate。
- 主な依存先：Win32高精度カウンター、AppConfig。
- 主な参照元：main。
- 所有・保持：周波数、前時刻、TimeScale、時間、FPS累積。

### Source/Framework/Etc/CPP/GameTimer.cpp
- 役割：上記APIの実装。Header内inline定義を除く処理を担当。
- 依存先：対応Header、および上記の実装に必要な依存先。
- 呼び出しの位置：mainから公開API経由で使用。cpp自体をincludeする構成ではない。
- 挙動・制約：DeltaTime=min(実時間,0.1)*TimeScale。FPSは実時間。SceneにTimer参照は渡されていない。

## 13. Logger

### Source/Framework/Etc/H/Logger.h
- 役割：分類付きログをVisual Studio出力へ送るの公開契約と状態宣言。
- 主なAPI：Write。
- 主な依存先：Win32 OutputDebugStringW、標準文字列。
- 主な参照元：Graphics、Loader、Renderer、Scene初期化処理。
- 所有・保持：永続状態なし。

### Source/Framework/Etc/CPP/Logger.cpp
- 役割：上記APIの実装。Header内inline定義を除く処理を担当。
- 依存先：対応Header、および上記の実装に必要な依存先。
- 呼び出しの位置：Graphics、Loader、Renderer、Scene初期化処理から公開API経由で使用。cpp自体をincludeする構成ではない。
- 挙動・制約：ファイル保存・UI表示のLoggerではない。

## 14. AudioSystem

### Source/Framework/Audio/H/AudioSystem.h
- 役割：DirectXTK Audioの初期化、BGM・SE、更新・終了の公開契約と状態宣言。
- 主なAPI：Initialize / Update / Uninit / Play*Bgm / StopBgm / Play*Se。
- 主な依存先：DirectXTK Audio、Win32、標準例外。
- 主な参照元：main、全Scene。
- 所有・保持：Implをunique_ptr所有。Engine、4 BGM、7 SE、BGM Instance。

### Source/Framework/Audio/CPP/AudioSystem.cpp
- 役割：上記APIの実装。Header内inline定義を除く処理を担当。
- 依存先：対応Header、および上記の実装に必要な依存先。
- 呼び出しの位置：main、全Sceneから公開API経由で使用。cpp自体をincludeする構成ではない。
- 挙動・制約：PImpl。終了順はBGM Instance→Engine→SE/BGM→Impl。順序の安全性検証は別。

## 15. ImGuiManager

### Source/Framework/Imgui/H/ImGuiManager.h
- 役割：ImGui ContextとWin32/DX11 backendの管理の公開契約と状態宣言。
- 主なAPI：Init / BeginFrame / EndFrame / Uninit。
- 主な依存先：GraphicsSystem、ImGui backend。
- 主な参照元：main。
- 所有・保持：初期化済みフラグ。

### Source/Framework/Imgui/CPP/ImGuiManager.cpp
- 役割：上記APIの実装。Header内inline定義を除く処理を担当。
- 依存先：対応Header、および上記の実装に必要な依存先。
- 呼び出しの位置：mainから公開API経由で使用。cpp自体をincludeする構成ではない。
- 挙動・制約：Scene遷移FadeとGame DebugはImGuiを利用。

## 16. DebugPlayer

### Source/Debug/H/DebugPlayer.h
- 役割：カメラ方向に沿うWASD水平移動の公開契約と状態宣言。
- 主なAPI：Update / GetPosition。
- 主な依存先：FpsCamera、InputSystem、DirectXMath、algorithm、cmath。
- 主な参照元：GameScene、ShopScene。
- 所有・保持：位置、移動速度6。

### Source/Debug/CPP/DebugPlayer.cpp
- 役割：上記APIの実装。Header内inline定義を除く処理を担当。
- 依存先：対応Header、および上記の実装に必要な依存先。
- 呼び出しの位置：GameScene、ShopSceneから公開API経由で使用。cpp自体をincludeする構成ではない。
- 挙動・制約：Y=1.6。XZは固定矩形にclamp。汎用Colliderではない。Shopでも同じ移動範囲。

## 17. Transform

### Source/Scene/Common/H/Transform.h
- 役割：位置・Euler回転・ScaleからWorldを生成の公開契約と状態宣言。
- 主なAPI：SetPosition / SetRotation / SetScale / Get* / GetWorldMatrix。
- 主な依存先：MathTypes。
- 主な参照元：StageField、StageGate、ShopGate、SkyDome、EnemyVisual。
- 所有・保持：位置・回転・Scale。

### Source/Scene/Common/CPP/Transform.cpp
- 役割：上記APIの実装。Header内inline定義を除く処理を担当。
- 依存先：対応Header、および上記の実装に必要な依存先。
- 呼び出しの位置：StageField、StageGate、ShopGate、SkyDome、EnemyVisualから公開API経由で使用。cpp自体をincludeする構成ではない。
- 挙動・制約：World=S*R*T。回転はラジアン、X=Pitch/Y=Yaw/Z=Roll。親子Transformなし。

## 18. SceneManager

### Source/Scene/Common/H/SceneManager.h
- 役割：Scene所有、予約、Fade、共有ゲーム進捗の公開契約と状態宣言。
- 主なAPI：Initialize / Update / Draw / Finalize / RequestSceneChange<TScene> / StartNewGame / GetGameProgress。
- 主な依存先：IScene、GameProgress、FadeOverlay、Input/Graphics/Audio、Logger、TitleScene（cppにはGameSceneもinclude）。
- 主な参照元：main、全Scene。
- 所有・保持：Current/Next unique_ptr、GameProgress、Fade、Systemへの非所有ポインター。

### Source/Scene/Common/CPP/SceneManager.cpp
- 役割：上記APIの実装。Header内inline定義を除く処理を担当。
- 依存先：対応Header、および上記の実装に必要な依存先。
- 呼び出しの位置：main、全Sceneから公開API経由で使用。cpp自体をincludeする構成ではない。
- 挙動・制約：要求時に次Sceneを生成。黒画面でInitialize/Init。失敗時旧Scene保持。Finalizeは予約Sceneをそのまま破棄。

## 19. StageProgressController

### Source/Scene/Common/H/StageProgressController.h
- 役割：敵撃破後の初回クリア・報酬・遷移判定の公開契約と状態宣言。
- 主なAPI：EvaluateEnemyDefeat。
- 主な依存先：独自外部依存なし。
- 主な参照元：GameScene。
- 所有・保持：保持状態なし。StageClearResultを返す。

### Source/Scene/Common/CPP/StageProgressController.cpp
- 役割：上記APIの実装。Header内inline定義を除く処理を担当。
- 依存先：対応Header、および上記の実装に必要な依存先。
- 呼び出しの位置：GameSceneから公開API経由で使用。cpp自体をincludeする構成ではない。
- 挙動・制約：既クリアなら全要求false。最終StageならResult要求、それ以外はStageClear要求。

## 20. StageGateController

### Source/Scene/Common/H/StageGateController.h
- 役割：前後Gateの利用可否と使用対象の選択の公開契約と状態宣言。
- 主なAPI：GetAvailability / GetUseResult。
- 主な依存先：StageGate、DirectXMath、algorithm。
- 主な参照元：GameScene。
- 所有・保持：保持状態なし。Availability/UseResultを返す。

### Source/Scene/Common/CPP/StageGateController.cpp
- 役割：上記APIの実装。Header内inline定義を除く処理を担当。
- 依存先：対応Header、および上記の実装に必要な依存先。
- 呼び出しの位置：GameSceneから公開API経由で使用。cpp自体をincludeする構成ではない。
- 挙動・制約：優先順位は前→次→Shop。Scene遷移は行わない。

## 21. StageGate

### Source/Scene/Common/H/StageGate.h
- 役割：ステージ前後・Shop GateのTransformと近接判定・描画の公開契約と状態宣言。
- 主なAPI：Initialize / Update / IsPlayerNear / Draw。
- 主な依存先：Transform、BasicMeshRenderer、GraphicsSystem。
- 主な参照元：GameScene、StageGateController。
- 所有・保持：種類、Transform、色、Animation時間。

### Source/Scene/Common/CPP/StageGate.cpp
- 役割：上記APIの実装。Header内inline定義を除く処理を担当。
- 依存先：対応Header、および上記の実装に必要な依存先。
- 呼び出しの位置：GameScene、StageGateControllerから公開API経由で使用。cpp自体をincludeする構成ではない。
- 挙動・制約：XZ距離判定。Ray照準を使うShopGateとは別型。

## 22. StageField

### Source/Scene/Common/H/StageField.h
- 役割：固定の床・4壁の配置と描画の公開契約と状態宣言。
- 主なAPI：Initialize / Draw。
- 主な依存先：Transform、BasicMeshRenderer、GraphicsSystem。
- 主な参照元：GameScene。
- 所有・保持：5個のTransform。

### Source/Scene/Common/CPP/StageField.cpp
- 役割：上記APIの実装。Header内inline定義を除く処理を担当。
- 依存先：対応Header、および上記の実装に必要な依存先。
- 呼び出しの位置：GameSceneから公開API経由で使用。cpp自体をincludeする構成ではない。
- 挙動・制約：描画のみ。壁衝突・ステージ進行を持たない。

## 23. SkyDome

### Source/Scene/Common/H/SkyDome.h
- 役割：カメラ追従SkyのTransformとPass調整の公開契約と状態宣言。
- 主なAPI：Initialize / Draw。
- 主な依存先：Transform、ObjModelRenderer、GraphicsSystem。
- 主な参照元：GameScene。
- 所有・保持：Transform。

### Source/Scene/Common/CPP/SkyDome.cpp
- 役割：上記APIの実装。Header内inline定義を除く処理を担当。
- 依存先：対応Header、および上記の実装に必要な依存先。
- 呼び出しの位置：GameSceneから公開API経由で使用。cpp自体をincludeする構成ではない。
- 挙動・制約：Drawで位置を変更。Sky Passで描画後Opaqueへ戻す。

## 24. ProjectileSystem

### Source/Scene/Common/H/ProjectileSystem.h
- 役割：16弾Poolの生成・移動・寿命の公開契約と状態宣言。
- 主なAPI：Initialize / Spawn / Update / GetProjectiles。
- 主な依存先：array、DirectXMath。
- 主な参照元：GameScene。
- 所有・保持：16個のProjectile（位置・方向・寿命・有効状態）。

### Source/Scene/Common/CPP/ProjectileSystem.cpp
- 役割：上記APIの実装。Header内inline定義を除く処理を担当。
- 依存先：対応Header、および上記の実装に必要な依存先。
- 呼び出しの位置：GameSceneから公開API経由で使用。cpp自体をincludeする構成ではない。
- 挙動・制約：空きなしは生成しない。衝突・ダメージなし。現行Gameでは視覚用。

## 25. PlayerHealthStateController

### Source/Scene/Common/H/PlayerHealthStateController.h
- 役割：低HPの出入りと死亡判定の公開契約と状態宣言。
- 主なAPI：Initialize / Update。
- 主な依存先：algorithm。
- 主な参照元：GameScene。
- 所有・保持：前回の低HP状態。

### Source/Scene/Common/CPP/PlayerHealthStateController.cpp
- 役割：上記APIの実装。Header内inline定義を除く処理を担当。
- 依存先：対応Header、および上記の実装に必要な依存先。
- 呼び出しの位置：GameSceneから公開API経由で使用。cpp自体をincludeする構成ではない。
- 挙動・制約：結果はisLowHealth/didEnter/didLeave/isPlayerDead。HP0は低HP扱いでない。

## 26. PlayerCombatController

### Source/Scene/Common/H/PlayerCombatController.h
- 役割：通常/旧特殊攻撃の要求とCooldownの公開契約と状態宣言。
- 主なAPI：Initialize / Update / RequestNormalShot / RequestSpecialAttack / IsSpecialAttackReady。
- 主な依存先：algorithm。
- 主な参照元：GameScene。
- 所有・保持：特殊攻撃残りCooldown。

### Source/Scene/Common/CPP/PlayerCombatController.cpp
- 役割：上記APIの実装。Header内inline定義を除く処理を担当。
- 依存先：対応Header、および上記の実装に必要な依存先。
- 呼び出しの位置：GameSceneから公開API経由で使用。cpp自体をincludeする構成ではない。
- 挙動・制約：SEやダメージは適用しない。旧Q特殊攻撃。

## 27. GunViewModel

### Source/Scene/Common/H/GunViewModel.h
- 役割：Camera追従のCube銃・銃身・Muzzle Flashの公開契約と状態宣言。
- 主なAPI：Initialize / Update / TriggerMuzzleFlash / DrawOpaque / DrawTransparent。
- 主な依存先：BasicMeshRenderer、GraphicsSystem、DirectXMath、algorithm。
- 主な参照元：GameScene。
- 所有・保持：Camera空間位置/Scale、Flash Timer。

### Source/Scene/Common/CPP/GunViewModel.cpp
- 役割：上記APIの実装。Header内inline定義を除く処理を担当。
- 依存先：対応Header、および上記の実装に必要な依存先。
- 呼び出しの位置：GameSceneから公開API経由で使用。cpp自体をincludeする構成ではない。
- 挙動・制約：WorldはScale*Translation*inverse(View)。Flashは0.08秒。Transparent Passへ変更する。

## 28. GameHud

### Source/Scene/Common/H/GameHud.h
- 役割：GameHudStateを受け画面HUDを描くの公開契約と状態宣言。
- 主なAPI：Draw。
- 主な依存先：HudRenderer、HudTextRenderer、GraphicsSystem、DirectXColors、cwchar。
- 主な参照元：GameScene。
- 所有・保持：自身にゲーム進捗を保持しない。

### Source/Scene/Common/CPP/GameHud.cpp
- 役割：上記APIの実装。Header内inline定義を除く処理を担当。
- 依存先：対応Header、および上記の実装に必要な依存先。
- 呼び出しの位置：GameSceneから公開API経由で使用。cpp自体をincludeする構成ではない。
- 挙動・制約：旧Q特殊攻撃、5ステージ、死亡25%減などの案内あり。終了時Opaqueへ戻す。

## 29. EnemyVisual

### Source/Scene/Common/H/EnemyVisual.h
- 役割：敵Transform、浮遊回転、モデル描画、命中位置の公開契約と状態宣言。
- 主なAPI：Initialize / Update / Draw / GetPosition / GetHitSphereCenter / GetHealthBarPosition。
- 主な依存先：Transform、ObjModelRenderer、GraphicsSystem、cmath。
- 主な参照元：GameScene。
- 所有・保持：Transform、基準位置、Animation時間。

### Source/Scene/Common/CPP/EnemyVisual.cpp
- 役割：上記APIの実装。Header内inline定義を除く処理を担当。
- 依存先：対応Header、および上記の実装に必要な依存先。
- 呼び出しの位置：GameSceneから公開API経由で使用。cpp自体をincludeする構成ではない。
- 挙動・制約：AI・HPを保持しない。Draw自体はPass変更なし。

## 30. EnemyHealthBar

### Source/Scene/Common/H/EnemyHealthBar.h
- 役割：敵頭上のY軸Billboard HPバーの公開契約と状態宣言。
- 主なAPI：Draw。
- 主な依存先：BasicMeshRenderer、GraphicsSystem、DirectXMath、algorithm。
- 主な参照元：GameScene。
- 所有・保持：保持状態なし。

### Source/Scene/Common/CPP/EnemyHealthBar.cpp
- 役割：上記APIの実装。Header内inline定義を除く処理を担当。
- 依存先：対応Header、および上記の実装に必要な依存先。
- 呼び出しの位置：GameSceneから公開API経由で使用。cpp自体をincludeする構成ではない。
- 挙動・制約：Transparentに設定。HP比で幅を変え、前景をCamera側へ補正。描画後のPass復帰はしない。

## 31. EnemyController

### Source/Scene/Common/H/EnemyController.h
- 役割：敵HP、生死、通常/特殊攻撃タイマーの公開契約と状態宣言。
- 主なAPI：Initialize / UpdateCombat / TakeDamage / GetCurrentHp / GetMaxHp / IsDead。
- 主な依存先：algorithm。
- 主な参照元：GameScene。
- 所有・保持：HP、State、攻撃設定と累積時間。

### Source/Scene/Common/CPP/EnemyController.cpp
- 役割：上記APIの実装。Header内inline定義を除く処理を担当。
- 依存先：対応Header、および上記の実装に必要な依存先。
- 呼び出しの位置：GameSceneから公開API経由で使用。cpp自体をincludeする構成ではない。
- 挙動・制約：移動AIなし。特殊攻撃優先。実Damageと撃破結果を返し、報酬は適用しない。

## 32. CombatSystem

### Source/Scene/Common/H/CombatSystem.h
- 役割：Ray-Sphereと水平距離の共通判定の公開契約と状態宣言。
- 主なAPI：RaycastSphere / IsHitScanHit / GetHorizontalDistanceSquared / IsWithinRangeSquared。
- 主な依存先：DirectXMath、DirectXCollision、algorithm。
- 主な参照元：GameScene、ShopScene、ShopGate。
- 所有・保持：保持状態なし。

### Source/Scene/Common/CPP/CombatSystem.cpp
- 役割：上記APIの実装。Header内inline定義を除く処理を担当。
- 依存先：対応Header、および上記の実装に必要な依存先。
- 呼び出しの位置：GameScene、ShopScene、ShopGateから公開API経由で使用。cpp自体をincludeする構成ではない。
- 挙動・制約：方向を正規化。半径・最大距離が0以下なら非命中。HP・SE・報酬なし。

## 33. TitleScene

### Source/Scene/TitleScene/H/TitleScene.h
- 役割：タイトルUI、入力、BGM、新規開始の公開契約と状態宣言。
- 主なAPI：Initialize / Init / Update / Draw / Finalize / StartGame。
- 主な依存先：IScene、SceneManager、ShopScene、Input/Graphics/Audio、HudRenderer、HudTextRenderer、Logger。
- 主な参照元：SceneManager、ShopScene、ResultScene。
- 所有・保持：各System参照、HUD2種を値所有。

### Source/Scene/TitleScene/CPP/TitleScene.cpp
- 役割：上記APIの実装。Header内inline定義を除く処理を担当。
- 依存先：対応Header、および上記の実装に必要な依存先。
- 呼び出しの位置：SceneManager、ShopScene、ResultSceneから公開API経由で使用。cpp自体をincludeする構成ではない。
- 挙動・制約：Enter→進捗初期化→Shop予約。描画ボタンのクリック判定なし。

## 34. ShopScene

### Source/Scene/ShopScene/H/ShopScene.h
- 役割：3D Shop、照準選択、購入、Gate、HUDの公開契約と状態宣言。
- 主なAPI：Initialize / Init / Update / Draw / Finalize / GetAimedInteractionTarget / TryInteractWithTarget。
- 主な依存先：IScene、SceneManager、GameProgress、DebugPlayer、FpsCamera、CombatSystem、ShopGate、BasicMeshRenderer、HudRenderer、HudTextRenderer、Input/Graphics/Audio、Logger。
- 主な参照元：TitleScene、GameScene。
- 所有・保持：Player/Camera、判定、2 Gate、Renderer、選択・購入結果・Timer。

### Source/Scene/ShopScene/CPP/ShopScene.cpp
- 役割：上記APIの実装。Header内inline定義を除く処理を担当。
- 依存先：対応Header、および上記の実装に必要な依存先。
- 呼び出しの位置：TitleScene、GameSceneから公開API経由で使用。cpp自体をincludeする構成ではない。
- 挙動・制約：4強化固定Table。最短Ray命中を選択。Eで購入/遷移、F1でCapture。

## 35. ShopGate

### Source/Scene/ShopScene/H/ShopGate.h
- 役割：ShopのChallenge/Title GateのRaycastと描画の公開契約と状態宣言。
- 主なAPI：Initialize / Update / Raycast / Draw。
- 主な依存先：Transform、CombatSystem、BasicMeshRenderer、GraphicsSystem。
- 主な参照元：ShopScene。
- 所有・保持：種類、Transform、色、Sphere半径、時間。

### Source/Scene/ShopScene/CPP/ShopGate.cpp
- 役割：上記APIの実装。Header内inline定義を除く処理を担当。
- 依存先：対応Header、および上記の実装に必要な依存先。
- 呼び出しの位置：ShopSceneから公開API経由で使用。cpp自体をincludeする構成ではない。
- 挙動・制約：照準で1.1倍拡大。描画サイズとSphere半径は別に設定。

## 36. ResultScene

### Source/Scene/ResultScene/H/ResultScene.h
- 役割：既存ゲームのクリア結果表示とTitle遷移の公開契約と状態宣言。
- 主なAPI：Initialize / Init / Update / Draw / Finalize / ReturnToTitle。
- 主な依存先：IScene、SceneManager、GameProgress、TitleScene、Input/Graphics/Audio、HudRenderer、HudTextRenderer、Logger。
- 主な参照元：GameScene。
- 所有・保持：System参照、HUD2種を値所有。

### Source/Scene/ResultScene/CPP/ResultScene.cpp
- 役割：上記APIの実装。Header内inline定義を除く処理を担当。
- 依存先：対応Header、および上記の実装に必要な依存先。
- 呼び出しの位置：GameSceneから公開API経由で使用。cpp自体をincludeする構成ではない。
- 挙動・制約：EnterでTitle。最終HP表示はmaxHp。死亡用Resultや周回継続は未実装。

## 37. GameScene

### Source/Scene/GameScene/H/GameScene.h
- 役割：戦闘・描画・報酬・進行・副作用を統合の公開契約と状態宣言。
- 主なAPI：Initialize / Init / Update / Draw / Finalize、各Update* / Draw* / ExecuteNormalShot / ExecuteSpecialAttack。
- 主な依存先：SceneManager、GameProgress、StageData、ShopScene、ResultScene、DebugPlayer、FpsCamera、CombatSystem、PlayerHealth、PlayerHealthStateController、PlayerCombatController、EnemyController、ProjectileSystem、EnemyVisual、StageField、StageGate、StageGateController、StageProgressController、GunViewModel、SkyDome、BasicMeshRenderer、ObjModelRenderer、GameHud、HudRenderer、HudTextRenderer、EnemyHealthBar、Input/Graphics/Audio、Logger、ImGui。
- 主な参照元：ShopScene、SceneManager（include）。
- 所有・保持：上記ComponentとRendererを値所有、GamePhase/要求/命中フラグ。

### Source/Scene/GameScene/CPP/GameScene.cpp
- 役割：上記APIの実装。Header内inline定義を除く処理を担当。
- 依存先：対応Header、および上記の実装に必要な依存先。
- 呼び出しの位置：ShopScene、SceneManager（include）から公開API経由で使用。cpp自体をincludeする構成ではない。
- 挙動・制約：1敵。HitScan即時Damage、Projectile視覚。死亡はShop、最終初回撃破はResult。敵Drawが2か所。

## Header単独の10ファイル

### Source/Framework/Etc/H/AppConfig.h
- 役割：ウィンドウサイズ1280×720、クラス名、タイトル、最大DeltaTime=0.1。
- 主な依存先：cstdint。
- 主な参照元：main、GameTimer。
- 特記事項：定数のみ。

### Source/Framework/Etc/H/MathTypes.h
- 役割：DirectX数学型とComPtrの別名、数学ラッパー。
- 主な依存先：DirectXMath、WRL。
- 主な参照元：FpsCamera、Transform。
- 特記事項：グローバルの型別名・inline関数。XMLoadFloat3ラッパーは受領本文にない。

### Source/Framework/Etc/H/GameObject.h
- 役割：更新/描画等を持つ抽象基底と破棄要求。
- 主な依存先：独自依存なし。
- 主な参照元：受領コードで利用箇所未確認。
- 特記事項：Destroy要求は自動削除ではない。専用cppなし。

### Source/Framework/DirectX/H/ConstantBuffer.h
- 役割：16バイト単位の型を用いる定数Bufferテンプレート。
- 主な依存先：D3D11、WRL、標準Header。
- 主な参照元：BasicMeshRenderer、ObjModelRenderer、HudRenderer。
- 特記事項：COM Buffer所有。Init/Update/Get/Uninit。専用cppなし。

### Source/Framework/DirectX/H/GraphicsConstants.h
- 役割：Camera/Object/Material/Light/PostProcessのCPU定数構造体。
- 主な依存先：DirectXMath。
- 主な参照元：BasicMeshRenderer、ObjModelRenderer。
- 特記事項：サイズassertあり。Light/PostProcessは定義存在と利用実装を区別。

### Source/Framework/DirectX/H/VertexFormat.h
- 役割：頂点属性の種類を示すenum。
- 主な依存先：独自依存なし。
- 主な参照元：BasicMeshRenderer、ObjModelRenderer、HudRenderer。
- 特記事項：実際のInputLayoutは各Rendererが生成。

### Source/Scene/Common/H/IScene.h
- 役割：Initialize/Init/Update/Draw/Finalizeの仮想契約。
- 主な依存先：独自依存なし。
- 主な参照元：SceneManager、4派生Scene。
- 特記事項：unique_ptr<IScene>による多態的所有。専用cppなし。

### Source/Scene/Common/H/StageData.h
- 役割：5ステージの敵パラメータ・報酬の固定Table。
- 主な依存先：array。
- 主な参照元：GameProgress、GameScene。
- 特記事項：StageConstantsとSTAGE_DATA。新企画の部屋/サイクル設定ではない。

### Source/Scene/Common/H/PlayerHealth.h
- 役割：現在/最大HP、初期化、全回復、Damage、死亡判定。
- 主な依存先：algorithm。
- 主な参照元：GameScene。
- 特記事項：現行は任意量回復API・回復アイテム管理なし。

### Source/Scene/Common/H/GameProgress.h
- 役割：Scene間の進捗・通貨・能力・統計と購入処理。
- 主な依存先：StageData、algorithm、array。
- 主な参照元：SceneManager、GameScene、ShopScene、ResultScene。
- 特記事項：SceneManagerが値所有。死亡25%減、与Damage報酬、4強化、5ステージ状態。専用cppなし。

## 付録：HLSLの依存関係
|ファイル（Source/Framework/Shaders配下）|役割・依存|
|---|---|
|ShaderCommon.hlsl|Camera b0、Object b1、Material b2と位置+UVの入出力。b3 Light/b4 PostProcessはコメント上の予約で、ここにcbuffer定義はない|
|BasicColorVS.hlsl|ShaderCommonをinclude。位置にWorld、ViewProjectionを適用、UVタイリング、色を出力|
|BasicColorPS.hlsl|ShaderCommonをinclude。useTextureで単色/Texture×色を切替。t0/s0を使用|
|HudVS.hlsl|独立したHudBuffer b0。ピクセル座標をClip座標へ変換|
|HudPS.hlsl|入力色を出力|

## 付録：設定ファイル
- GunfireLike.slnx：同階層のGunfireLike.vcxprojを参照。
- GunfireLike.vcxproj：独自/外部コード登録、C++20、v145、x64のD3D11リンク、SM5、Asset/CSOコピー、DirectXTK targets。
- GunfireLike.vcxproj.filters：Visual Studio上の表示分類。実際のビルド登録はvcxprojと照合する。
- packages.config：登録はあるが本文未受領。新たな独自コードが不足しているという意味ではない。

## 変更時の更新方針
- 改名：宣言・定義・呼出・include・vcxproj・filters・本文を同時に照合。
- 依存先削除：逆参照元、生成/解放順、GPU BindとScene遷移を確認。
- 整形後はこの資料の版を更新し、整形前スナップショットと区別する。
