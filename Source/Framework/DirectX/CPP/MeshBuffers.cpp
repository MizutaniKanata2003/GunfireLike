#include "../H/MeshBuffers.h"

// 初期データを持つBufferの生成と、失敗時のログの記録を実装する。
// 既存のBuffer記述（ByteWidth・Usage・BindFlags以外は0）を維持し、新しい検証や例外捕捉は追加しない。

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsErrorLog.h"

//========= 生成関数=========
// 初期データを持つBufferを1個生成する。
bool MeshBuffers::Create(ID3D11Device* device,const MESH_BUFFER_SOURCE& source,ComPtr<ID3D11Buffer>& output)
{
  // ByteWidthは総バイト数。UINTへの変換の上限は検証しない。CPUAccessFlags等の残りはゼロ設定を維持する。
  D3D11_BUFFER_DESC buffer_desc{};
  buffer_desc.ByteWidth = static_cast<UINT>(source.byteWidth);
  buffer_desc.Usage = source.usage;
  buffer_desc.BindFlags = source.bindFlags;

  // CPU配列を読取元として渡し、所有は移さない。BufferなのでPitch類はゼロのまま。
  D3D11_SUBRESOURCE_DATA buffer_data{};
  buffer_data.pSysMem = source.data;
  const HRESULT result = device->CreateBuffer(&buffer_desc,&buffer_data,output.GetAddressOf());

  // 失敗ならAPI名とHRESULTを記録して失敗を返す。Resourceの解放は、呼出側が行う。
  if(FAILED(result)){
    GraphicsErrorLog::Write(source.errorName,result);
    return false;
  }
  return true;
}
