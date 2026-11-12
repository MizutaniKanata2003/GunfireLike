#pragma once

// Direct3D 11の定数バッファを所有し、生成・全体更新・借用参照の取得・解放を提供する。
// DeviceとContextは所有しない。ShaderへのBind、slot選択、行列のtransposeは呼出側が担当する。

//========= 外部ライブラリ インクルード=========
#include <d3d11.h>

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

// CPU側の転送データ型に対応する定数バッファを管理する。
// TValue: 16バイト単位のサイズを持つ転送型。CPUとHLSLのフィールド配置の整合は利用側で確認する。
template<class TValue>
class ConstantBuffer final
{
public:
	//========= データサイズ検証=========
	// サイズ条件だけを検証する。フィールド配置やShaderとの対応を自動で検証するものではない。
	static_assert(sizeof(TValue) % 16 == 0,"Constant Bufferの構造体サイズは16バイト単位である必要があります。");

	//========= ライフサイクル関数=========
	// 指定Deviceで初期データなしのバッファを生成する。使用前に必要なデータをUpdateする。
	// device: 非所有の生成用Device。nullptrなら生成しない。
	// result: nullptr時はE_POINTER、それ以外はCreateBufferのHRESULTを書き込む。
	// 戻り値: HRESULTが成功ならtrue、Device未指定または生成失敗ならfalse。
	// 再生成前にUninitを呼ぶ。既存Resourceの自動解放や失敗後の追加Resetは行わない。
	bool Init(ID3D11Device* device,HRESULT& result)
	{
		// Deviceが未指定でも呼出側が失敗理由を取得できるよう、既定の結果を設定する。
		result = E_POINTER;
		if(!device)return false;

		// CPU型と同じサイズのDEFAULTバッファを作る。初期データは渡さず、生成結果を返す。
		D3D11_BUFFER_DESC buffer_desc{};
		buffer_desc.ByteWidth = static_cast<UINT>(sizeof(TValue));
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		result = device->CreateBuffer(&buffer_desc,nullptr,m_buffer.GetAddressOf());
		return SUCCEEDED(result);
	}

	// データをバッファ全体へ転送する。constでも、所有するGPU Resourceの内容は更新する。
	// context: 非所有の更新用Context。nullptrなら何もしない。
	// data: この呼出中に読み取れる転送元。参照自体は保持しない。
	// 未生成時も何もしない。UpdateSubresourceの実行成否を戻り値で通知するAPIではない。
	void Update(ID3D11DeviceContext* context,const TValue& data) const
	{
		// 更新に必要なContextとBufferが揃わない場合は、転送処理へ進まない。
		if(!context || !m_buffer)return;

		// 先頭のsubresource全体へ転送し、ShaderのBind状態は変更しない。
		context->UpdateSubresource(m_buffer.Get(),0,nullptr,&data,0,0);
	}

	// 所有COM参照を解放する。未生成・解放済みでも呼出可能。
	void Uninit() { m_buffer.Reset(); }

	//========= Getter・状態取得関数=========
	// BufferへのCOM参照を保持しているかを返す。GPU全体の正常性やBind済みかは判定しない。
	[[nodiscard]] bool IsValid() const { return m_buffer != nullptr; }
	// 所有権を移さずBufferの借用ポインターを返す。未生成ならnullptr。
	// 所有者がUninit・再生成・破棄された後も使う場合は、呼出側で別途寿命を管理する。
	[[nodiscard]] ID3D11Buffer* Get() const { return m_buffer.Get(); }

private:
	//========= GPU Resource所有状態=========
	// BufferへのCOM参照。Uninitまたは自身の破棄で解放する。
	ComPtr<ID3D11Buffer> m_buffer{};
};