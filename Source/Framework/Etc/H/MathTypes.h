#pragma once

// DirectX数学型とMicrosoft COM所有型の別名、および数学演算のラッパー関数を提供する。
// 元の型名・関数名を維持し、演算規則とResourceの所有関係は変更しない。

//========= 外部ライブラリ インクルード=========
#include <DirectXMath.h>
#include <wrl/client.h>

//========= DirectX保存用ベクトル型エイリアス=========
// DirectX::XMFLOAT2の別名。座標やUVなどの2成分浮動小数点値を保持する。
using XMFLOAT2 = DirectX::XMFLOAT2;
// DirectX::XMFLOAT3の別名。座標、回転、Scale、方向などの3成分浮動小数点値を保持する。
using XMFLOAT3 = DirectX::XMFLOAT3;
// DirectX::XMFLOAT4の別名。色などの4成分浮動小数点値を保持する。
using XMFLOAT4 = DirectX::XMFLOAT4;

//========= DirectX演算用ベクトル型エイリアス=========
// DirectX::XMVECTORの別名。ベクトル演算に使用し、保存用のXMFLOAT3やXMFLOAT4とは区別する。
using XMVECTOR = DirectX::XMVECTOR;

//========= DirectX行列型エイリアス=========
// DirectX::XMFLOAT4X4の別名。Constant Bufferへの格納などに使用する保存用4×4行列型。
using XMFLOAT4X4 = DirectX::XMFLOAT4X4;
// DirectX::XMMATRIXの別名。行列演算に使用し、保存用のXMFLOAT4X4とは区別する。
using XMMATRIX = DirectX::XMMATRIX;

//========= Microsoft COM型エイリアス=========
// Microsoft::WRL::ComPtrの別名。参照カウントによるCOMインターフェースの所有を管理する。
// TInterface: 所有するCOMインターフェースの型。
template<class TInterface>
using ComPtr = Microsoft::WRL::ComPtr<TInterface>;

//========= 行列生成関数=========
// 各軸の拡大縮小率から行列を生成する。
// scaleX: X軸方向の拡大縮小率。
// scaleY: Y軸方向の拡大縮小率。
// scaleZ: Z軸方向の拡大縮小率。
// 戻り値: 指定された拡大縮小率を表すXMMATRIX。
[[nodiscard]] inline XMMATRIX XM_CALLCONV XMMatrixScaling(float scaleX,float scaleY,float scaleZ) noexcept
{
	return DirectX::XMMatrixScaling(scaleX,scaleY,scaleZ);
}

// Euler角から回転行列を生成する。引数順と回転規則はDirectXMathに従う。
// pitch: X軸周りの回転角。単位はラジアン。
// yaw: Y軸周りの回転角。単位はラジアン。
// roll: Z軸周りの回転角。単位はラジアン。
// 戻り値: 指定されたEuler角をDirectXMathの回転規則で合成したXMMATRIX。
[[nodiscard]] inline XMMATRIX XM_CALLCONV XMMatrixRotationRollPitchYaw(float pitch,float yaw,float roll) noexcept
{
	return DirectX::XMMatrixRotationRollPitchYaw(pitch,yaw,roll);
}

// 各軸の移動量から平行移動行列を生成する。
// offsetX: X軸方向の移動量。
// offsetY: Y軸方向の移動量。
// offsetZ: Z軸方向の移動量。
// 戻り値: 指定された移動量を表すXMMATRIX。
[[nodiscard]] inline XMMATRIX XM_CALLCONV XMMatrixTranslation(float offsetX,float offsetY,float offsetZ) noexcept
{
	return DirectX::XMMatrixTranslation(offsetX,offsetY,offsetZ);
}

// カメラ位置、視線方向、上方向から左手座標系のView行列を生成する。
// eyePosition: カメラのワールド座標。
// eyeDirection: カメラの視線方向。ゼロベクトルは許可しない。
// upDirection: カメラの上方向。ゼロベクトルや視線方向と平行なベクトルは指定しない。
// 戻り値: ワールド空間からView空間へ変換する左手座標系のXMMATRIX。
[[nodiscard]] inline XMMATRIX XM_CALLCONV XMMatrixLookToLH(DirectX::FXMVECTOR eyePosition,DirectX::FXMVECTOR eyeDirection,DirectX::FXMVECTOR upDirection) noexcept
{
	return DirectX::XMMatrixLookToLH(eyePosition,eyeDirection,upDirection);
}

//========= 角度変換関数=========
// 度で指定された角度をラジアンへ変換する。
// degrees: 変換する角度。単位は度。
// 戻り値: ラジアンへ変換した角度。
[[nodiscard]] constexpr float XMConvertToRadians(float degrees) noexcept
{
	return DirectX::XMConvertToRadians(degrees);
}

//========= ベクトル生成・入出力関数=========
// 各成分を指定して演算用ベクトルを生成する。
// x: X成分。
// y: Y成分。
// z: Z成分。
// w: W成分。
// 戻り値: 指定された4成分を持つXMVECTOR。
[[nodiscard]] inline XMVECTOR XM_CALLCONV XMVectorSet(float x,float y,float z,float w) noexcept
{
	return DirectX::XMVectorSet(x,y,z,w);
}


// 演算用ベクトルのXYZ成分を保存用ベクトルへ書き戻す。
// destination: 書き込み先のXMFLOAT3への有効なポインター。nullptrは許可しない。
// vector: 書き戻すXYZ成分を持つ演算用ベクトル。W成分は保存しない。
// 戻り値: なし。結果はdestinationへ書き込む。
inline void XM_CALLCONV XMStoreFloat3(XMFLOAT3* destination,DirectX::FXMVECTOR vector) noexcept
{
	DirectX::XMStoreFloat3(destination,vector);
}

//========= ベクトル演算関数=========
// XYZ成分を3次元ベクトルとして正規化する。
// vector: 正規化する演算用ベクトル。
// 戻り値: DirectXMathの規則で正規化したXMVECTOR。長さ0の場合はゼロベクトル。
[[nodiscard]] inline XMVECTOR XM_CALLCONV XMVector3Normalize(DirectX::FXMVECTOR vector) noexcept
{
	return DirectX::XMVector3Normalize(vector);
}