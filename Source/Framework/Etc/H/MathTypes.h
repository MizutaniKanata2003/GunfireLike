#pragma once

// DirectXMathの型別名・数学関数、COM所有型、DirectXTKの2D描画型・音声型の共通宣言をまとめる。
// 関数やクラスは再実装せず、元の型・関数・呼出規約をそのまま使用する。
// 描画型と音声型も公開するため、このHeaderはDirectXTKの描画・音声Headerへ依存する。

//========= 外部ライブラリ インクルード=========
#include <Audio.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <SpriteBatch.h>
#include <SpriteFont.h>

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
// 定数色などの保持に使用する型。重複するusing宣言は追加しない。
using XMVECTORF32 = DirectX::XMVECTORF32;

//========= COM所有型=========
// COMインターフェースの参照カウントを管理する。
// TInterface: 所有するCOMインターフェースの型。
template<class TInterface>
using ComPtr = Microsoft::WRL::ComPtr<TInterface>;

//========= DirectXTKの2D描画型=========
// 2Dスプライトや文字列の描画をまとめて処理するDirectXTKのクラス。
using DirectX::SpriteBatch;
// SpriteFont形式のフォントを保持し、文字列描画と文字サイズ計測を行うDirectXTKのクラス。
using DirectX::SpriteFont;

//========= DirectXTKの音声型=========
// XAudio2を使用した音声Engineの生成・更新・Resetを行うクラス。
using DirectX::AudioEngine;
// 読み込んだ音声データを保持し、単発再生や再生Instanceの生成を行うクラス。
using DirectX::SoundEffect;
// BGMなどの再生状態を保持し、音量設定・ループ再生・停止を行うクラス。
using DirectX::SoundEffectInstance;

//========= DirectXTKの音声初期化設定=========
// AudioEngineの生成に使用する既定の初期化フラグ。
using DirectX::AudioEngine_Default;

//========= 行列生成関数=========
// 各軸の拡大縮小率から行列を生成する。
using DirectX::XMMatrixScaling;
// Pitch、Yaw、Roll（ラジアン）から回転行列を生成する。
using DirectX::XMMatrixRotationRollPitchYaw;
// 各軸の移動量から平行移動行列を生成する。
using DirectX::XMMatrixTranslation;
// カメラ位置、視線方向、上方向から左手系View行列を生成する。
using DirectX::XMMatrixLookToLH;
// 垂直視野角（ラジアン）、アスペクト比、近距離・遠距離Clip面から左手系の透視投影行列を生成する。
using DirectX::XMMatrixPerspectiveFovLH;

//========= 行列変換・格納関数=========
// 行と列を入れ替えた転置行列を取得する。
using DirectX::XMMatrixTranspose;
// 演算用行列を有効なXMFLOAT4X4のポインターへ書き込む。
using DirectX::XMStoreFloat4x4;

//========= 角度変換関数=========
// 度をラジアンへ変換する。
using DirectX::XMConvertToRadians;

//========= ベクトル生成・入出力関数=========
// 4成分から演算用ベクトルを生成する。
using DirectX::XMVectorSet;
// 有効なXMFLOAT3のポインターから演算用ベクトルを読み込む。
using DirectX::XMLoadFloat3;
// 演算用ベクトルのXYZを有効なXMFLOAT3のポインターへ書き込む。
using DirectX::XMStoreFloat3;

//========= ベクトル成分取得関数=========
// 演算用ベクトルのX成分をfloatで取得する。
using DirectX::XMVectorGetX;
// 演算用ベクトルのY成分をfloatで取得する。
using DirectX::XMVectorGetY;

//========= ベクトル演算関数=========
// XYZを3次元ベクトルとして正規化する。
using DirectX::XMVector3Normalize;
// 2つの3次元ベクトルの外積を計算する。
using DirectX::XMVector3Cross;

using DirectX::XMMatrixPerspectiveFovLH;