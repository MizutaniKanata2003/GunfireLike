#include "../H/TextureLoader.h"

// WICのデコード結果をCPU側の連続BGRA配列へ取り出し、D3D11 TextureとSRVを生成する。
// COM初期化、ShaderへのBind、Sampler、Mip生成、色空間変換の追加処理は担当しない。

//========= Windows インクルード=========
#include <Windows.h>
#include <wincodec.h>

//========= C++標準ライブラリ インクルード=========
#include <string>
#include <vector>

//========= Framework インクルード=========
#include "Framework/Etc/H/Logger.h"

// 32bit BGRAのバイト幅と、Asset/GPU生成の失敗を区別するログ補助処理をまとめる。
namespace
{
	//========= ピクセル形式定数=========
	// 変換先32bit BGRAは各成分8bitで4バイト。row_pitchとimage_sizeを同じ形式で計算する。
	constexpr UINT ns_BYTES_PER_PIXEL = 4;

	//========= ログ補助関数=========
	// HRESULTを0x付き8桁の大文字16進数へ変換して返す。
	// result: 失敗ログへ記録するHRESULT。
	std::wstring GetHRESULTText(HRESULT result)
	{
		// unsigned表現を4bitずつ上位から取り出す。符号付きの10進表示へ変えず、既存の8桁表記を維持する。
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
	// functionName: 失敗したAPIを示す有効な終端付きワイド文字列。nullptr不可。
	// filePath: 対象画像のパス。参照は保持せず、ログ文字列へ内容を連結する。
	// result: 画像の取得・デコード失敗を示すHRESULT。
	void WriteTextureAssetError(const wchar_t* functionName,std::wstring_view filePath,HRESULT result)
	{
		// API名、画像パス、HRESULTを一つのメッセージへ組み立て、失敗した処理と対象を追跡できるようにする。
		std::wstring message{functionName};
		message += L" に失敗しました。Texture: ";
		message += filePath;
		message += L" HRESULT: ";
		message += GetHRESULTText(result);

		// Asset失敗と描画Resource失敗の分類を維持する。ログ用メモリ確保の例外捕捉は追加しない。
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_ASSET,message);
	}

	// GraphicsカテゴリでTexture GPU Resource生成失敗を出力する。
	// functionName: 失敗したAPIを示す有効な終端付きワイド文字列。nullptr不可。
	// filePath: 対象画像のパス。参照は保持せず、ログ文字列へ内容を連結する。
	// result: GPU Resourceの生成失敗を示すHRESULT。
	void WriteTextureGraphicsError(const wchar_t* functionName,std::wstring_view filePath,HRESULT result)
	{
		// API名、画像パス、HRESULTを一つのメッセージへ組み立て、失敗した処理と対象を追跡できるようにする。
		std::wstring message{functionName};
		message += L" に失敗しました。Texture: ";
		message += filePath;
		message += L" HRESULT: ";
		message += GetHRESULTText(result);

		// Asset失敗と描画Resource失敗の分類を維持する。ログ用メモリ確保の例外捕捉は追加しない。
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_GRAPHICS,message);
	}
}

//========= Texture読込関数=========
// 画像の先頭フレームからBGRAデータを取り出し、単一Mip・単一画像のTextureをSRVとして返す。
bool TextureLoader::LoadWicTexture(ID3D11Device* device,std::wstring_view filePath,ComPtr<ID3D11ShaderResourceView>& textureView)
{
	// 引数検証より先に呼出側の旧SRV所有参照を解放する。失敗時に旧画像を保持する設計ではない。
	// Contextなどが別に保持する参照のUnbindや、GPU処理の完了待機はこのResetでは行わない。
	textureView.Reset();

	// Deviceは非所有で借りる。未指定はGraphicsの失敗として記録し、画像処理へ進まない。
	if(device == nullptr){
		// Asset失敗と描画Resource失敗の分類を維持する。ログ用メモリ確保の例外捕捉は追加しない。
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_GRAPHICS,L"TextureLoaderにDeviceが渡されていません。");
		return false;
	}

	// 空パスをAssetの失敗として扱う。このLoader単体では空パスを正常な単色指定とは解釈しない。
	if(filePath.empty()){
		// Asset失敗と描画Resource失敗の分類を維持する。ログ用メモリ確保の例外捕捉は追加しない。
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_ASSET,L"Textureファイルパスが空です。");
		return false;
	}

	// wstring_viewは終端を保証しないため、WICのFilename APIへ渡せる終端付き文字列をローカルに作る。
	// 埋込NULLの検査やパスの正規化は追加しない。相対パスは呼出時の作業ディレクトリに従う。
	const std::wstring file_path_text{filePath};

	// WICのFactory→Decoder→Frame→Converterという処理用オブジェクトを、一時ComPtrで所有する。
	// 各returnや例外でローカル所有参照は自動解放される。呼出スレッドのCOM初期化・終了は呼出側の責務。
	// Device/Contextを作る処理ではなく、まだGPU Textureも生成していない段階。
	ComPtr<IWICImagingFactory> WIC_factory{};
	ComPtr<IWICBitmapDecoder> decoder{};
	ComPtr<IWICBitmapFrameDecode> frame{};
	ComPtr<IWICFormatConverter> converter{};

	// CLSIDでWIC Factoryをプロセス内COMオブジェクトとして生成する。集約用outerはnullptr。
	// IID_PPV_ARGSで要求Interfaceと出力先の型を対応付ける。失敗時は出力SRVを空のまま返す。
	const HRESULT factory_result = CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(WIC_factory.GetAddressOf()));
	if(FAILED(factory_result)){
		WriteTextureAssetError(L"CoCreateInstance(IWICImagingFactory)",filePath,factory_result);
		return false;
	}

	// 有効なファイル名を読み取り専用で開く。Vendor GUIDは指定せず、利用可能なWIC Decoderの選択へ任せる。
	// CacheOnLoadはメタデータのキャッシュ方針。ここだけで全画素のCPU配列を確保・取得する処理ではない。
	const HRESULT decoder_result = WIC_factory->CreateDecoderFromFilename(file_path_text.c_str(),nullptr,GENERIC_READ,
																																				WICDecodeMetadataCacheOnLoad,decoder.GetAddressOf());

	if(FAILED(decoder_result)){
		WriteTextureAssetError(L"IWICImagingFactory::CreateDecoderFromFilename",filePath,decoder_result);
		return false;
	}

	// Frame Index 0で先頭フレームだけを取得する。複数フレームの列挙やアニメーション再生は行わない。
	const HRESULT frame_result = decoder->GetFrame(0,frame.GetAddressOf());
	if(FAILED(frame_result)){
		WriteTextureAssetError(L"IWICBitmapDecoder::GetFrame",filePath,frame_result);
		return false;
	}

	// 幅と高さはpixel単位。GetSizeの成否を確認し、0の寸法では後段のメモリ確保・Texture生成へ進まない。
	UINT width{};
	UINT height{};

	const HRESULT image_size_result = frame->GetSize(&width,&height);
	if(FAILED(image_size_result)){
		WriteTextureAssetError(L"IWICBitmapFrameDecode::GetSize",filePath,image_size_result);
		return false;
	}

	if(width == 0 || height == 0){
		// Asset失敗と描画Resource失敗の分類を維持する。ログ用メモリ確保の例外捕捉は追加しない。
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_ASSET,L"Textureの幅または高さが0です。");
		return false;
	}

	// 元画像の形式をそのままD3Dへ渡すのではなく、固定の32bit BGRAへ揃える変換器を生成する。
	// CanConvert等の新しい事前判定は追加せず、既存どおりInitializeのHRESULTで判定する。
	const HRESULT converter_result = WIC_factory->CreateFormatConverter(converter.GetAddressOf());
	if(FAILED(converter_result)){
		WriteTextureAssetError(L"IWICImagingFactory::CreateFormatConverter",filePath,converter_result);
		return false;
	}

	// 変換先はGUID_WICPixelFormat32bppBGRA。GPU側のB8G8R8A8形式と、後段の4バイト/pixel計算へ合わせる。
	// Ditherなし・Paletteなし・Alpha閾値0・Custom Paletteという、ここでPalette変換を追加しない既存設定を維持する。
	// このAlpha閾値はPalette等への変換用の引数で、画像全体のAlphaを0へ設定する指定ではない。
	const HRESULT converter_initialize_result = converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,
																																		nullptr,0.0,WICBitmapPaletteTypeCustom);

	if(FAILED(converter_initialize_result)){
		WriteTextureAssetError(L"IWICFormatConverter::Initialize",filePath,converter_initialize_result);
		return false;
	}

	// row_pitchは隣接する行の先頭間隔をバイト数で表す。32bit BGRAなのでwidth*4で連続配置する。
	// image_sizeはその行間隔*heightの総バイト数。両方とも既存のUINT計算で、乗算のオーバーフロー検証は行わない。
	// pixelsの確保失敗などのC++例外は捕捉せず伝播する。偽の成功や旧SRVの復元には切り替えない。
	const UINT row_pitch = width * ns_BYTES_PER_PIXEL;
	const UINT image_size = row_pitch * height;
	std::vector<unsigned char> pixels(image_size);

	// ROIがnullptrなので画像全体を取り出す。CopyPixelsのStrideと総容量はpixel数ではなくバイト数で渡す。
	// Converterの出力を呼出側が所有するCPU配列へ書き込む段階であり、GPU Textureへ直接転送するAPIではない。
	// この後のD3D初期データにも同じrow_pitchを使い、WIC側とGPU側で行の解釈を揃える。
	const HRESULT copy_pixels_result = converter->CopyPixels(nullptr,row_pitch,image_size,pixels.data());
	if(FAILED(copy_pixels_result)){
		WriteTextureAssetError(L"IWICBitmapSource::CopyPixels",filePath,copy_pixels_result);
		return false;
	}

	// D3D11の2D Texture記述を作る。Width/Heightはpixel、MipLevels=1は元サイズの1段、ArraySize=1は1画像。
	// B8G8R8A8_UNORMはBGRA各8bitの正規化形式。UNORM_SRGBへの変更や明示的な色空間処理は追加しない。
	// SampleDesc.Count=1は非MSAA。ゼロ初期化によりQuality=0、CPUAccessFlags=0、MiscFlags=0を維持する。
	// Usage=DEFAULT、BindFlags=SHADER_RESOURCEで、Shader参照用のGPU Resourceとして生成する。
	// 自動Mip生成・Render Target用途・CPUからの直接Mapを有効にする設定は追加しない。
	D3D11_TEXTURE2D_DESC texture_description{};
	texture_description.Width = width;
	texture_description.Height = height;
	texture_description.MipLevels = 1;
	texture_description.ArraySize = 1;
	texture_description.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	texture_description.SampleDesc.Count = 1;
	texture_description.Usage = D3D11_USAGE_DEFAULT;
	texture_description.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	// Mip 1段×Array 1画像なので、初期データ記述は1個を渡す。
	// pSysMemは変換済みBGRA配列、SysMemPitchは1行のバイト間隔。2D画像で不要なSlicePitchは0のまま。
	// CPU配列の所有権をDeviceへ移す処理ではない。CreateTexture2D呼出中は配列を有効に保つ。
	D3D11_SUBRESOURCE_DATA texture_data{};
	texture_data.pSysMem = pixels.data();
	texture_data.SysMemPitch = row_pitch;

	// 一時Textureの所有参照を保持する。SRV生成に失敗してもローカルComPtrがこの参照を解放する。
	ComPtr<ID3D11Texture2D> texture{};

	const HRESULT texture_result = device->CreateTexture2D(&texture_description,&texture_data,texture.GetAddressOf());
	if(FAILED(texture_result)){
		WriteTextureGraphicsError(L"ID3D11Device::CreateTexture2D",filePath,texture_result);
		return false;
	}

	// Texture本体とは別に、Shaderが読むためのViewを生成する。desc=nullptrでResourceに基づく既定Viewを要求する。
	// 成功したViewは参照先Resourceの寿命も保持するため、関数終了でローカルTextureの所有参照を解放できる。
	// 出力ComPtrへ所有参照を渡すだけで、PS t0へのBindやSampler設定はここでは行わない。
	const HRESULT texture_view_result = device->CreateShaderResourceView(texture.Get(),nullptr,textureView.GetAddressOf());
	if(FAILED(texture_view_result)){
		WriteTextureGraphicsError(L"ID3D11Device::CreateShaderResourceView",filePath,texture_view_result);
		// この検出失敗経路では、出力側も明示的に空へ戻してfalseを返す。
		textureView.Reset();
		return false;
	}

	// 出力SRVだけを呼出側へ残す。CPU画素配列と一時WIC/Texture参照はローカルの寿命に従って解放される。
	return true;
}