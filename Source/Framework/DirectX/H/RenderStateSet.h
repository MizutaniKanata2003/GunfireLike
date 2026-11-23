#pragma once

// Blend・Depth・Rasterizerの描画Stateを5つ所有し、Passごとの組合せの設定と、BlendまたはDepthだけの切替を提供する。
// Deviceの生成・Contextの所有・描画先Viewの生成は担当しない。GraphicsSystemが、メンバーとして所有して使う。

//========= Windows インクルード=========
#include <windows.h> // Win32型・定義を使用する。

//========= 外部ライブラリ インクルード=========
#include <d3d11.h> // D3D11のStateのインターフェース、記述、Contextの設定APIを使用する。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

//========= 列挙型=========
// Blend・Depth・Rasterizerの組合せを選ぶ。公開名e_RenderPass、列挙順と暗黙値を互換性のため維持する。
// Render Target・Shaderを切り替える識別値ではなく、以前のStateも自動保存しない。
enum class e_RenderPass
{
  e_SKY,         // DepthとCullを無効化し、Blendなしで描く。
  e_OPAQUE,      // 既定Depth・背面Cull・Blendなしの不透明3D設定。
  e_TRANSPARENT, // Depth比較を維持して書込を止め、Alpha Blend・背面Cullを使う。
  e_SCREEN_UI    // DepthとCullを無効化し、Alpha Blendで画面UIを描く。
};

// 描画Stateの所有と、Contextへの設定を行うクラス。
class RenderStateSet final
{
public:
  //========= 生成・解放関数=========
  // 5つのState（Alpha Blend・Depth無効・Depth読取専用・背面Cull・Cullなし）を、この順に生成する。
  // device: State生成に使うDevice。呼出中だけ参照する。
  // 戻り値: 全て成功したらtrue。最初に失敗したStateは、API名とHRESULTをログへ記録してfalse。生成済みのStateは、呼出側がReleaseで解放する。
  [[nodiscard]] bool Create(ID3D11Device& device);
  // 5つのStateの所有参照を解放する。未生成・解放済みでも呼出可能。
  void Release();

  //========= 設定関数=========
  // 指定したPassの、Blend・Depth・RasterizerをContextへBindする。以前の設定は保存・復元しない。
  // context: 設定先の即時Context。
  // renderPass: 選ぶ設定。未定義値では変更しない。各Stateの個別有効性検証は行わない。
  void Apply(ID3D11DeviceContext& context,e_RenderPass renderPass) const;
  // Blendだけを切り替える。Depth・Cullは変更しない。
  // context: 設定先の即時Context。
  // isEnabled: trueで生成済みAlpha Blend、falseでnullptrによる既定Blend。
  void SetAlphaBlendEnabled(ID3D11DeviceContext& context,bool isEnabled) const;
  // Depthだけを切り替える。Blend・Cullは変更しない。
  // context: 設定先の即時Context。
  // isEnabled: trueで既定Depth、falseで比較・書込を無効化したState。
  void SetDepthTestEnabled(ID3D11DeviceContext& context,bool isEnabled) const;

private:
  //========= 生成の補助関数=========
  // Alpha Blend Stateを生成する。失敗はログへ記録する。
  // device: State生成に使うDevice。
  // 戻り値: 成功したらtrue。
  [[nodiscard]] bool CreateBlendState(ID3D11Device& device);
  // Depth無効Stateと、Depth読取専用Stateを、この順に生成する。失敗はログへ記録する。
  // device: State生成に使うDevice。
  // 戻り値: 両方成功したらtrue。
  [[nodiscard]] bool CreateDepthStates(ID3D11Device& device);
  // 背面CullのStateと、CullなしのStateを、この順に生成する。失敗はログへ記録する。
  // device: State生成に使うDevice。
  // 戻り値: 両方成功したらtrue。
  [[nodiscard]] bool CreateRasterizerStates(ID3D11Device& device);

  //========= 描画State関連=========
  // Depth比較・書込を無効にしたStateの所有参照。Sky・UIで使用する。
  ComPtr<ID3D11DepthStencilState> m_depthDisabledState{};
  // LESS比較を行い、Depthを書き込まない半透明3D用Stateの所有参照。
  ComPtr<ID3D11DepthStencilState> m_depthReadOnlyState{};
  // RGBはSrcAlphaと1-SrcAlphaで加算合成するState。Alpha成分自体はONE・ONEの加算設定を維持する。
  ComPtr<ID3D11BlendState> m_alphaBlendState{};
  // SOLID・背面Cull・DepthClip有効のRasterizer Stateを所有する。
  ComPtr<ID3D11RasterizerState> m_cullBackRasterizerState{};
  // SOLID・Cullなし・DepthClip有効のRasterizer Stateを所有する。
  ComPtr<ID3D11RasterizerState> m_cullNoneRasterizerState{};
};
