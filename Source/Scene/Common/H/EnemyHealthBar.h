#pragma once

//========= Framework インクルード=========
#include "Framework/3D/H/BasicMeshRenderer.h"

//========= 前方宣言=========
class GraphicsSystem;

// Enemy頭上に表示するWorld Space HPバーのBillboard描画を管理する。
class EnemyHealthBar final
{
public:
	//========= 描画関数=========
	// Enemyの現在HPをCamera方向へ向けたBillboard HPバーとして描画する。
	void Draw(
	BasicMeshRenderer& basicMeshRenderer,
	GraphicsSystem& graphicsSystem,
	const DirectX::XMMATRIX& viewMatrix,
	const DirectX::XMMATRIX& projectionMatrix,
	const DirectX::XMFLOAT3& cameraPosition,
	const DirectX::XMFLOAT3& enemyPosition,
	float currentHp,
	float maxHp,
	bool isEnemyDead ) const;
};