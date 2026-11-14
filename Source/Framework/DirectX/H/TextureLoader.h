#pragma once

// WICで画像をCPU側の32bit BGRA配列へ変換し、D3D11 TextureとShader Resource Viewを生成する。
// 成功したViewの所有を呼出側へ渡す。ShaderへのBind・Sampler設定・描画・Mip生成は担当しない。

//========= C++標準ライブラリ インクルード=========
#include <string_view>

//========= 外部ライブラリ インクルード=========
#include <d3d11.h>

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

// 状態を持たない画像Loader。デコード用COM参照と一時Textureは呼出中のローカルで所有する。
class TextureLoader final
{
public:
	//========= Texture読込関数=========
	// 先頭フレームを32bit BGRAへ変換し、単一Mip・単一画像・非MSAAのTextureとSRVを生成する。
	// device: 初期化済みの非所有Device。nullptrならfalse。Resource生成の呼出中に有効であること。
	// filePath: WICへ渡すワイド文字パス。空ならfalse。終端付きwstringへコピーし、参照は保持しない。
	// textureView: 呼出側の所有参照の出力先。引数検証より前にResetし、成功したViewの参照で置き換える。
	// 戻り値: 全処理成功ならtrue。検出した引数不正・WIC/GPU生成失敗ではfalseで出力先は空。
	// 失敗はAssetまたはGraphicsへログ出力する。旧SRVの復帰や別形式での再試行は行わない。
	// 前提: 呼出スレッドのCOM初期化を呼出側で済ませておく。ここではCoInitialize/CoUninitializeを行わない。
	// Viewは参照先Textureの寿命を保持する。返却後のSRV所有・Bind・解放は呼出側で管理する。
	// メモリ確保などのC++例外は捕捉しない。画像サイズのUINT乗算に対するオーバーフロー検証も行わない。
	// 埋込NULL検査・画像の向き補正・明示的な色空間変換・アニメーション処理・自動Mip生成は追加しない。
	// 公開名LoadWicTextureは既存参照との互換性のため維持する。
	static bool LoadWicTexture(ID3D11Device* device,std::wstring_view filePath,ComPtr<ID3D11ShaderResourceView>& textureView);

private:
	//========= 生成禁止関数=========
	// 静的メソッドだけを使用するため、インスタンス生成を禁止する。
	TextureLoader() = delete;
};