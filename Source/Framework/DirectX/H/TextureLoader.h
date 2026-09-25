#pragma once

//========= C++標準ライブラリ インクルード=========
#include <string_view>

//========= DirectX インクルード=========
#include <d3d11.h>
#include <wrl/client.h>

// WIC対応画像ファイルからTextureとShader Resource Viewを生成する共通Loader。
class TextureLoader final
{
public:
	//========= Texture読込関数=========
	// 指定した画像ファイルを読み込み、Shader Resource Viewを生成する。
	static bool LoadWicTexture( ID3D11Device* device, std::wstring_view filePath, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& textureView );

private:
	//========= 生成禁止関数=========
	// TextureLoaderのインスタンス生成を禁止する。
	TextureLoader() = delete;
};