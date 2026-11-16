#include "../H/TextureLoader.h"

// WIC Factory→Decoder→Frame→Converter→CPU画素配列→GPU Texture→SRVの順で、画像を描画用Resourceへ変換する。
// 既存の形式・設定・条件・ログ・出力Reset順を維持する。COM初期化、Shader Bind、Sampler設定は呼出側の責務。

//========= Windows インクルード=========
#include <Windows.h> // HRESULT判定、CoCreateInstance、プロセス内COM生成や読取アクセスの指定に使用する。
#include <wincodec.h> // WICのFactory・Decoder・Frame・Converter、変換先BGRA形式とデコード設定を使用する。

//========= C++標準ライブラリ インクルード=========
#include <string> // std::wstringで終端付きファイル名と、API名・画像パス・HRESULTを含むログを組み立てる。
#include <vector> // std::vector<unsigned char>で変換済みBGRA画素の連続したCPU領域を所有する。

//========= Framework インクルード=========
#include "Framework/Etc/H/Logger.h"

// BGRA形式のバイト幅と、画像処理・GPU生成の失敗を区別するログ補助処理をまとめる。
namespace
{
	//========= ピクセル形式定数=========
	// BGRAの各8bit成分を4個持つため1pixelは4バイト。WIC出力形式、行間隔、D3D Formatを対応させる。
	constexpr UINT ns_BYTES_PER_PIXEL = 4;

	//========= ログ補助関数=========
	// HRESULTを0x付き大文字16進数8桁へ変換する。エラーの検出やResource解放は呼出側で行う。
	// result: 表示対象のHRESULT。unsigned表現で各桁を抽出する。
	std::wstring GetHRESULTText(HRESULT result)
	{
		// 先頭表記と数字表を用意する。4bitの値0〜15を数字・A〜Fへ対応させる。
		std::wstring result_text{L"0x"};
		constexpr wchar_t HEX_DIGITS[]{L"0123456789ABCDEF"};
		const unsigned long result_value = static_cast<unsigned long>(result);

		// 桁番号7から0へ逆順走査し、上位から8桁を出力する。先頭の0も省略しない。
		for(int digit_index = 7; digit_index >= 0; --digit_index){
			// 1桁4bitなので対象桁を下位へ移動し、0x0fでその4bitだけを抽出する。
			const unsigned long digit = (result_value >> digit_index * 4) & 0x0f;
			result_text += HEX_DIGITS[digit]; // 抽出値を対応文字へ置換し、表示文字列へ加える。
		}
		return result_text;
	}

	// 画像ファイルの取得・デコード失敗をAsset分類へ出力する。
	// functionName: 失敗APIを示す有効な終端付きワイド文字列。nullptr不可。
	// filePath: 対象画像のパス。呼出中に読み取り、参照は保持しない。
	// result: WIC等で発生した失敗のHRESULT。
	void WriteTextureAssetError(const wchar_t* functionName,std::wstring_view filePath,HRESULT result)
	{
		// API名・画像パス・失敗値を同じログへ連結する。viewは長さ付きで内容をコピーする。
		std::wstring message{functionName};
		message += L" に失敗しました。Texture: ";
		message += filePath;
		message += L" HRESULT: ";
		message += GetHRESULTText(result);

		// ログ生成の例外は捕捉しない。Resource解放や出力SRVのResetは、この関数では行わない。
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_ASSET,message);
	}

	// GPU Texture・SRV生成失敗をGraphics分類へ出力する。
	// functionName: 失敗APIを示す有効な終端付きワイド文字列。nullptr不可。
	// filePath: 対象画像のパス。呼出中に読み取り、参照は保持しない。
	// result: GPU Resource生成で発生した失敗のHRESULT。
	void WriteTextureGraphicsError(const wchar_t* functionName,std::wstring_view filePath,HRESULT result)
	{
		// 画像処理側と同じ表示形式を使い、分類だけでなくAPI名でも失敗段階を追えるようにする。
		std::wstring message{functionName};
		message += L" に失敗しました。Texture: ";
		message += filePath;
		message += L" HRESULT: ";
		message += GetHRESULTText(result);

		// Graphics分類で記録する。メモリ確保等の例外捕捉やResource復旧は追加しない。
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_GRAPHICS,message);
	}
}

//========= Texture読込関数=========
// WICで先頭フレームをBGRAへ揃えてからTextureとSRVを生成し、成功したViewだけを出力先へ残す。
bool TextureLoader::LoadWicTexture(ID3D11Device* device,std::wstring_view filePath,ComPtr<ID3D11ShaderResourceView>& textureView)
{
	// 引数検証より先に旧SRVの所有参照を解放する。失敗しても旧画像へ戻さない。
	// Context等が別に保持する参照のUnbindや、GPU完了待機を行うResetではない。
	textureView.Reset();

	// Device未指定なら画像処理へ進まず、Graphics分類へ記録して空の出力先のままfalseを返す。
	if(device == nullptr){
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_GRAPHICS,L"TextureLoaderにDeviceが渡されていません。");
		return false;
	}

	// パスが空ならAsset分類へ記録して失敗にする。空パスを正常な単色指定へ読み替える処理は行わない。
	if(filePath.empty()){
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_ASSET,L"Textureファイルパスが空です。");
		return false;
	}

	// viewは終端を保証しないため、Filename API用に終端付きwstringを作る。相対パスは作業ディレクトリ基準。
	// 埋込NULL検査や正規化は追加しない。文字列・画素配列・ログ等のメモリ確保例外も捕捉しない。
	const std::wstring file_path_text{filePath};

	// 一時WIC参照はローカルComPtrが所有し、各returnや例外で解放する。呼出スレッドのCOM初期化は外部の前提。
	ComPtr<IWICImagingFactory> WIC_factory{};
	ComPtr<IWICBitmapDecoder> decoder{};
	ComPtr<IWICBitmapFrameDecode> frame{};
	ComPtr<IWICFormatConverter> converter{};

	// プロセス内WIC Factoryを生成する。集約指定なしで、IID_PPV_ARGSによりInterfaceと出力先を対応させる。
	const HRESULT factory_result = CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(WIC_factory.GetAddressOf()));
	// Factory生成失敗では、API名・パス・HRESULTを記録し、出力SRVを空のまま返す。
	if(FAILED(factory_result)){
		WriteTextureAssetError(L"CoCreateInstance(IWICImagingFactory)",filePath,factory_result);
		return false;
	}

	// 読取専用で開き、Vendorは指定しない。CacheOnLoadはメタデータの方針で、全画素配列の取得はまだ行わない。
	const HRESULT decoder_result = WIC_factory->CreateDecoderFromFilename(file_path_text.c_str(),nullptr,GENERIC_READ,
																																			 WICDecodeMetadataCacheOnLoad,decoder.GetAddressOf());
	// Decoder生成失敗では画像取得を中止する。生成済みのローカルFactory等の所有参照は寿命に従って解放する。
	if(FAILED(decoder_result)){
		WriteTextureAssetError(L"IWICImagingFactory::CreateDecoderFromFilename",filePath,decoder_result);
		return false;
	}

	// Index 0の先頭フレームだけを取得する。全フレーム列挙やアニメーション再生は行わない。
	const HRESULT frame_result = decoder->GetFrame(0,frame.GetAddressOf());
	// 先頭フレーム取得失敗ならデコードを続けず、Assetログを出して空のSRVとfalseを返す。
	if(FAILED(frame_result)){
		WriteTextureAssetError(L"IWICBitmapDecoder::GetFrame",filePath,frame_result);
		return false;
	}

	// pixel単位の幅・高さを取得する。取得後にサイズ0も別条件で除外する。
	UINT width{};
	UINT height{};
	const HRESULT image_size_result = frame->GetSize(&width,&height);
	// サイズ取得APIが失敗した場合は、未取得の寸法で容量計算やGPU生成を行わない。
	if(FAILED(image_size_result)){
		WriteTextureAssetError(L"IWICBitmapFrameDecode::GetSize",filePath,image_size_result);
		return false;
	}
	// どちらかの寸法が0なら画像として処理せず、出力SRVを空のまま返す。
	if(width == 0 || height == 0){
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_ASSET,L"Textureの幅または高さが0です。");
		return false;
	}

	// 元画像形式を固定の32bit BGRAへ変換するためのConverterを作る。
	const HRESULT converter_result = WIC_factory->CreateFormatConverter(converter.GetAddressOf());
	// Converter生成失敗なら画素を取得せず戻る。先行するWIC参照はローカルComPtrで解放する。
	if(FAILED(converter_result)){
		WriteTextureAssetError(L"IWICImagingFactory::CreateFormatConverter",filePath,converter_result);
		return false;
	}

	// BGRA各8bitの出力形式をD3DのB8G8R8A8と対応させる。Ditherなし・Paletteなし・閾値0の既存設定を維持する。
	// 閾値0は画像全体のAlphaを0へ書き換える指定ではない。CanConvertの追加検証や色空間変換は行わない。
	const HRESULT converter_initialize_result = converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,
																																		nullptr,0.0,WICBitmapPaletteTypeCustom);
	// BGRA変換の初期化に失敗した場合は、未準備のConverterから画素を読まずAssetログとfalseを返す。
	if(FAILED(converter_initialize_result)){
		WriteTextureAssetError(L"IWICFormatConverter::Initialize",filePath,converter_initialize_result);
		return false;
	}

	// row_pitchは1行のバイト数でwidth*4、image_sizeは全行の総容量。pixel数とバイト数を区別する。
	// 既存のUINT乗算のままでオーバーフロー検証はしない。CPU配列確保の例外もfalseへ変換せず伝播する。
	const UINT row_pitch = width * ns_BYTES_PER_PIXEL;
	const UINT image_size = row_pitch * height;
	std::vector<unsigned char> pixels(image_size);

	// nullptrの領域指定で画像全体をCPU配列へ取得する。Strideと容量はバイト単位で、GPUへ直接転送する段階ではない。
	const HRESULT copy_pixels_result = converter->CopyPixels(nullptr,row_pitch,image_size,pixels.data());
	// 画素コピー失敗ならGPU Resourceを作らず戻る。CPU配列とWIC参照はローカルの寿命に従って解放する。
	if(FAILED(copy_pixels_result)){
		WriteTextureAssetError(L"IWICBitmapSource::CopyPixels",filePath,copy_pixels_result);
		return false;
	}

	// Width/Heightはpixel、MipLevels=1は1段、ArraySize=1は1画像、Count=1は非MSAA。
	// B8G8R8A8_UNORMは変換済みBGRAに対応する。sRGB形式へ変更せず、DEFAULT・Shader参照用途の既存設定を維持する。
	// ゼロ初期化したQuality・CPUAccessFlags・MiscFlags等は0。自動Mip生成、Render Target、直接Map用途は追加しない。
	D3D11_TEXTURE2D_DESC texture_description{};
	texture_description.Width = width;
	texture_description.Height = height;
	texture_description.MipLevels = 1;
	texture_description.ArraySize = 1;
	texture_description.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	texture_description.SampleDesc.Count = 1;
	texture_description.Usage = D3D11_USAGE_DEFAULT;
	texture_description.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	// 1Mip×1画像に対して初期データ記述を1個用意する。WICで使った行間隔をそのままD3Dにも渡す。
	// CPU配列の所有権は移さず、生成呼出中に有効に保つ。2Dで不要なSysMemSlicePitchは0のまま。
	D3D11_SUBRESOURCE_DATA texture_data{};
	texture_data.pSysMem = pixels.data();
	texture_data.SysMemPitch = row_pitch;

	// 一時Textureはローカルで所有する。SRV生成後はView側の参照でTextureの寿命を保持する構成。
	ComPtr<ID3D11Texture2D> texture{};
	const HRESULT texture_result = device->CreateTexture2D(&texture_description,&texture_data,texture.GetAddressOf());
	// GPU Texture生成失敗をGraphicsへ記録し、SRVを空のまま返す。先行するCPU/WICの一時データも解放される。
	if(FAILED(texture_result)){
		WriteTextureGraphicsError(L"ID3D11Device::CreateTexture2D",filePath,texture_result);
		return false;
	}

	// nullptrのView記述でResourceに基づく既定SRVを生成し、出力ComPtrへ所有参照を渡す。BindやSampler設定はしない。
	const HRESULT texture_view_result = device->CreateShaderResourceView(texture.Get(),nullptr,textureView.GetAddressOf());
	// SRV生成失敗ならログ後に出力先も明示的に空へ戻す。一時Textureの所有参照はローカルComPtrが解放する。
	if(FAILED(texture_view_result)){
		WriteTextureGraphicsError(L"ID3D11Device::CreateShaderResourceView",filePath,texture_view_result);
		textureView.Reset();
		return false;
	}

	// 成功した出力SRVだけを残す。CPU画素配列とローカルWIC/Textureの所有参照は関数終了時に解放する。
	return true;
}