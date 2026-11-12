#pragma once

// WICで画像をデコードし、Direct3D 11のTextureとShader Resource Viewを生成する共通Loader。
// 描画状態のBindやSampler生成は担当しない。生成したViewは呼出側のComPtrへ渡す。

//========= C++標準ライブラリ インクルード=========
#include <string_view>

//========= 外部ライブラリ インクルード=========
#include <d3d11.h>

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

// インスタンスを持たず、WIC対応画像の読込とGPU Resource生成を提供する。
class TextureLoader final
{
public:
	//========= Texture読込関数=========
	// 画像の先頭フレームを32bit BGRAへ変換し、単一MipのTextureとViewを生成する。
	// device: Resource生成に使用する非所有のDevice。nullptrは失敗。呼出中は有効であること。
	// filePath: 画像のワイド文字パス。空なら失敗。終端付き文字列へコピーし、参照は保持しない。
	// textureView: 呼出側が所有する出力先。開始時にResetし、成功時に生成したViewを保持する。
	// 戻り値: デコードとResource生成がすべて成功した場合はtrue。検出した失敗ではfalseで出力先は空。
	// 失敗はAssetまたはGraphicsカテゴリへログ出力する。メモリ確保などの例外は捕捉しない。
	// 呼出スレッドのCOM初期化は呼出側で済ませておく。この関数ではCOM初期化・終了を行わない。
	// 注意: 画像サイズの乗算オーバーフローを検証する処理は含まない。
	static bool LoadWicTexture(ID3D11Device* device,std::wstring_view filePath,ComPtr<ID3D11ShaderResourceView>& textureView);

private:
	//========= 生成禁止関数=========
	// 静的メソッドだけを使用するため、インスタンス生成を禁止する。
	TextureLoader() = delete;
};