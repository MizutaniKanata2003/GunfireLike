#pragma once

// Direct3D 11の定数バッファを所有し、生成・データ更新・参照取得・解放を提供する。
// ShaderへのBindとslotの選択は呼出側が担当する。このクラスはDeviceとContextを所有しない。

//========= 外部ライブラリ インクルード=========
#include <d3d11.h>

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

// Shaderへ転送するデータ型に対応した定数バッファを管理する。
// TValue: 転送対象のデータ型。サイズは16バイトの倍数とし、CPU側とHLSL側のレイアウトを一致させる。
template<class TValue>
class ConstantBuffer final
{
public:
	//========= データサイズ検証=========
	// D3D11の定数バッファに必要な16バイト単位のサイズ条件をコンパイル時に確認する。
	static_assert(sizeof(TValue) % 16 == 0,"Constant Bufferの構造体サイズは16バイト単位である必要があります。");

	//========= ライフサイクル関数=========
	// 指定Deviceで定数バッファを生成する。再生成前にはUninitを呼び、既存Resourceを解放しておく。
	// device: 生成に使う非所有Deviceポインター。nullptrの場合は生成しない。
	// result: CreateBufferの結果を書き込む。deviceがnullptrの場合はE_POINTERを書き込む。
	// 戻り値: 生成成功ならtrue、Device未指定または生成失敗ならfalse。
	bool Init(ID3D11Device* device,HRESULT& result)
	{
		// Device未指定の経路でも、呼出側が失敗理由を取得できるようにする。
		result = E_POINTER;
		if(!device)return false;

		// CPU構造体と同じサイズのDEFAULT使用法の定数バッファを生成する。
		D3D11_BUFFER_DESC buffer_desc{};
		buffer_desc.ByteWidth = static_cast<UINT>(sizeof(TValue));
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		result = device->CreateBuffer(&buffer_desc,nullptr,m_buffer.GetAddressOf());
		return SUCCEEDED(result);
	}

	// 指定データをバッファ全体へ書き込む。ShaderへのBindは行わない。
	// context: 更新に使う非所有Contextポインター。nullptrの場合は何もしない。
	// data: この呼出中に読み取れる転送元データ。参照を保持しない。
	// 注意: バッファ未生成の場合は何もしない。UpdateSubresourceの結果を返すAPIではない。
	void Update(ID3D11DeviceContext* context,const TValue& data) const
	{
		if(!context || !m_buffer)return;

		context->UpdateSubresource(m_buffer.Get(),0,nullptr,&data,0,0);
	}

	// 所有するCOM参照を解放する。未生成状態や解放済み状態で呼び出してもよい。
	void Uninit() { m_buffer.Reset(); }

	//========= Getter・状態取得関数=========
	// 利用可能なバッファのCOM参照を保持しているかを返す。
	[[nodiscard]] bool IsValid() const { return m_buffer != nullptr; }
	// ShaderへのBindなどに使用する非所有ポインターを返す。未生成ならnullptr。
	// 戻り値を保持する場合は、この所有者のUninit・破棄による寿命変化に注意する。
	[[nodiscard]] ID3D11Buffer* Get() const { return m_buffer.Get(); }

private:
	//========= GPU Resource所有状態=========
	// 定数バッファのCOM参照を所有する。Uninitまたは自身の破棄で参照を解放する。
	ComPtr<ID3D11Buffer> m_buffer{};
};