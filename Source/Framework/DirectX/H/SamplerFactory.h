#pragma once

// 画像の参照に使うSampler Stateを、Linear補間で生成する共通の補助を宣言する。
// 生成に失敗した時は、API名とHRESULTをログへ記録して失敗を返す。失敗後のResourceの解放・再試行は担当しない。
// 生成したSamplerの所有は、呼出側の出力先ComPtrが持つ。Deviceは借用し、保持しない。

//========= 外部ライブラリ インクルード=========
#include <d3d11.h> // DeviceとSampler Stateの型、アドレス指定の型を使用する。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

// Sampler Stateの生成を提供する。
class SamplerFactory final
{
public:
	//========= 生成関数=========
	// 縮小・拡大・Mip間をLinear補間し、U・V・Wを同じアドレス指定にするSamplerを生成して、出力先へ所有参照を受け取る。
	// LODは0〜FLOAT32_MAXを許可し、実際のMipは画像Resourceに従う。その他の設定は、ゼロ初期値のまま。Mipの自動生成の指定ではない。
	// device: 生成に使うDevice。nullptr不可。
	// addressMode: U・V・Wに共通のアドレス指定。画像の端を繰り返すならD3D11_TEXTURE_ADDRESS_WRAP、繰り返さないならD3D11_TEXTURE_ADDRESS_CLAMP。
	// output: 所有参照の出力先。呼出前に空であること。失敗時は、空のまま、またはCreateSamplerStateが設定した値のまま。
	// 戻り値: 成功したらtrue。失敗はAPI名（ID3D11Device::CreateSamplerState）とHRESULTをログへ記録してfalse。
	[[nodiscard]] static bool CreateLinear(ID3D11Device* device,D3D11_TEXTURE_ADDRESS_MODE addressMode,ComPtr<ID3D11SamplerState>& output);

private:
	//========= 生成禁止関数=========
	// SamplerFactoryのインスタンス生成を禁止する。
	SamplerFactory() = delete;
};