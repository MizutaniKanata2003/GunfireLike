#pragma once

// Shaderへ転送するCamera・Object・Material、および未接続のLight・PostProcess用CPUデータ型を定義する。
// Resource生成・更新・Bind・行列計算は行わない。利用側が値を準備し、対応HLSLとフィールド順・型を揃える。
// フィールドの追加・削除・並べ替え、型・初期値・配置の変更は今回行わない。型の存在は機能実装済みを意味しない。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

//========= Camera転送データ=========
// ShaderCommon.hlslのCameraConstantsと対応する64バイトの転送型。現行3D RendererはVS b0へBindする。
struct CAMERA_CONSTANTS final
{
  // World→View→Clipの合成行列を保存する。RendererがView*Projectionを転置して格納する契約。
  // {}は全成分0の初期値で、単位行列ではない。描画前に利用側が有効な行列を設定する。
  XMFLOAT4X4 viewProjectionMatrix{};
};

//========= Object転送データ=========
// ShaderCommon.hlslのObjectConstantsと対応する80バイトの転送型。現行3D RendererはVS/PS b1へBindする。
struct OBJECT_CONSTANTS final
{
  // Local→World行列の保存先。64バイトの行列の後ろにRGBA色が続く。転置はRenderer側で行う。
  // {}は全成分0であり、単位行列ではない。この型の定義だけでは行列を生成しない。
  XMFLOAT4X4 worldMatrix{};
  // ObjectのRGBA色。各成分1の既定値は不透明な白で、BasicColorではTexture使用時にTexture色との積に使う。
  // 単色時はObject色を使う。値のclampやBlend設定をこのフィールドで実行するものではない。
  XMFLOAT4 color{1.0f,1.0f,1.0f,1.0f};
};

//========= Material転送データ=========
// ShaderCommon.hlslのMaterialConstantsと対応する32バイト。現行3D RendererはVS/PS b2へBindする。
// baseColor(16)、uvTiling(8)、useTexture(4)、padding(4)の順とバイト幅を維持する。
struct MATERIAL_CONSTANTS final
{
  // Material基準のRGBA色。既定は各成分1の不透明な白。現行BasicColorでは未参照だが転送配置のため残す。
  XMFLOAT4 baseColor{1.0f,1.0f,1.0f,1.0f};
  // U・Vへ掛ける単位なし倍率。既定(1,1)は元のUVを維持し、BasicColorVSが頂点UVへ乗算する。
  // 画像の繰り返し・補間の動作は別途BindするSamplerにも依存する。
  XMFLOAT2 uvTiling{1.0f,1.0f};
  // Texture使用のfloat転送値。既定0は未使用。BasicColorPSは0.5未満なら画像を読まずObject色を返す。
  // CPUのboolへ変更すると転送型が変わるため、このfloatを維持する。画像のロードやBindは別の処理。
  float useTexture{};
  // 対応HLSL末尾のfloatと配置を揃える4バイトの余白。既定0で、描画計算には使わない。
  float padding{};
};

//========= Light転送データ=========
// 平行光源用の32バイトCPUデータ型。ShaderCommon.hlslのb3は用途の記述だけで、このcbuffer宣言はない。
// 実際のShader接続、方向の座標系、照明計算との対応は別途実装・照合が必要。
struct LIGHT_CONSTANTS final
{
  // 光の方向の3成分。既定(0,-1,0)は-Y方向。利用する座標系や正規化の必要性は接続先との契約で決める。
  XMFLOAT3 direction{0.0f,-1.0f,0.0f};
  // 光の強さの既存パラメーター。既定1を維持するが、物理単位や適用式は未接続のShaderで確定していない。
  float intensity{1.0f};
  // 光のRGB値。既定(1,1,1)は白で、Alpha成分は持たない。色空間変換や範囲制限は行わない。
  XMFLOAT3 color{1.0f,1.0f,1.0f};
  // 環境光の強さの既存パラメーター。既定0.1を維持する。具体値の採用経緯や適用式は断定しない。
  float ambientIntensity{0.1f};
};

//========= PostProcess転送データ=========
// ポスト処理用の32バイトCPUデータ型。ShaderCommon.hlslのb4は用途の記述だけで、このcbuffer宣言はない。
// 対応する効果処理が動作中であることを意味しない。利用時は各値とShaderの契約を別途定める。
struct POST_PROCESS_CONSTANTS final
{
  // 対象画像の幅・高さの保存先。既定(0,0)は有効サイズの設定済みを意味しない。単位は利用側で整合させる。
  XMFLOAT2 screenSize{};
  // 対象画像の幅・高さの逆数を格納する想定の値。既定(0,0)。自動計算やサイズ0での除算回避は行わない。
  XMFLOAT2 inverseScreenSize{};
  // 既定0の時間パラメーター。実時間・ゲーム時間のどちらかや、秒などの単位は接続先と利用側が決める。
  float time{};
  // 既定0の効果強度パラメーター。0が効果無効を意味するかも、この型だけでは保証しない。
  float intensity{};
  // 総サイズ32バイトを維持する末尾8バイトの余白。既定(0,0)。効果の入力値としては使わない。
  XMFLOAT2 padding{};
};

//========= 転送サイズ検証=========
// 既存の総サイズをコンパイル時に検証する。各フィールドのOffset・HLSL配置・Bind先まで検証するものではない。
// 条件不成立は実行時の失敗分岐ではなくコンパイル時のエラー。alignasや追加Paddingは導入しない。
static_assert(sizeof(CAMERA_CONSTANTS) == 64,"CAMERA_CONSTANTSは64バイトである必要があります。");
static_assert(sizeof(OBJECT_CONSTANTS) == 80,"OBJECT_CONSTANTSは80バイトである必要があります。");
static_assert(sizeof(MATERIAL_CONSTANTS) == 32,"MATERIAL_CONSTANTSは32バイトである必要があります。");
static_assert(sizeof(LIGHT_CONSTANTS) == 32,"LIGHT_CONSTANTSは32バイトである必要があります。");
static_assert(sizeof(POST_PROCESS_CONSTANTS) == 32,"POST_PROCESS_CONSTANTSは32バイトである必要があります。");

//========= 既存Renderer向け型エイリアス=========
// 旧参照名を維持する同じ型の別名。別構造体・変換処理・追加の保存領域は作らない。
using CameraConstants = CAMERA_CONSTANTS;
// Object転送型の既存名。Worldと色の転送配置は上の型と同一。
using ObjectConstants = OBJECT_CONSTANTS;
// Material転送型の既存名。未使用フィールドを含む同じ32バイトの型を参照する。
using MaterialConstants = MATERIAL_CONSTANTS;
// Light転送型の既存名。別名の存在も照明Shaderへの接続済みを意味しない。
using LightConstants = LIGHT_CONSTANTS;
// PostProcess転送型の既存名。対応する効果機能を新たに実装するものではない。
using PostProcessConstants = POST_PROCESS_CONSTANTS;