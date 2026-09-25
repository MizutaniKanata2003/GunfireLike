#include "../H/TextureLoader.h"

//========= C++標準ライブラリ インクルード=========
#include <string>
#include <vector>

//========= Windows インクルード=========
#include <Windows.h>
#include <wincodec.h>

//========= Framework インクルード=========
#include "../../Etc/H/Logger.h"

namespace
{
	// BGRA形式の画像データにおける1ピクセルのバイト数。
	constexpr UINT BYTES_PER_PIXEL = 4;

	// HRESULTを16進数の表示用文字列へ変換する。
	std::wstring GetHRESULTText( HRESULT result )
	{
		std::wstring resultText{ L"0x" };
		constexpr wchar_t hexDigits[]{ L"0123456789ABCDEF" };
		const unsigned long resultValue = static_cast<unsigned long>( result );

		for ( int digitIndex = 7; digitIndex >= 0; --digitIndex )
		{
			const unsigned long digit = ( resultValue >> digitIndex * 4 ) & 0x0f;
			resultText += hexDigits[ digit ];
		}

		return resultText;
	}

	// AssetカテゴリでTexture読込失敗を出力する。
	void WriteTextureAssetError( const wchar_t* functionName, std::wstring_view filePath, HRESULT result )
	{
		std::wstring message{ functionName };
		message += L" に失敗しました。Texture: ";
		message += filePath;
		message += L" HRESULT: ";
		message += GetHRESULTText( result );

		Logger::Write( e_LogLevel::e_ERROR, e_LogCategory::e_ASSET, message );
	}

	// GraphicsカテゴリでTexture GPU Resource生成失敗を出力する。
	void WriteTextureGraphicsError( const wchar_t* functionName, std::wstring_view filePath, HRESULT result )
	{
		std::wstring message{ functionName };
		message += L" に失敗しました。Texture: ";
		message += filePath;
		message += L" HRESULT: ";
		message += GetHRESULTText( result );

		Logger::Write( e_LogLevel::e_ERROR, e_LogCategory::e_GRAPHICS, message );
	}
}

// 指定した画像ファイルを読み込み、Shader Resource Viewを生成する。
bool TextureLoader::LoadWicTexture(
	ID3D11Device* device,
	std::wstring_view filePath,
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& textureView )
{
	textureView.Reset();

	if ( device == nullptr )
	{
		Logger::Write( e_LogLevel::e_ERROR, e_LogCategory::e_GRAPHICS, L"TextureLoaderにDeviceが渡されていません。" );
		return false;
	}

	if ( filePath.empty() )
	{
		Logger::Write( e_LogLevel::e_ERROR, e_LogCategory::e_ASSET, L"Textureファイルパスが空です。" );
		return false;
	}

	const std::wstring filePathText{ filePath };

	// WICを使用して画像をデコードするためのCOMオブジェクト。
	Microsoft::WRL::ComPtr<IWICImagingFactory> wicFactory{};
	Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder{};
	Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame{};
	Microsoft::WRL::ComPtr<IWICFormatConverter> converter{};

	const HRESULT factoryResult = CoCreateInstance(
		CLSID_WICImagingFactory,
		nullptr,
		CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS( wicFactory.GetAddressOf() ) );

	if ( FAILED( factoryResult ) )
	{
		WriteTextureAssetError( L"CoCreateInstance(IWICImagingFactory)", filePath, factoryResult );
		return false;
	}

	const HRESULT decoderResult = wicFactory->CreateDecoderFromFilename(
		filePathText.c_str(),
		nullptr,
		GENERIC_READ,
		WICDecodeMetadataCacheOnLoad,
		decoder.GetAddressOf() );

	if ( FAILED( decoderResult ) )
	{
		WriteTextureAssetError( L"IWICImagingFactory::CreateDecoderFromFilename", filePath, decoderResult );
		return false;
	}

	const HRESULT frameResult = decoder->GetFrame( 0, frame.GetAddressOf() );

	if ( FAILED( frameResult ) )
	{
		WriteTextureAssetError( L"IWICBitmapDecoder::GetFrame", filePath, frameResult );
		return false;
	}

	// デコードした画像サイズとBGRA変換後のピクセルデータを取得する。
	UINT width{};
	UINT height{};

	const HRESULT imageSizeResult = frame->GetSize( &width, &height );

	if ( FAILED( imageSizeResult ) )
	{
		WriteTextureAssetError( L"IWICBitmapFrameDecode::GetSize", filePath, imageSizeResult );
		return false;
	}

	if ( width == 0 || height == 0 )
	{
		Logger::Write( e_LogLevel::e_ERROR, e_LogCategory::e_ASSET, L"Textureの幅または高さが0です。" );
		return false;
	}

	const HRESULT converterResult = wicFactory->CreateFormatConverter( converter.GetAddressOf() );

	if ( FAILED( converterResult ) )
	{
		WriteTextureAssetError( L"IWICImagingFactory::CreateFormatConverter", filePath, converterResult );
		return false;
	}

	const HRESULT converterInitializeResult = converter->Initialize(
		frame.Get(),
		GUID_WICPixelFormat32bppBGRA,
		WICBitmapDitherTypeNone,
		nullptr,
		0.0,
		WICBitmapPaletteTypeCustom );

	if ( FAILED( converterInitializeResult ) )
	{
		WriteTextureAssetError( L"IWICFormatConverter::Initialize", filePath, converterInitializeResult );
		return false;
	}

	const UINT rowPitch = width * BYTES_PER_PIXEL;
	const UINT imageSize = rowPitch * height;
	std::vector<unsigned char> pixels( imageSize );

	const HRESULT copyPixelsResult = converter->CopyPixels( nullptr, rowPitch, imageSize, pixels.data() );

	if ( FAILED( copyPixelsResult ) )
	{
		WriteTextureAssetError( L"IWICBitmapSource::CopyPixels", filePath, copyPixelsResult );
		return false;
	}

	// Direct3DのTextureとShader Resource Viewを生成する。
	D3D11_TEXTURE2D_DESC textureDescription{};
	textureDescription.Width = width;
	textureDescription.Height = height;
	textureDescription.MipLevels = 1;
	textureDescription.ArraySize = 1;
	textureDescription.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	textureDescription.SampleDesc.Count = 1;
	textureDescription.Usage = D3D11_USAGE_DEFAULT;
	textureDescription.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA textureData{};
	textureData.pSysMem = pixels.data();
	textureData.SysMemPitch = rowPitch;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> texture{};

	const HRESULT textureResult = device->CreateTexture2D(
		&textureDescription,
		&textureData,
		texture.GetAddressOf() );

	if ( FAILED( textureResult ) )
	{
		WriteTextureGraphicsError( L"ID3D11Device::CreateTexture2D", filePath, textureResult );
		return false;
	}

	const HRESULT textureViewResult = device->CreateShaderResourceView(
		texture.Get(),
		nullptr,
		textureView.GetAddressOf() );

	if ( FAILED( textureViewResult ) )
	{
		WriteTextureGraphicsError( L"ID3D11Device::CreateShaderResourceView", filePath, textureViewResult );
		textureView.Reset();
		return false;
	}

	return true;
}