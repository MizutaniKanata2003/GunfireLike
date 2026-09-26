#pragma once

//========= C++標準ライブラリ インクルード=========
#include <type_traits>

//========= DirectX インクルード=========
#include <d3d11.h>
#include <wrl/client.h>

// Constant Bufferを作成、更新、取得するテンプレートクラス。
template<typename T>
class ConstantBuffer final
{
public:
	// Constant Bufferの構造体サイズが16バイト単位であることを検証する。
	static_assert( sizeof( T ) % 16 == 0, "Constant Bufferの構造体サイズは16バイト単位である必要があります。" );

	//========= ライフサイクル関数=========
	// Constant Bufferを生成し、失敗時はHRESULTを返す。
	bool Init( ID3D11Device* device, HRESULT& result )
	{
		result = E_POINTER;

		if ( !device ) return false;

		D3D11_BUFFER_DESC bufferDesc{};
		bufferDesc.ByteWidth = static_cast<UINT>( sizeof( T ) );
		bufferDesc.Usage = D3D11_USAGE_DEFAULT;
		bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

		result = device->CreateBuffer( &bufferDesc, nullptr, m_Buffer.GetAddressOf() );

		return SUCCEEDED( result );
	}

	// Constant Bufferへ最新データを書き込む。
	void Update( ID3D11DeviceContext* context, const T& data ) const
	{
		if ( !context || !m_Buffer ) return;

		context->UpdateSubresource( m_Buffer.Get(), 0, nullptr, &data, 0, 0 );
	}

	// Constant Bufferを解放する。
	void Uninit()
	{
		m_Buffer.Reset();
	}

	//========= Getter・状態取得関数=========
	// Constant Bufferが使用可能かを返す。
	[[nodiscard]] bool IsValid() const
	{
		return m_Buffer != nullptr;
	}

	// Direct3DのConstant Bufferを返す。
	[[nodiscard]] ID3D11Buffer* Get() const
	{
		return m_Buffer.Get();
	}

private:
	// Direct3DのConstant Bufferを保持する。
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_Buffer{};
};