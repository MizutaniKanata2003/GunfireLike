#pragma once

// DirectXの型別名と、修飾なしで使用する数学関数のusing宣言をまとめる。
// 関数を再実装せず、DirectXMath本来の関数を使用する。

//========= 外部ライブラリ インクルード=========
#include <DirectXMath.h>
#include <wrl/client.h>

//========= 保存用ベクトル・行列型=========
// 座標やUVに使用する2成分の保存用型。
using XMFLOAT2 = DirectX::XMFLOAT2;
// 座標・方向・回転・Scaleに使用する3成分の保存用型。
using XMFLOAT3 = DirectX::XMFLOAT3;
// 色などに使用する4成分の保存用型。
using XMFLOAT4 = DirectX::XMFLOAT4;
// Constant Bufferなどに格納する保存用4×4行列型。
using XMFLOAT4X4 = DirectX::XMFLOAT4X4;

//========= 演算用・引数・色型=========
// SIMDベクトル演算用型。
using XMVECTOR = DirectX::XMVECTOR;
// 行列演算用型。
using XMMATRIX = DirectX::XMMATRIX;
// DirectXMathのベクトル引数型。元の呼出規約を維持する。
using FXMVECTOR = DirectX::FXMVECTOR;
// 定数色などの保持に使用する型。
using XMVECTORF32 = DirectX::XMVECTORF32;

//========= COM所有型=========
// COMインターフェースの参照カウントを管理する。
// TInterface: 所有するCOMインターフェースの型。
template<class TInterface>
using ComPtr = Microsoft::WRL::ComPtr<TInterface>;

//========= 行列生成・角度変換関数=========
// 各軸の拡大縮小率から行列を生成する。
using DirectX::XMMatrixScaling;
// Pitch、Yaw、Roll（ラジアン）から回転行列を生成する。
using DirectX::XMMatrixRotationRollPitchYaw;
// 各軸の移動量から平行移動行列を生成する。
using DirectX::XMMatrixTranslation;
// カメラ位置、視線方向、上方向から左手系View行列を生成する。
using DirectX::XMMatrixLookToLH;
// 度をラジアンへ変換する。
using DirectX::XMConvertToRadians;

//========= ベクトル生成・入出力関数=========
// 4成分から演算用ベクトルを生成する。
using DirectX::XMVectorSet;
// 有効なXMFLOAT3のポインターから演算用ベクトルを読み込む。
using DirectX::XMLoadFloat3;
// 演算用ベクトルのXYZを有効なXMFLOAT3のポインターへ書き込む。
using DirectX::XMStoreFloat3;

//========= ベクトル演算関数=========
// XYZを3次元ベクトルとして正規化する。
using DirectX::XMVector3Normalize;
// 2つの3次元ベクトルの外積を計算する。
using DirectX::XMVector3Cross;

//========= DirectXMath関数のusing宣言=========
// 行と列を入れ替えた行列を取得するDirectXMath関数を、修飾なしで使用できるようにする。
using DirectX::XMMatrixTranspose;
// SIMD行列をXMFLOAT4X4へ格納するDirectXMath関数を、修飾なしで使用できるようにする。
using DirectX::XMStoreFloat4x4;

using DirectX::SpriteBatch;

using DirectX::SpriteFont;

using DirectX::XMVECTORF32;

using DirectX::XMVectorGetX;

using DirectX::XMVectorGetY;