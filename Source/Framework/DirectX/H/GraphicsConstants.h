#pragma once

// Shaderへ転送するCamera・Object・Material・Light・PostProcessのCPU側データ構造を定義する。
// GPU Resourceの生成・更新・Bindは担当しない。フィールド順・型・サイズは対応HLSLと一致させる。
// LightとPostProcessの型が存在することは、それらの描画機能が実装済みであることを意味しない。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

//========= Camera転送データ=========
// 3D描画のCamera用定数バッファへ渡す行列。現在のRendererではb0に設定する。
struct CAMERA_CONSTANTS final
{
	// ViewとProjectionの合成行列。転送前のtransposeはRenderer側が担当する。
	XMFLOAT4X4 viewProjectionMatrix{};
};

//========= Object転送データ=========
// 3D描画のObject用定数バッファへ渡す変換と色。現在のRendererではb1に設定する。
struct OBJECT_CONSTANTS final
{
	// ObjectのWorld行列。転送前のtransposeはRenderer側が担当する。
	XMFLOAT4X4 worldMatrix{};
	// Objectへ適用するRGBA色。既定値は不透明な白。
	XMFLOAT4 color{1.0f,1.0f,1.0f,1.0f};
};

//========= Material転送データ=========
// 3D描画のMaterial用定数バッファに対応する配置。現在のRendererではb2に設定する。
struct MATERIAL_CONSTANTS final
{
	// Materialの基準RGBA色。現在のBasicColor ShaderではObject側のcolorを使用する。
	XMFLOAT4 baseColor{1.0f,1.0f,1.0f,1.0f};
	// UV座標へ掛けるタイリング倍率。既定値は縦横1倍。
	XMFLOAT2 uvTiling{1.0f,1.0f};
	// Texture使用フラグの転送値。現在のPixel Shaderでは0.5未満を未使用とする。
	float useTexture{};
	// HLSLとの配置と32バイトの構造体サイズを維持する余白。
	float padding{};
};

//========= Light転送データ=========
// 平行光源用のCPUデータ構造。対応Shaderへの接続は別途実装・照合する。
struct LIGHT_CONSTANTS final
{
	// 光の方向。既定値は下向き。必要な正規化は利用側が担当する。
	XMFLOAT3 direction{0.0f,-1.0f,0.0f};
	// 光の強さ。既定値は1。
	float intensity{1.0f};
	// 光のRGB色。既定値は白。
	XMFLOAT3 color{1.0f,1.0f,1.0f};
	// 環境光の強さ。既定値は0.1。
	float ambientIntensity{0.1f};
};

//========= PostProcess転送データ=========
// ポスト処理用のCPUデータ構造。対応Shaderへの接続は別途実装・照合する。
struct POST_PROCESS_CONSTANTS final
{
	// 処理対象画像の幅と高さ。
	XMFLOAT2 screenSize{};
	// 処理対象画像の幅と高さの逆数。値の計算は利用側が担当する。
	XMFLOAT2 inverseScreenSize{};
	// 利用側が設定する時間パラメーター。解釈と単位は対応Shaderとの契約で決める。
	float time{};
	// 利用側が設定する効果の強さ。
	float intensity{};
	// 32バイトの構造体サイズを維持する余白。
	XMFLOAT2 padding{};
};

//========= 転送サイズ検証=========
// 既存の型サイズを検証する。サイズ一致だけでHLSLのフィールド配置やBind先の正しさは保証しない。
static_assert(sizeof(CAMERA_CONSTANTS) == 64,"CAMERA_CONSTANTSは64バイトである必要があります。");
static_assert(sizeof(OBJECT_CONSTANTS) == 80,"OBJECT_CONSTANTSは80バイトである必要があります。");
static_assert(sizeof(MATERIAL_CONSTANTS) == 32,"MATERIAL_CONSTANTSは32バイトである必要があります。");
static_assert(sizeof(LIGHT_CONSTANTS) == 32,"LIGHT_CONSTANTSは32バイトである必要があります。");
static_assert(sizeof(POST_PROCESS_CONSTANTS) == 32,"POST_PROCESS_CONSTANTSは32バイトである必要があります。");

//========= 既存Renderer向け型エイリアス=========
// 既存RendererのCamera転送型名を維持する。同じ型の別名であり、データは追加しない。
using CameraConstants = CAMERA_CONSTANTS;
// 既存RendererのObject転送型名を維持する。
using ObjectConstants = OBJECT_CONSTANTS;
// 既存RendererのMaterial転送型名を維持する。
using MaterialConstants = MATERIAL_CONSTANTS;
// 既存のLight転送型名を維持する。
using LightConstants = LIGHT_CONSTANTS;
// 既存のPostProcess転送型名を維持する。
using PostProcessConstants = POST_PROCESS_CONSTANTS;