#pragma once

// DirectXMathの型・数学関数、WRLのCOM所有型、DirectXTKの描画・音声型を共通の名前で参照できるようにする。
// 独自の演算、Resource生成、COM初期化、描画・再生の実行は行わない。型や関数の元の契約を変更しない。
// 数学だけでなく描画・音声Headerにも依存する。依存分割・外部識別子の改名は今回のコメント整理に含めない。

//========= 外部ライブラリ インクルード=========
#include <Audio.h> // AudioEngine、SoundEffect、SoundEffectInstanceと既定の音声初期化設定を公開する。
#include <DirectXMath.h> // 保存用・演算用のベクトル/行列型と、下記で使用宣言する数学関数を提供する。
#include <wrl/client.h> // COMインターフェースの参照カウントを管理するMicrosoft::WRL::ComPtrを提供する。
#include <SpriteBatch.h> // DirectXTKの2D描画Batch型を共通名で参照するために使用する。
#include <SpriteFont.h> // DirectXTKの文字描画・計測用Font型を共通名で参照するために使用する。

//========= 保存用ベクトル・行列型=========
// 元の外部型名を維持する別名。新しい構造体やメモリ配置は作らず、独自の型名規則へ改名しない。
// 2成分の保存用型。位置・サイズ・UVなど、成分の意味と単位は利用箇所が決める。
using XMFLOAT2 = DirectX::XMFLOAT2;
// 3成分の保存用型。位置・方向・回転などを保持するが、型自体に座標系や正規化済みという保証はない。
using XMFLOAT3 = DirectX::XMFLOAT3;
// 4成分の保存用型。RGBA色や転送値などに使う。値の範囲制限はこの別名では行わない。
using XMFLOAT4 = DirectX::XMFLOAT4;
// 4×4行列の保存用型。GPUへ渡す行列の転置やHLSLとの配置確認はRendererなど利用側の責務。
using XMFLOAT4X4 = DirectX::XMFLOAT4X4;

//========= 演算用・引数・色型=========
// SIMDベクトル演算用の元の型。保存用float3等との入出力は、必要なLoad/Store関数で利用側が行う。
using XMVECTOR = DirectX::XMVECTOR;
// 行列演算用の元の型。World・View・Projectionの意味と積の順序は利用側が定義する。
using XMMATRIX = DirectX::XMMATRIX;
// DirectXMathが定めるベクトル引数用の型。独自ラッパーで呼出規約や引数の渡し方を置き換えない。
using FXMVECTOR = DirectX::FXMVECTOR;
// 定数色などの4成分値を保持する元の型。RGBAなどの用途と有効範囲は呼出側の契約に従う。
using XMVECTORF32 = DirectX::XMVECTORF32;

//========= COM所有型=========
// 元のComPtrテンプレートの別名。保持するCOM参照の解放と、ContextのBind解除・GPU完了待機は別の処理。
// TInterface: 保持するCOMインターフェース型。借用ポインターや出力先の扱いは元のComPtrの契約に従う。
template<class TInterface>
using ComPtr = Microsoft::WRL::ComPtr<TInterface>;

//========= DirectXTKの2D描画型=========
// 下記は元の型を参照するusing宣言。ここではBatchやFontの実体を生成・所有しない。
// 2D描画をまとめて扱うBatch型。Begin/Endの対応や依存Contextの寿命は利用側が管理する。
using DirectX::SpriteBatch;
// SpriteFont形式のFont型。読込・文字列描画・計測を提供するが、その実行と所有は利用側が担当する。
using DirectX::SpriteFont;

//========= DirectXTKの音声型=========
// 音声Engine型。生成、Update、Reset、終了などの操作はAudioSystem側で行う。
using DirectX::AudioEngine;
// 読込済み音声データの型。単発再生や再生Instanceの生成に使い、依存Engineとの寿命は利用側で管理する。
using DirectX::SoundEffect;
// BGMなどの再生状態を持つInstance型。停止・音量・ループ再生は利用側から操作する。
using DirectX::SoundEffectInstance;

//========= DirectXTKの音声初期化設定=========
// AudioEngineへ渡す外部ライブラリ既定の初期化設定名。独自定数へ複製せず、元の識別子を使用する。
using DirectX::AudioEngine_Default;

//========= 行列生成関数=========
// 以下は元の関数を修飾なしで参照するための宣言であり、独自関数や引数検証を追加するものではない。
// X・Y・Z各軸の倍率から拡大縮小行列を生成する。距離や角度ではなく、各軸のScaleを受け取る。
using DirectX::XMMatrixScaling;
// 3成分ベクトルの各成分を、X・Y・Z各軸の倍率として、拡大縮小行列を生成する。スキニングのアニメーションの拡大のキーから、行列を作るために使用する。
using DirectX::XMMatrixScalingFromVector;
// Pitch・Yaw・Rollの各角度をラジアンで受け取り、回転行列を生成する。度からの変換は利用側で行う。
using DirectX::XMMatrixRotationRollPitchYaw;
// クォータニオンから回転行列を生成する。入力は単位の長さであること。スキニングのアニメーションの回転のキーから、行列を作るために使用する。
using DirectX::XMMatrixRotationQuaternion;
// X・Y・Zの移動量から平行移動行列を生成する。どの座標系で使うかは合成する利用側が決める。
using DirectX::XMMatrixTranslation;
// 3成分ベクトルを移動量として、平行移動行列を生成する。スキニングのアニメーションの位置のキーから、行列を作るために使用する。
using DirectX::XMMatrixTranslationFromVector;
// 何も変換しない単位行列を生成する。行列の初期値や、使わないボーン行列の埋め草に使用する。
using DirectX::XMMatrixIdentity;
// 視点位置・視線方向・上方向から左手系View行列を生成する。第2引数は注視点ではなく方向。
using DirectX::XMMatrixLookToLH;
// 垂直視野角(ラジアン)、幅/高さの比、近・遠Clip距離から左手系Projection行列を生成する。
using DirectX::XMMatrixPerspectiveFovLH;

//========= 行列演算・変換・格納関数=========
// 2つの行列の積を返す。行ベクトルの向きで、第1引数の変換を先に、第2引数の変換を後に適用する順序で合成する。
using DirectX::XMMatrixMultiply;
// 行列を、拡大・回転（クォータニオン）・移動の3つへ分解する。分解できなければfalseを返し、出力は不定。
using DirectX::XMMatrixDecompose;
// 行と列を入れ替えた行列を返す。入力自体の書換えや、座標系の自動変換を行うラッパーではない。
using DirectX::XMMatrixTranspose;
// 有効なXMFLOAT4X4から、演算用の行列へ読み込む。転置はしない。保存済みのボーン行列やノードの変換を、計算へ戻すために使用する。
using DirectX::XMLoadFloat4x4;
// 行列を有効なXMFLOAT4X4出力先へ保存する。転置は行わず、必要なら利用側が先にTransposeを適用する。
using DirectX::XMStoreFloat4x4;

//========= 角度変換関数=========
// 度単位の角度をラジアンへ換算する。三角関数や行列生成へ渡す単位を揃えるために使用する。
using DirectX::XMConvertToRadians;

//========= ベクトル生成・入出力関数=========
// X・Y・Z・Wの4成分から演算用ベクトルを生成する。位置・方向などのWの選択は利用側が行う。
using DirectX::XMVectorSet;
// 有効なXMFLOAT3のXYZを演算用ベクトルへ読み込む。元データを所有したり位置へ自動変換したりしない。
using DirectX::XMLoadFloat3;
// 有効なXMFLOAT4のXYZWを演算用ベクトルへ読み込む。クォータニオンや色を、計算へ渡すために使用する。
using DirectX::XMLoadFloat4;
// 演算用ベクトルのXYZを有効なXMFLOAT3出力先へ書き込む。W成分はこの出力型には保存しない。
using DirectX::XMStoreFloat3;

//========= ベクトル成分取得関数=========
// 演算用ベクトルからX成分をfloatで取得する。HudTextRendererでは計測幅の取り出しに使用する。
using DirectX::XMVectorGetX;
// 演算用ベクトルからY成分をfloatで取得する。HudTextRendererでは計測高さの取り出しに使用する。
using DirectX::XMVectorGetY;

//========= ベクトル演算関数=========
// XYZを3次元方向として単位化する元の関数。ゼロ長などに対する独自の条件判定・代替方向は追加しない。
using DirectX::XMVector3Normalize;
// XYZの外積を求める。引数順を逆にすると符号が反転するため、利用側で左右方向との対応を維持する。
using DirectX::XMVector3Cross;
// 2つのベクトルの間を、割合で直線補間する。割合0で第1引数、1で第2引数。位置・拡大のキーの補間に使用する。
using DirectX::XMVectorLerp;

//========= クォータニオン関数=========
// クォータニオンを、単位の長さへ整える。補間後の回転の誤差を取り除くために使用する。
using DirectX::XMQuaternionNormalize;
// 2つのクォータニオンの間を、割合で球面補間する。割合0で第1引数、1で第2引数。回転のキーの補間に使用する。
using DirectX::XMQuaternionSlerp;

//========= 既存の重複using宣言=========
// 原本末尾にある同じ関数の再宣言を維持する。重複整理は今回混ぜず、新しい別名や関数実装も追加しない。
using DirectX::XMMatrixPerspectiveFovLH;

using DirectX::XMVectorGetZ;
using DirectX::XMVectorAdd;