#pragma once

//========= DirectX インクルード=========
#include <algorithm>

//========= Framework インクルード=========
#include "Framework/3D/H/BasicMeshRenderer.h"

//========= 前方宣言=========
class GraphicsSystem;

// FPS Cameraに追従するGun本体、Gun Barrel、Muzzle Flashの描画を管理する。
class GunViewModel final
{
public:
	//========= 初期化関数=========
	// Gun View ModelのMuzzle Flash状態を初期化する。
	void Initialize();

	//========= 更新関数=========
	// Muzzle Flash表示用Timerを更新する。
	void Update( float deltaTime ) { m_MuzzleFlashTimer = std::max( 0.0f, m_MuzzleFlashTimer - deltaTime ); }

	//========= 操作関数=========
	// Muzzle Flashの表示を開始する。
	void TriggerMuzzleFlash() { m_MuzzleFlashTimer = m_MuzzleFlashDuration; }

	//========= 描画関数=========
	// Gun本体とGun BarrelをOpaque Passで描画する。
	void DrawOpaque( BasicMeshRenderer& basicMeshRenderer, GraphicsSystem& graphicsSystem,
					 const DirectX::XMMATRIX& viewMatrix, const DirectX::XMMATRIX& projectionMatrix ) const;
	// Muzzle FlashをTransparent Passで描画する。
	void DrawTransparent( BasicMeshRenderer& basicMeshRenderer, GraphicsSystem& graphicsSystem,
					 const DirectX::XMMATRIX& viewMatrix, const DirectX::XMMATRIX& projectionMatrix ) const;
private:
	//========= Gun設定=========
	// Gun本体のCamera空間における位置とScale。
	DirectX::XMFLOAT3 m_BodyPosition {};
	DirectX::XMFLOAT3 m_BodyScale {};
	// Gun BarrelのCamera空間における位置とScale。
	DirectX::XMFLOAT3 m_BarrelPosition {};
	DirectX::XMFLOAT3 m_BarrelScale {};
	// Muzzle FlashのCamera空間における位置とScale。
	DirectX::XMFLOAT3 m_MuzzleFlashPosition {};
	float m_MuzzleFlashScale {};
	// Muzzle Flashの表示時間。
	float m_MuzzleFlashDuration {};
	// Muzzle Flashを表示する残り時間。
	float m_MuzzleFlashTimer {};
};