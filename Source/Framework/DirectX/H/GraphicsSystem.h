#pragma once

// Direct3D 11のDevice・即時Context・SwapChain、描画先View、描画State（RenderStateSet）を所有する。
// SceneやMeshの生成、ShaderのBind、描画内容の選択は利用側の責務。Windowは所有しない。
// 3D・HUD・文字は、BackBufferの内側に収めた16:9の描画領域（Viewport）へ描く。比率が違うWindowでは、領域の外を黒帯にし、視野・HUDの配置を変えない。
// 明示的UninitでContextの状態も解除する。メンバーの自動破棄だけで同じ終了処理を行うデストラクターは定義しない。

//========= Windows インクルード=========
#include <windows.h> // 初期化先WindowのHWNDと、関連するWin32型・定義を使用する。

//========= 外部ライブラリ インクルード=========
#include <d3d11.h> // D3D11/DXGIのインターフェース、Resource記述、描画Stateの設定とAPIを使用する。

//========= Framework インクルード=========
#include "Framework/DirectX/H/RenderStateSet.h" // 描画Stateの所有と、列挙型e_RenderPassを使用する。
#include "Framework/Etc/H/MathTypes.h"

// 描画基盤の生成・フレーム準備・表示・Resize・終了と、明示的なState切替を提供する。
class GraphicsSystem final
{
public:
  //========= ライフサイクル・フレーム制御関数=========
  // 旧ResourceをUninitしてから生成し、描画先と16:9のViewportを準備してOPAQUE設定へ移す。
  // windowHandle: 表示先の有効なWindow。借用し、メンバーへハンドルを保存しない。
  // width: 初期のBackBufferの幅。pixel単位。引数0に対する独自の早期検証は行わない。
  // height: 初期のBackBufferの高さ。pixel単位。引数0に対する独自の早期検証は行わない。
  // 戻り値: 全生成成功でtrue。検出した最終生成失敗はログ後にUninitしてfalse。旧状態へ復帰しない。
  // Debug構成はDebug Layer付き生成失敗で通常生成へ再試行する。例外捕捉やDevice消失時の自動復旧はない。
  bool Init(HWND windowHandle,unsigned int width,unsigned int height);
  // BackBufferとDepthをBind・Clearし、OPAQUEへ設定する。Contextか必要Viewがなければ何もしない。
  // BackBuffer全体を黒で塗り、16:9の描画領域だけをclearColorで塗る。D3D11.1のContextを取得できない場合は、全体をclearColorで塗る。
  // clearColor: 呼出中に有効なRGBA4成分配列。nullptr不可。値のclampはせず、参照も保持しない。
  // Shaderや各Mesh Resourceをフレームごとに再設定する処理ではない。
  void BeginFrame(const float clearColor[4]);
  // SwapChainへPresent(1,0)を要求する。未生成なら何もせず、失敗はログだけで呼出元へ成否を返さない。
  void EndFrame();
  // 旧Viewと保持サイズを解除してBackBufferをResizeし、新しいViewと16:9のViewportを生成する。
  // width: 新しいBackBufferの幅。pixel単位。0なら全処理を省略する。
  // height: 新しいBackBufferの高さ。pixel単位。0なら全処理を省略する。
  // SwapChainなしでも何もしない。失敗後の旧描画先の復元は行わない。Contextの旧Bind解除も追加しない。
  void Resize(unsigned int width,unsigned int height);
  // View→ContextのClearState→各State→SwapChain・Context・Deviceの順で終了する。
  // 部分初期化・未初期化・終了済みでも呼出可能。外部の借用参照の使用を終えてから呼ぶ。GPU完了待機はしない。
  void Uninit();

  //========= Getter関数=========
  // Deviceの借用ポインター。未生成・終了後はnullptr。取得だけでは所有権を移さずAddRefもしない。
  [[nodiscard]] ID3D11Device* GetDevice() const { return m_device.Get(); }
  // 即時Contextの借用ポインター。Uninit・再初期化をまたいだ利用は、利用側で寿命を管理する。
  [[nodiscard]] ID3D11DeviceContext* GetContext() const { return m_context.Get(); }
  // 16:9の描画領域（Viewport）の幅。pixel単位。BackBuffer全体の幅ではない。View解除後は0。
  // 最後に描画先生成が成功した時の値で、投影の比率とHUDの基準の計算に使う。
  [[nodiscard]] unsigned int GetRenderWidth() const { return m_renderWidth; }
  // 16:9の描画領域（Viewport）の高さ。pixel単位。BackBuffer全体の高さではない。View解除後は0。
  [[nodiscard]] unsigned int GetRenderHeight() const { return m_renderHeight; }

  //========= 描画State設定関数=========
  // PassごとのBlend・Depth・RasterizerをBindする。以前の設定は保存・復元しない。
  // renderPass: 選ぶ設定。未定義値やContextなしでは変更しない。各Stateの個別有効性検証は行わない。
  void SetRenderPass(e_RenderPass renderPass);
  // Blendだけを切り替える。Contextなしでは何もしない。
  // isEnabled: trueで生成済みAlpha Blend、falseでnullptrによる既定Blend。DepthやCullは変更しない。
  void SetAlphaBlendEnabled(bool isEnabled);
  // Depthだけを切り替える。Contextなしでは何もしない。
  // isEnabled: trueで既定Depth、falseで比較・書込を無効化したState。BlendやCullは変更しない。
  void SetDepthTestEnabled(bool isEnabled);
  // 深度だけを、最も遠い値（1.0）へ戻す。一人称の銃を、ワールドの上に重ねて描く前に使う。Contextか深度のViewがなければ、何もしない。
  void ClearDepth()
  {
    if(m_context.Get() == nullptr || m_depthStencilView.Get() == nullptr)return;

    m_context->ClearDepthStencilView(m_depthStencilView.Get(),D3D11_CLEAR_DEPTH,1.0f,0);
  }
private:
  //========= 補助関数=========
  // Device・即時Context・SwapChainを同時生成する。Debug構成では、Debug Layer付きを先に試し、失敗したら通常生成へ再試行する。
  // 失敗した場合は、API名とHRESULTをログへ記録する。生成済みの部分の解放は、呼出側が行う。
  // windowHandle: 表示先の有効なWindow。借用し、保存しない。
  // width: BackBufferの幅。pixel単位。
  // height: BackBufferの高さ。pixel単位。
  // 戻り値: 生成に成功したらtrue。
  bool CreateDevice(HWND windowHandle,unsigned int width,unsigned int height);
  // BackBufferのRTVとDepth用Texture/DSVを生成し、成功後に16:9のViewportと保持サイズを設定する。
  // width: BackBufferとDepthの幅。pixel単位。SwapChainのBackBufferサイズとの対応は呼出側が維持する。
  // height: BackBufferとDepthの高さ。pixel単位。Viewportは、この寸法の内側に収めた16:9の領域になる。
  // 戻り値: 成功でtrue。基盤不足でfalse。検出した生成失敗はViewを部分解放してfalse。
  bool CreateRenderTargets(unsigned int width,unsigned int height);
  // Viewの所有参照を解放して保持サイズを0へ戻す。Contextが別途持つBind参照は解除しない。
  void ReleaseRenderTargets();

  //========= Direct3D主要Resource=========
  // Resource生成用Deviceの所有参照。未生成は空。借用先より先に利用を終了させる。
  ComPtr<ID3D11Device> m_device{};
  // 描画・State設定用の即時Contextの所有参照。BindしたResourceを別参照として保持し得る。
  ComPtr<ID3D11DeviceContext> m_context{};
  // BackBuffer管理・Present用SwapChainの所有参照。Flip Modelへの変更は今回行わない。
  ComPtr<IDXGISwapChain> m_swapChain{};

  //========= Render Target・Depth Stencil関連=========
  // BackBufferへ描画するためのRTVを所有する。Resizeで作り直す。
  ComPtr<ID3D11RenderTargetView> m_renderTargetView{};
  // 奥行き判定先DSVを所有する。Depth TextureはこのViewの参照で寿命を保持する。
  ComPtr<ID3D11DepthStencilView> m_depthStencilView{};

  //========= 描画領域（Viewport）=========
  // 16:9の描画領域の幅。生成成功後に確定する。初期値0は未生成、ReleaseRenderTargetsで0へ戻す。単位はpixel。
  unsigned int m_renderWidth{};
  // 16:9の描画領域の高さ。生成成功後に確定する。初期値0は未生成、ReleaseRenderTargetsで0へ戻す。単位はpixel。
  unsigned int m_renderHeight{};
  // 描画領域の左端のBackBuffer上のX。単位はpixel。黒帯を左右へ均等に配るため、(BackBufferの幅 - 描画領域の幅)の半分になる。
  unsigned int m_viewportLeft{};
  // 描画領域の上端のBackBuffer上のY。単位はpixel。黒帯を上下へ均等に配るため、(BackBufferの高さ - 描画領域の高さ)の半分になる。
  unsigned int m_viewportTop{};

  //========= 描画State関連=========
  // Blend・Depth・Rasterizerの5つのStateを所有し、Passごとの組合せを設定する。Deviceの生成後に作り、Uninitで解放する。
  RenderStateSet m_renderStates{};
};