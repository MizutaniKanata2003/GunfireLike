#include "../H/TextureLoader.h"

// WICによる画像デコードと、Texture・Shader Resource Viewの生成を実装する。

//========= Windows インクルード=========
#include <Windows.h>
#include <wincodec.h>

//========= C++標準ライブラリ インクルード=========
#include <string>
#include <vector>

//========= Framework インクルード=========
#include "Framework/Etc/H/Logger.h"

// 画像形式の定数と、HRESULT・失敗ログの内部用処理をまとめる。
namespace
{
	//========= ピクセル形式定数=========
	// BGRA形式の画像データにおける1ピクセルのバイト数。
	constexpr UINT ns_BYTES_PER_PIXEL = 4;

	//========= ログ補助関数=========
	// HRESULTを0x付き8桁の16進数文字列へ変換して返す。
	// result: 失敗ログへ表示するHRESULT。
	std::wstring GetHRESULTText(HRESULT result)
	{
		std::wstring result_text{L"0x"};
		constexpr wchar_t HEX_DIGITS[]{L"0123456789ABCDEF"};
		const unsigned long result_value = static_cast<unsigned long>(result);

		for(int digit_index = 7; digit_index >= 0; --digit_index){
			const unsigned long digit = (result_value >> digit_index * 4) & 0x0f;
			result_text += HEX_DIGITS[digit];
		}

		return result_text;
	}

	// AssetカテゴリでTexture読込失敗を出力する。
	// functionName: 失敗した処理の終端付きワイド文字列。nullptr不可。
	// filePath: ログへ含める画像パス。参照は保持しない。
	// result: 失敗理由を示すHRESULT。
	void WriteTextureAssetError(const wchar_t* functionName,std::wstring_view filePath,HRESULT result)
	{
		std::wstring message{functionName};
		message += L" に失敗しました。Texture: ";
		message += filePath;
		message += L" HRESULT: ";
		message += GetHRESULTText(result);

		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_ASSET,message);
	}

	// GraphicsカテゴリでTexture GPU Resource生成失敗を出力する。
	// functionName: 失敗した処理の終端付きワイド文字列。nullptr不可。
	// filePath: ログへ含める画像パス。参照は保持しない。
	// result: 失敗理由を示すHRESULT。
	void WriteTextureGraphicsError(const wchar_t* functionName,std::wstring_view filePath,HRESULT result)
	{
		std::wstring message{functionName};
		message += L" に失敗しました。Texture: ";
		message += filePath;
		message += L" HRESULT: ";
		message += GetHRESULTText(result);

		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_GRAPHICS,message);
	}
}

//========= Texture読込関数=========
// 先頭フレームをBGRAへ変換して、単一MipのTextureとShader Resource Viewを生成する。
bool TextureLoader::LoadWicTexture(ID3D11Device* device,std::wstring_view filePath,ComPtr<ID3D11ShaderResourceView>& textureView)
{
	textureView.Reset();

	if(device == nullptr){
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_GRAPHICS,L"TextureLoaderにDeviceが渡されていません。");
		return false;
	}

	if(filePath.empty()){
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_ASSET,L"Textureファイルパスが空です。");
		return false;
	}

	const std::wstring file_path_text{filePath};

	// WIC用のCOM初期化は呼出側の責務とし、この関数では開始・終了しない。
	// 一時COMオブジェクトは各ComPtrが所有し、関数終了時に解放する。
	ComPtr<IWICImagingFactory> WIC_factory{};
	ComPtr<IWICBitmapDecoder> decoder{};
	ComPtr<IWICBitmapFrameDecode> frame{};
	ComPtr<IWICFormatConverter> converter{};

	const HRESULT factory_result = CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(WIC_factory.GetAddressOf()));
	if(FAILED(factory_result)){
		WriteTextureAssetError(L"CoCreateInstance(IWICImagingFactory)",filePath,factory_result);
		return false;
	}

	const HRESULT decoder_result = WIC_factory->CreateDecoderFromFilename(file_path_text.c_str(),nullptr,GENERIC_READ,
																																				WICDecodeMetadataCacheOnLoad,decoder.GetAddressOf());

	if(FAILED(decoder_result)){
		WriteTextureAssetError(L"IWICImagingFactory::CreateDecoderFromFilename",filePath,decoder_result);
		return false;
	}

	const HRESULT frame_result = decoder->GetFrame(0,frame.GetAddressOf());
	if(FAILED(frame_result)){
		WriteTextureAssetError(L"IWICBitmapDecoder::GetFrame",filePath,frame_result);
		return false;
	}

	// デコードした画像サイズとBGRA変換後のピクセルデータを取得する。
	UINT width{};
	UINT height{};

	const HRESULT image_size_result = frame->GetSize(&width,&height);
	if(FAILED(image_size_result)){
		WriteTextureAssetError(L"IWICBitmapFrameDecode::GetSize",filePath,image_size_result);
		return false;
	}

	if(width == 0 || height == 0){
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_ASSET,L"Textureの幅または高さが0です。");
		return false;
	}

	const HRESULT converter_result = WIC_factory->CreateFormatConverter(converter.GetAddressOf());
	if(FAILED(converter_result)){
		WriteTextureAssetError(L"IWICImagingFactory::CreateFormatConverter",filePath,converter_result);
		return false;
	}

	const HRESULT converter_initialize_result = converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,
																																		nullptr,0.0,WICBitmapPaletteTypeCustom);

	if(FAILED(converter_initialize_result)){
		WriteTextureAssetError(L"IWICFormatConverter::Initialize",filePath,converter_initialize_result);
		return false;
	}

	// 既存のUINT計算を維持する。巨大画像に対する乗算のオーバーフロー検証は行わない。
	const UINT row_pitch = width * ns_BYTES_PER_PIXEL;
	const UINT image_size = row_pitch * height;
	std::vector<unsigned char> pixels(image_size);

	const HRESULT copy_pixels_result = converter->CopyPixels(nullptr,row_pitch,image_size,pixels.data());
	if(FAILED(copy_pixels_result)){
		WriteTextureAssetError(L"IWICBitmapSource::CopyPixels",filePath,copy_pixels_result);
		return false;
	}

	// Direct3DのTextureとShader Resource Viewを生成する。
	D3D11_TEXTURE2D_DESC texture_description{};
	texture_description.Width = width;
	texture_description.Height = height;
	texture_description.MipLevels = 1;
	texture_description.ArraySize = 1;
	texture_description.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	texture_description.SampleDesc.Count = 1;
	texture_description.Usage = D3D11_USAGE_DEFAULT;
	texture_description.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA texture_data{};
	texture_data.pSysMem = pixels.data();
	texture_data.SysMemPitch = row_pitch;

	ComPtr<ID3D11Texture2D> texture{};

	const HRESULT texture_result = device->CreateTexture2D(&texture_description,&texture_data,texture.GetAddressOf());
	if(FAILED(texture_result)){
		WriteTextureGraphicsError(L"ID3D11Device::CreateTexture2D",filePath,texture_result);
		return false;
	}

	const HRESULT texture_view_result = device->CreateShaderResourceView(texture.Get(),nullptr,textureView.GetAddressOf());
	if(FAILED(texture_view_result)){
		WriteTextureGraphicsError(L"ID3D11Device::CreateShaderResourceView",filePath,texture_view_result);
		textureView.Reset();
		return false;
	}

	return true;
}