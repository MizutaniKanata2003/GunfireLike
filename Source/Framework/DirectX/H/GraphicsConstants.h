#pragma once

//========= DirectX インクルード=========
#include <DirectXMath.h>

// Camera用Constant Bufferデータを保持する。
struct CameraConstants final
{
	DirectX::XMFLOAT4X4 viewProjectionMatrix{};
};

// Object用Constant Bufferデータを保持する。
struct ObjectConstants final
{
	DirectX::XMFLOAT4X4 worldMatrix{};
};

// Material用Constant Bufferデータを保持する。
struct MaterialConstants final
{
	DirectX::XMFLOAT4 baseColor{ 1.0f, 1.0f, 1.0f, 1.0f };
};

// Directional Light用Constant Bufferデータを保持する。
struct LightConstants final
{
	DirectX::XMFLOAT3 direction{ 0.0f, -1.0f, 0.0f };
	float intensity{ 1.0f };
	DirectX::XMFLOAT3 color{ 1.0f, 1.0f, 1.0f };
	float ambientIntensity{ 0.1f };
};

// PostProcess用Constant Bufferデータを保持する。
struct PostProcessConstants final
{
	DirectX::XMFLOAT2 screenSize{};
	DirectX::XMFLOAT2 inverseScreenSize{};
	float time{};
	float intensity{};
	DirectX::XMFLOAT2 padding{};
};

static_assert( sizeof( CameraConstants ) % 16 == 0, "CameraConstantsは16バイト単位である必要があります。" );
static_assert( sizeof( ObjectConstants ) % 16 == 0, "ObjectConstantsは16バイト単位である必要があります。" );
static_assert( sizeof( MaterialConstants ) % 16 == 0, "MaterialConstantsは16バイト単位である必要があります。" );
static_assert( sizeof( LightConstants ) % 16 == 0, "LightConstantsは16バイト単位である必要があります。" );
static_assert( sizeof( PostProcessConstants ) % 16 == 0, "PostProcessConstantsは16バイト単位である必要があります。" );