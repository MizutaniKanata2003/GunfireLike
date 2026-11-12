#pragma once

// Rendererが扱う頂点属性の組み合わせを識別する列挙型を定義する。
// 頂点データのメモリ配置やInput Layoutの生成は、このファイルでは担当しない。

//========= 頂点形式識別型=========
// 頂点が持つ属性の組み合わせを表す。
// この値だけでは座標の次元数や各属性のバイト位置は確定しない。
// 実際の頂点構造体とVertex Shader入力の対応は、各RendererのInput Layoutで定義する。
enum class VERTEX_FORMAT
{
	// Positionのみ。現在のHUD Quad描画で使用する。
	e_POSITION,
	// Positionと頂点Colorを持つ形式。
	e_POSITION_COLOR,
	// PositionとTexture UVを持つ形式。現在のCube・OBJ描画で使用する。
	e_POSITION_TEXTURE,
	// Position、Normal、Texture UVを持つ形式。値の定義だけでLighting機能の実装済みを意味しない。
	e_POSITION_NORMAL_TEXTURE
};