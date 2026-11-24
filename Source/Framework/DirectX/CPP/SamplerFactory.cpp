#include "../H/SamplerFactory.h"

// Linear補間のSampler Stateの生成と、失敗時のログの記録を実装する。
// 設定値は、各Rendererが個別に持っていた設定（Linear補間・LOD 0〜FLOAT32_MAX・U/V/W共通のアドレス指定）と同じ。

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsErrorLog.h"

//========= 生成関数=========
// Linear補間・指定したアドレス指定のSamplerを生成する。
bool SamplerFactory::CreateLinear(ID3D11Device* device,D3D11_TEXTURE_ADDRESS_MODE addressMode,ComPtr<ID3D11SamplerState>& output)
{
	// 縮小・拡大・Mip間のLinear参照と、U/V/Wの共通のアドレス指定を設定する。許可するLODは0〜FLOAT32_MAXで、その他はゼロ初期値のまま。
	D3D11_SAMPLER_DESC sampler_description{};
	sampler_description.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampler_description.AddressU = addressMode;
	sampler_description.AddressV = addressMode;
	sampler_description.AddressW = addressMode;
	sampler_description.MinLOD = 0.0f;
	sampler_description.MaxLOD = D3D11_FLOAT32_MAX;
	const HRESULT sampler_state_result = device->CreateSamplerState(&sampler_description,output.GetAddressOf());

	// Sampler生成失敗ならAPI名とHRESULTを記録して失敗を返す。生成済みResourceの解放は、呼出側が行う。
	if(FAILED(sampler_state_result)){
		GraphicsErrorLog::Write(L"ID3D11Device::CreateSamplerState",sampler_state_result);
		return false;
	}
	return true;
}