#pragma once

// 頂点Buffer・Index Bufferを、CPU配列を初期データとして生成する共通の補助を宣言する。
// 生成に失敗した時は、呼出側が指定したAPI名とHRESULTを、ログへ記録して失敗を返す。失敗後のResourceの解放・再試行は担当しない。
// 生成したBufferの所有は、呼出側の出力先ComPtrが持つ。DeviceとCPU配列は借用し、保持しない。

//========= 外部ライブラリ インクルード=========
#include <d3d11.h> // Device・Bufferの型と、Usage・Bind Flagを使用する。

//========= C++標準ライブラリ インクルード=========
#include <cstddef> // std::size_tで、初期データの総バイト数を扱う。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

//========= 構造体=========
// Bufferの生成に必要な、初期データと用途の指定。
struct MESH_BUFFER_SOURCE
{
  // 初期データの先頭。呼出中だけ有効であればよい。所有権は移さない。nullptrは不可。
  const void* data{};
  // 初期データの総バイト数。要素数ではない。UINTへの変換の上限は検証しない。
  std::size_t byteWidth{};
  // GPUとCPUのアクセスの用途。D3D11_USAGE_DEFAULTまたはD3D11_USAGE_IMMUTABLE。
  D3D11_USAGE usage{};
  // Bufferの用途。D3D11_BIND_VERTEX_BUFFERまたはD3D11_BIND_INDEX_BUFFER。
  UINT bindFlags{};
  // 失敗時のログに使うAPI名。「ID3D11Device::CreateBuffer(VertexBuffer)」の形。有効な終端付きの文字列で、nullptrは不可。
  const wchar_t* errorName{};
};

// 頂点Buffer・Index Bufferの生成を提供する。
class MeshBuffers final
{
public:
  //========= 生成関数=========
  // 初期データを持つBufferを1個生成し、出力先へ所有参照を受け取る。
  // device: 生成に使うDevice。nullptr不可。
  // source: 初期データと用途の指定。CPUAccessFlagsなどの残りの設定は、0のまま。
  // output: 所有参照の出力先。呼出前に空であること。失敗時は、空のまま、またはCreateBufferが設定した値のまま。
  // 戻り値: 成功したらtrue。失敗はAPI名とHRESULTをログへ記録してfalse。
  [[nodiscard]] static bool Create(ID3D11Device* device,const MESH_BUFFER_SOURCE& source,ComPtr<ID3D11Buffer>& output);

private:
  //========= 生成禁止関数=========
  // MeshBuffersのインスタンス生成を禁止する。
  MeshBuffers() = delete;
};
