#pragma once

// 設定表から生成する3D Object1個分の状態を保持する。Transform・色・UV倍率と、共有Rendererへの借用ポインターを持つ。
// GPU Resourceは所有しない。Rendererの所有はModelRegistryが担当し、このObjectは描画時に借用先へWorld行列と色を渡す。
// ModelRegistryが解放または移動された後は、借用ポインターが無効になるため、先にObjectを破棄すること。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

//========= Scene インクルード=========
#include "Scene/Common/Object/H/Transform.h"
#include "Scene/Common/Object/H/ObjectConfig.h"

//========= 前方宣言=========
class GraphicsSystem;
class ModelRegistry;
class ObjModelRenderer;

// 位置・回転・倍率・色を持ち、共有Rendererで描画される3D Object。
class SceneObject final
{
public:
	//========= 初期化関数=========
	// 設定から状態をコピーし、種別に応じた共有Rendererを借用する。
	// config: 生成設定。参照と、設定内の文字列ポインターは保持しない。OBJの場合はmodelPathが必須。
	// registry: 共有Rendererの取得先。Initialize済みであること。このObjectより長く生存させる。
	// 戻り値: Rendererを借用できればtrue。未初期化のRegistry、OBJパスなし、OBJ生成失敗ではfalse。
	// 失敗時もTransformや色は設定済みで、借用ポインターだけが空になる。失敗したObjectは描画しても何も行わない。
	bool Initialize(const OBJECT_CONFIG& config,ModelRegistry& registry);

	//========= 描画関数=========
	// 種別に応じた共有Rendererへ、現在のWorld行列と色を渡して描画する。Transformは変更しない。
	// graphicsSystem: RendererがContextなどを取得する描画基盤。非所有で、参照は保持しない。
	// viewMatrix: World空間からView空間への変換行列。参照は保持しない。
	// projectionMatrix: View空間からClip空間への変換行列。参照は保持しない。
	// 借用ポインターがない場合は何もしない。Render Passなどの描画Stateは呼出側が設定する。
	void Draw(GraphicsSystem& graphicsSystem,const XMMATRIX& viewMatrix,const XMMATRIX& projectionMatrix) const;

	//========= Getter関数=========
	// Transformへの参照を返す。位置・回転・倍率を後から変更するために使う。参照先はこのObjectの寿命まで有効。
	[[nodiscard]] Transform& GetTransform() { return m_transform; }
	// Transformへの読み取り専用参照を返す。参照先はこのObjectの寿命まで有効。
	[[nodiscard]] const Transform& GetTransform() const { return m_transform; }

private:
	//========= 描画設定=========
	// 描画に使う形状の種別。既定はCube。
	OBJECT_MESH_KIND m_meshKind{OBJECT_MESH_KIND::e_CUBE};
	// Cube描画時の画像選択。既定は画像なしの単色。
	BasicMeshRenderer::TextureType m_cubeTexture{BasicMeshRenderer::TextureType::Color};
	// 乗算するRGBA色。既定は不透明な白。
	XMFLOAT4 m_color{1.0f,1.0f,1.0f,1.0f};
	// Cube描画時のUV倍率。OBJ描画では使用しない。
	XMFLOAT2 m_uvTiling{1.0f,1.0f};

	//========= 配置=========
	// 位置・回転・倍率を保持し、描画時のWorld行列を生成するTransform。
	Transform m_transform{};

	//========= 借用するRenderer=========
	// Cube描画に使う共有Renderer。非所有で、ModelRegistryが所有する。未取得はnullptr。
	BasicMeshRenderer* m_cubeRenderer{};
	// OBJ描画に使う共有Renderer。非所有で、ModelRegistryが所有する。未取得はnullptr。
	ObjModelRenderer* m_objRenderer{};
};
