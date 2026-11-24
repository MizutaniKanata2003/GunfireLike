#pragma once

// CPU転送型に対応するDirect3D 11定数Bufferを所有し、生成・全体更新・借用参照の取得・解放を提供する。
// DeviceとContextは借用し、保持しない。ShaderへのBind、Stage・slot選択、転送値の計算と行列転置は利用側が行う。
// Bufferの生成・更新・Bindは別操作。生成だけでCPU型の既定値がGPUへ設定されるものではない。

//========= 外部ライブラリ インクルード=========
#include <d3d11.h> // BufferとDevice/Contextの型、生成記述、Usage・Bind Flag、転送APIを使用する。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

// 転送型ごとに固定サイズの定数Bufferを管理する。実装はHeader内に置き、cppへ重複定義しない。
// TValue: サイズが16バイト単位のCPU転送型。GPUへコピーできるデータとし、HLSLとの配置は利用側が照合する。
// このクラスは転送型のフィールド配置、ポインター等の有無、D3Dのサイズ上限を追加検証しない。
template<class TValue>
class ConstantBuffer final
{
public:
  //========= データサイズ検証=========
  // sizeof(TValue)が16の倍数かをコンパイル時に検証する。自動の切上げ・Padding追加は行わない。
  // 総サイズの条件だけであり、各フィールドのOffset・HLSL配置・転送内容の妥当性までは保証しない。
  static_assert(sizeof(TValue) % 16 == 0,"Constant Bufferの構造体サイズは16バイト単位である必要があります。");

  //========= ライフサイクル関数=========
  // 初期データなしで生成する。初回描画の前に利用側がUpdateで値を転送し、必要なShader slotへBindする。
  // device: 呼出中に有効な生成用Deviceの借用ポインター。nullptrでは生成せずfalseを返す。
  // result: HRESULTの出力先。Device未指定ならE_POINTER、それ以外はCreateBufferの結果を書き込む。
  // 戻り値: resultが成功ならtrue。Device未指定・生成失敗ならfalse。ログ出力や再試行は行わない。
  // 再生成前にUninitを呼び、出力先ComPtrを空にすること。既存所有参照の自動解放・失敗後Resetは追加しない。
  bool Init(ID3D11Device* device,HRESULT& result)
  {
    // 早期returnでも呼出側が失敗理由を取得できるよう、Device未指定の結果を先に用意する。
    result = E_POINTER;
    // Deviceがない場合はCreateBufferを呼ばず、Bufferの既存保持状態を変更しないままfalseを返す。
    if(!device)return false;

    // ByteWidthは要素数ではなくCPU型全体のバイト数。UINTへの変換上限を検査する処理は追加しない。
    // DEFAULTは既存の使用法、CONSTANT_BUFFERは定数用途。ゼロ初期化したCPUAccessFlags等は0を維持する。
    // CPUからMapする設定ではなく、後続のUpdateSubresourceで内容を更新する構成。
    D3D11_BUFFER_DESC buffer_desc{};
    buffer_desc.ByteWidth = static_cast<UINT>(sizeof(TValue));
    buffer_desc.Usage = D3D11_USAGE_DEFAULT;
    buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    // 初期データのnullptrは「CPU型を0初期化して転送する」という意味ではない。
    // 生成結果を出力先へ受け取り、HRESULTの成功判定を返す。失敗後の追加Resetや例外捕捉は行わない。
    result = device->CreateBuffer(&buffer_desc,nullptr,m_buffer.GetAddressOf());
    return SUCCEEDED(result);
  }

  // dataをBuffer全体へ転送する。constメソッドでも、参照するGPU Resourceの内容は変更する。
  // context: 有効な更新用Contextの借用ポインター。nullptrでは転送しない。生成Deviceとの対応は利用側が守る。
  // data: 呼出中に読み取れるCPU転送値。参照は保持せず、TValueの配置のまま渡す。行列を自動転置しない。
  // 戻り値はなく、更新成功を通知しない。未生成時は何もしない。ShaderのBind状態は変更しない。
  void Update(ID3D11DeviceContext* context,const TValue& data) const
  {
    // ContextまたはBufferがなければAPIを呼ばず戻る。有効性はポインターの有無だけを確認する。
    if(!context || !m_buffer)return;

    // subresource 0を、領域指定nullptrで全体更新する。&dataがCPU側の読取元で、所有権を移す指定ではない。
    // Buffer転送なので画像用の行・Slice Pitchはともに0。更新だけではShader Stageやslotへ接続されない。
    context->UpdateSubresource(m_buffer.Get(),0,nullptr,&data,0,0);
  }

  // メンバーのCOM所有参照を解放する。未生成・解放済みでも呼出可能。
  // Context等が別に持つ参照は解除せず、Bind解除・状態復元・GPU完了待機は行わない。
  void Uninit() { m_buffer.Reset(); }

  //========= Getter・状態取得関数=========
  // Bufferへの参照を保持していればtrue。GPU正常性、転送値の設定済み、Bind済みまでは判定しない。
  [[nodiscard]] bool IsValid() const { return m_buffer != nullptr; }
  // Bufferの借用ポインターを返す。未生成・解放済みならnullptr。所有権移動やAddRefは行わない。
  // Uninit・再生成・所有者破棄をまたいで使う場合は、利用側で別途参照の寿命を管理する。
  [[nodiscard]] ID3D11Buffer* Get() const { return m_buffer.Get(); }

private:
  //========= GPU Resource所有状態=========
  // BufferへのCOM参照を所有する。既定は空で、Initで受け取り、Uninitまたは自身の破棄でこの参照を解放する。
  ComPtr<ID3D11Buffer> m_buffer{};
};