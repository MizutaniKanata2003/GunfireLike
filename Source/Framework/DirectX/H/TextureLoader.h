#pragma once

// WICで画像の先頭フレームをCPU側の32bit BGRA配列へ変換し、D3D11 TextureとSRVを生成する。
// 成功したSRVの所有参照を呼出側へ渡す。ShaderへのBind、Sampler設定、描画、自動Mip生成は担当しない。

//========= C++標準ライブラリ インクルード=========
#include <cstddef> // std::size_tで、画像のバイト数を表す。
#include <cstdint> // std::uint8_tで、画像のファイルの中身を、バイト列として扱う。
#include <string_view> // 呼出側のワイド文字パスをstd::wstring_viewで非所有のまま受け取る。

//========= 外部ライブラリ インクルード=========
#include <d3d11.h> // DeviceとShader Resource Viewの型を公開関数の入出力に使用する。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

// メモリ上にある、圧縮された画像（PNG・JPEGなど）のファイルの中身の、借用の束。読込の引数の数を減らす。所有はしない。
struct ENCODED_IMAGE_VIEW
{
  // ファイルの中身の先頭。呼出中だけ有効であればよい。
  const std::uint8_t* data{};
  // ファイルの中身のバイト数。
  std::size_t size{};
  // ログに使う名前。
  std::wstring_view name{};
  // 縮小表示のちらつきを防ぐため、全てのMip（縮小画像）を、GPUで生成するか。trueなら、Mip付きのTextureにする。
  bool generateMips{};
};

// デコード用COM参照と一時Textureを呼出中のローカルで所有する、状態を持たない画像Loader。
class TextureLoader final
{
public:
  //========= Texture読込関数=========
  // 先頭フレームをBGRAへ変換し、単一Mip・単一画像・非MSAAのTextureとShader参照用Viewを生成する。
  // device: 呼出中に有効な生成用Deviceの借用ポインター。nullptrならGraphicsログを出してfalse。
  // filePath: 画像のワイド文字パス。空ならAssetログを出してfalse。終端付きwstringへコピーし、参照は保持しない。
  // textureView: 呼出側が所有するSRVの出力先。引数検証より前にResetし、成功したViewの所有参照へ置き換える。
  // 戻り値: 全処理成功ならtrue。検出した引数不正・WIC処理失敗・GPU生成失敗ではfalseで出力先は空。
  // 旧SRVへの復帰、別形式の再試行は行わない。画像の失敗はAsset、GPU生成の失敗はGraphicsへ記録する。
  // 前提: 呼出スレッドのCOM初期化は呼出側が済ませる。この関数はCoInitialize/CoUninitializeを行わない。
  // 返すViewが参照先Textureの寿命も保持する。返却後の所有・Bind・解放は呼出側が管理する。
  // メモリ確保・ログ生成等のC++例外は捕捉せず、例外経路をfalse返却時の状態保証には含めない。
  // UINTによる画像容量の乗算オーバーフロー検証、埋込NULL検査、向き補正、明示的色空間変換は行わない。
  // アニメーション処理・自動Mip生成は行わない。公開名LoadWicTextureは既存参照との互換性のため維持する。
  static bool LoadWicTexture(ID3D11Device* device,std::wstring_view filePath,ComPtr<ID3D11ShaderResourceView>& textureView);

  // メモリ上の、圧縮された画像の、先頭フレームをBGRAへ変換し、TextureとSRVを生成する。条件・失敗の扱いは、LoadWicTextureと同じ。
  // device: 生成に使うDevice。nullptrならGraphicsログを出してfalse。
  // image: 画像のファイルの中身。dataがnullptr、またはsizeが0ならAssetログを出してfalse。
  // textureView: SRVの出力先。引数検証より前にResetし、成功したViewの所有参照へ置き換える。
  // 戻り値: 全処理成功ならtrue。
  static bool LoadWicTextureFromMemory(ID3D11Device* device,const ENCODED_IMAGE_VIEW& image,ComPtr<ID3D11ShaderResourceView>& textureView);
private:
  //========= 生成禁止関数=========
  // 静的な読込関数だけを使用するため、インスタンス生成を禁止する。
  TextureLoader() = delete;
};