#include "../H/SceneObject.h"

// 設定からの状態コピーと共有Rendererの借用、および種別に応じた描画の呼出しを実装する。
// GPU Resourceの生成・所有は担当しない。例外の捕捉や、借用先の寿命管理は行わない。

//========= Framework インクルード=========
#include "Framework/3D/H/ObjModelRenderer.h"

//========= Scene インクルード=========
#include "Scene/Common/Object/H/ModelRegistry.h"

//========= 初期化関数=========
// 設定をコピーし、種別に応じた共有Rendererを借用する。
bool SceneObject::Initialize(const OBJECT_CONFIG& config,ModelRegistry& registry)
{
	// 前回の借用ポインターを消し、失敗時に古いRendererが残らないようにする。
	m_cubeRenderer = nullptr;
	m_objRenderer = nullptr;

	// 種別・画像選択・見た目・初期Transformをコピーする。設定内のパス文字列は、Renderer取得後は保持しない。
	m_meshKind = config.meshKind;
	m_cubeTexture = config.cubeTexture;
	m_color = config.color;
	m_uvTiling = config.uvTiling;
	m_transform.SetPosition(config.position);
	m_transform.SetRotation(config.rotation);
	m_transform.SetScale(config.scale);

	// 形状の種別ごとに共有Rendererの取得方法を切り替え、取得に成功した場合だけtrueを返す。
	switch(config.meshKind){
		// Cubeは全Objectで共通の1個を借用する。Registry未初期化ならnullptrが返り、失敗として戻す。
	case OBJECT_MESH_KIND::e_CUBE:
	m_cubeRenderer = registry.GetCubeRenderer();
	return m_cubeRenderer != nullptr;

	// OBJはOBJと画像のパスの組で共有Rendererを取得する。取得できなければ失敗として戻す。
	case OBJECT_MESH_KIND::e_OBJ:
	// OBJパスがなければ読み込む対象がないため、Registryへ問い合わせず失敗として戻す。
	if(config.modelPath == nullptr)return false;

	// 画像パスがnullptrなら空文字列として扱い、単色のRendererを取得する。
	m_objRenderer = registry.AcquireObjModel(config.modelPath,config.texturePath != nullptr ? config.texturePath : L"");
	return m_objRenderer != nullptr;
	}

	// 列挙範囲外の値は未対応の種別として失敗とする。
	return false;
}

//========= 描画関数=========
// 現在のTransformから求めたWorld行列と色を、種別に応じた共有Rendererへ渡す。
void SceneObject::Draw(GraphicsSystem& graphicsSystem,const XMMATRIX& viewMatrix,const XMMATRIX& projectionMatrix) const
{
	// Scale・Rotation・Translationの順に合成したLocal→World行列を、描画ごとに求める。GPU用の転置は各Rendererが行う。
	const XMMATRIX world_matrix = m_transform.GetWorldMatrix();

	// 形状の種別ごとに、借用しているRendererの描画関数を切り替える。
	switch(m_meshKind){
		// Cubeは画像選択とUV倍率を渡して描画する。
	case OBJECT_MESH_KIND::e_CUBE:
	// Rendererを借用できていなければ、Initialize失敗または未実行のため描画しない。
	if(m_cubeRenderer == nullptr)return;

	m_cubeRenderer->DrawCube(graphicsSystem,world_matrix,viewMatrix,projectionMatrix,m_color,m_uvTiling,m_cubeTexture);
	break;

	// OBJは色だけを渡して描画する。UV倍率は現行ObjModelRendererが固定値(1,1)を使うため渡さない。
	case OBJECT_MESH_KIND::e_OBJ:
	// Rendererを借用できていなければ、Initialize失敗または未実行のため描画しない。
	if(m_objRenderer == nullptr)return;

	m_objRenderer->Draw(graphicsSystem,world_matrix,viewMatrix,projectionMatrix,m_color);
	break;
	}
}
