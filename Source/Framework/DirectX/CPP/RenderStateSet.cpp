#include "../H/RenderStateSet.h"

// 描画Stateの生成・解放と、Passごとの組合せの設定を実装する。
// 既存の設定値・生成順・失敗時のログのAPI名を維持する。

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsErrorLog.h"

// Bind時に渡すBlend係数を、静的配列としてまとめる。
namespace
{
  //========= Blend設定補助関数=========
  // Bindへ渡す4成分配列へのポインターを返す。静的寿命であり、呼出側は書換え・解放しない。
  const float* GetBlendFactor()
  {
    // 全成分0の既存設定。使用するBlend式がBLEND_FACTORを参照しない場合、RGB合成の係数には使われない。
    static const float BLEND_FACTOR[4]{};
    return BLEND_FACTOR;
  }

  //========= State生成補助関数=========
  // Depth Stencil Stateを1つ生成し、失敗した場合はAPI名とHRESULTを記録する。
  // device: 生成に使用するDevice。
  // description: 生成するStateの記述。参照は保持しない。
  // state: 生成したStateの出力先。GetAddressOfで得た、メンバーのポインターを渡す。失敗した場合は、変更されない。
  // 戻り値: 生成できればtrue。失敗した場合は、ログへ記録してfalse。
  bool CreateDepthStencilStateWithLog(ID3D11Device& device,const D3D11_DEPTH_STENCIL_DESC& description,ID3D11DepthStencilState** state)
  {
    const HRESULT state_result = device.CreateDepthStencilState(&description,state);

    // State生成失敗なら、API名とHRESULTを記録して失敗を返す。
    if(FAILED(state_result)){
      GraphicsErrorLog::Write(L"ID3D11Device::CreateDepthStencilState",state_result);
      return false;
    }
    return true;
  }

  // Rasterizer Stateを1つ生成し、失敗した場合はAPI名とHRESULTを記録する。
  // device: 生成に使用するDevice。
  // description: 生成するStateの記述。参照は保持しない。
  // state: 生成したStateの出力先。GetAddressOfで得た、メンバーのポインターを渡す。失敗した場合は、変更されない。
  // 戻り値: 生成できればtrue。失敗した場合は、ログへ記録してfalse。
  bool CreateRasterizerStateWithLog(ID3D11Device& device,const D3D11_RASTERIZER_DESC& description,ID3D11RasterizerState** state)
  {
    const HRESULT state_result = device.CreateRasterizerState(&description,state);

    // State生成失敗なら、API名とHRESULTを記録して失敗を返す。
    if(FAILED(state_result)){
      GraphicsErrorLog::Write(L"ID3D11Device::CreateRasterizerState",state_result);
      return false;
    }
    return true;
  }

  //========= SampleMask設定=========
  // 全bit 1は全Sampleを許可する指定。
  constexpr UINT ns_SAMPLE_MASK_ALL = 0xffffffff;
}

//========= 生成・解放関数=========
// 5つのStateを、Alpha Blend・Depth無効・Depth読取専用・背面Cull・Cullなしの順に生成する。
bool RenderStateSet::Create(ID3D11Device& device)
{
  // 最初に失敗した時点で、後続のStateは生成せずfalseを返す。失敗のログは、各生成関数が記録する。
  return CreateBlendState(device) && CreateDepthStates(device) && CreateRasterizerStates(device);
}

// 5つのStateの所有参照を解放する。
void RenderStateSet::Release()
{
  m_cullNoneRasterizerState.Reset();
  m_cullBackRasterizerState.Reset();
  m_alphaBlendState.Reset();
  m_depthReadOnlyState.Reset();
  m_depthDisabledState.Reset();
}

//========= 設定関数=========
// 指定したPassのBlend・Depth・RasterizerをContextへBindする。
void RenderStateSet::Apply(ID3D11DeviceContext& context,e_RenderPass renderPass) const
{
  // SampleMaskは全Sampleを許可し、Stencil参照値0も既存値を維持する。
  // nullptrのBlend/Depthはそれぞれ既定State。GetBlendFactorは静的配列で、以前の値の保存先ではない。
  switch(renderPass){
    // 奥行き比較・書込とCullを止め、BlendなしでSkyを描く。以前のStateは復元しない。
  case e_RenderPass::e_SKY:
  context.OMSetBlendState(nullptr,GetBlendFactor(),ns_SAMPLE_MASK_ALL);
  context.OMSetDepthStencilState(m_depthDisabledState.Get(),0);
  context.RSSetState(m_cullNoneRasterizerState.Get());
  return;

  // 既定Depth、背面Cull、Blendなしへ揃える。以前のStateは復元しない。
  case e_RenderPass::e_OPAQUE:
  context.OMSetBlendState(nullptr,GetBlendFactor(),ns_SAMPLE_MASK_ALL);
  context.OMSetDepthStencilState(nullptr,0);
  context.RSSetState(m_cullBackRasterizerState.Get());
  return;

  // Depth比較を維持して書込だけを止め、Alpha Blendと背面Cullを使う。以前のStateは復元しない。
  case e_RenderPass::e_TRANSPARENT:
  context.OMSetBlendState(m_alphaBlendState.Get(),GetBlendFactor(),ns_SAMPLE_MASK_ALL);
  context.OMSetDepthStencilState(m_depthReadOnlyState.Get(),0);
  context.RSSetState(m_cullBackRasterizerState.Get());
  return;

  // DepthとCullを止め、Alpha Blendで画面UIを描く。以前のStateは復元しない。
  case e_RenderPass::e_SCREEN_UI:
  context.OMSetBlendState(m_alphaBlendState.Get(),GetBlendFactor(),ns_SAMPLE_MASK_ALL);
  context.OMSetDepthStencilState(m_depthDisabledState.Get(),0);
  context.RSSetState(m_cullNoneRasterizerState.Get());
  return;

  // 未定義値では新しいStateを設定せず、そのまま戻る。
  default:
  return;
  }
}

// Blendだけを切り替え、他のStateは維持する。
void RenderStateSet::SetAlphaBlendEnabled(ID3D11DeviceContext& context,bool isEnabled) const
{
  // trueは所有するAlpha Blend、falseは既定Blend。SampleMaskは全bit有効で、Depth・Cullは変更しない。
  context.OMSetBlendState(isEnabled ? m_alphaBlendState.Get() : nullptr,GetBlendFactor(),ns_SAMPLE_MASK_ALL);
}

// Depthだけを切り替え、Blend・Cullは維持する。
void RenderStateSet::SetDepthTestEnabled(ID3D11DeviceContext& context,bool isEnabled) const
{
  // trueは既定Depth、falseは比較と書込が無効なState。既定Depthへの復帰であり、呼出前Stateの復元ではない。
  context.OMSetDepthStencilState(isEnabled ? nullptr : m_depthDisabledState.Get(),0);
}

//========= 生成の補助関数=========
// Alpha Blend Stateを生成する。
bool RenderStateSet::CreateBlendState(ID3D11Device& device)
{
  // AlphaToCoverageなし、独立Blendなし。RT0のRGBはSrc*SrcAlpha + Dest*(1-SrcAlpha)。
  // Alpha自体はSrcAlpha + DestAlphaというONE/ONEの加算で、RGBと同じ係数には変えない。全RGBAを書込可能にする。
  D3D11_BLEND_DESC blend_description{};
  blend_description.AlphaToCoverageEnable = FALSE;
  blend_description.IndependentBlendEnable = FALSE;
  D3D11_RENDER_TARGET_BLEND_DESC& render_target_blend = blend_description.RenderTarget[0];
  render_target_blend.BlendEnable = TRUE;
  render_target_blend.SrcBlend = D3D11_BLEND_SRC_ALPHA;
  render_target_blend.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
  render_target_blend.BlendOp = D3D11_BLEND_OP_ADD;
  render_target_blend.SrcBlendAlpha = D3D11_BLEND_ONE;
  render_target_blend.DestBlendAlpha = D3D11_BLEND_ONE;
  render_target_blend.BlendOpAlpha = D3D11_BLEND_OP_ADD;
  render_target_blend.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
  const HRESULT blend_state_result = device.CreateBlendState(&blend_description,m_alphaBlendState.GetAddressOf());

  // Blend State生成失敗なら、API名とHRESULTを記録して失敗を返す。
  if(FAILED(blend_state_result)){
    GraphicsErrorLog::Write(L"ID3D11Device::CreateBlendState",blend_state_result);
    return false;
  }
  return true;
}

// Depth無効Stateと、Depth読取専用Stateを、この順に生成する。
bool RenderStateSet::CreateDepthStates(ID3D11Device& device)
{
  // Sky/UI用はDepth無効・書込なし・Stencil無効。比較値ALWAYSも既存設定として維持する。
  D3D11_DEPTH_STENCIL_DESC depth_disabled_description{};
  depth_disabled_description.DepthEnable = FALSE;
  depth_disabled_description.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
  depth_disabled_description.DepthFunc = D3D11_COMPARISON_ALWAYS;
  depth_disabled_description.StencilEnable = FALSE;

  // Depth無効Stateを生成する。失敗のログは、補助関数が記録する。失敗したら、読取専用Stateは生成しない。
  if(!CreateDepthStencilStateWithLog(device,depth_disabled_description,m_depthDisabledState.GetAddressOf()))return false;

  // 半透明3D用はLESS比較で奥行き判定するが、Depthの書込を止める。Stencilは使わない。
  D3D11_DEPTH_STENCIL_DESC depth_read_only_description{};
  depth_read_only_description.DepthEnable = TRUE;
  depth_read_only_description.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
  depth_read_only_description.DepthFunc = D3D11_COMPARISON_LESS;
  depth_read_only_description.StencilEnable = FALSE;

  // 読取専用Depth Stateを生成する。失敗のログは、補助関数が記録する。
  return CreateDepthStencilStateWithLog(device,depth_read_only_description,m_depthReadOnlyState.GetAddressOf());
}

// 背面CullのStateと、CullなしのStateを、この順に生成する。
bool RenderStateSet::CreateRasterizerStates(ID3D11Device& device)
{
  // 3D用はSOLID・背面Cull・DepthClip有効。ゼロ初期化によりFrontCounterClockwise等はFALSEのまま。
  D3D11_RASTERIZER_DESC cull_back_rasterizer_description{};
  cull_back_rasterizer_description.FillMode = D3D11_FILL_SOLID;
  cull_back_rasterizer_description.CullMode = D3D11_CULL_BACK;
  cull_back_rasterizer_description.DepthClipEnable = TRUE;

  // 背面Cull Stateを生成する。失敗のログは、補助関数が記録する。失敗したら、Cullなしは生成しない。
  if(!CreateRasterizerStateWithLog(device,cull_back_rasterizer_description,m_cullBackRasterizerState.GetAddressOf()))return false;

  // Sky/UI用は両面を描くCullなし設定。SOLIDとDepthClip、その他のゼロ初期値は維持する。
  D3D11_RASTERIZER_DESC cull_none_rasterizer_description{};
  cull_none_rasterizer_description.FillMode = D3D11_FILL_SOLID;
  cull_none_rasterizer_description.CullMode = D3D11_CULL_NONE;
  cull_none_rasterizer_description.DepthClipEnable = TRUE;

  // CullなしStateを生成する。失敗のログは、補助関数が記録する。
  return CreateRasterizerStateWithLog(device,cull_none_rasterizer_description,m_cullNoneRasterizerState.GetAddressOf());
}