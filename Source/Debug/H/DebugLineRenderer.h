#pragma once

// 線分の組をDirect3D 11の線描画（Line List）で描画するRenderer。Collisionの形状など、デバッグ表示に使う。
// BasicColorのShader・定数Bufferは、BasicColorPipelineが所有する。Shaderは画像なし・Object色の単色経路を使う。
// 色は描画1回につき1色。Render Pass・Depth・Blendは呼出側が設定し、このクラスは変更しない。GraphicsSystemは借用し、保持しない。

//========= C++標準ライブラリ インクルード=========
#include <cstddef> // std::size_tで、頂点数と開始位置を扱う。
#include <vector> // 線分の端点の列を、呼出側から読み取り専用で受け取る。

//========= 外部ライブラリ インクルード=========
#include <d3d11.h> // Buffer・Contextの型と、Map・Draw・Bind用のAPIを使用する。

//========= Framework インクルード=========
#include "Framework/3D/H/BasicColorPipeline.h"
#include "Framework/3D/H/CameraMatrices.h"
#include "Framework/Etc/H/MathTypes.h"

//========= 前方宣言=========
class GraphicsSystem;

// 同じ色の線分をまとめて描画する。
class DebugLineRenderer final
{
public:
	//========= ライフサイクル・描画関数=========
	// 旧ResourceをUninitした後、頂点Bufferを生成し、BasicColorPipelineを初期化する。
	// graphicsSystem: 生成Deviceの取得元。Resourceと対応する描画基盤を渡す。参照は保持しない。
	// 戻り値: 全生成に成功すればtrue。検出した読込・生成の失敗では、部分Resourceを解放してfalse。
	// 旧状態へは復帰しない。下位処理の例外は捕捉しない。
	bool Initialize(GraphicsSystem& graphicsSystem);
	// 線分をまとめて描画する。2点ずつを1本の線分として扱う。
	// graphicsSystem: Contextの取得元。描画先・Viewport・Blend・Depth・Rasterizerは呼出側が設定する。
	// points: 線分の端点の列。[0]と[1]が1本目、[2]と[3]が2本目となる。ワールド座標。奇数個なら最後の1点は無視する。
	// color: 全線分に共通のRGBA色。値のclampはしない。
	// camera: View行列とProjection行列。view*projectionの順で合成する。
	// 必須Resourceがない、またはContextがない、または端点が2点未満なら何もしない。端点が多い場合は、Bufferの容量ごとに分けて描画する。
	// Bind状態（Topology・Shader・Layout・定数Buffer）は描画後も元へ戻さない。
	void DrawLines(GraphicsSystem& graphicsSystem,const std::vector<XMFLOAT3>& points,const XMFLOAT4& color,const CAMERA_MATRICES& camera);
	// メンバーの所有参照を解放する。未初期化・部分初期化・終了済みでも呼出可能。
	// ContextのBind解除・状態復元・GPU完了待機は行わない。
	void Uninit();

private:
	//========= 描画の補助関数=========
	// 頂点Bufferへ、端点の一部を書き込み、その分を描画する。
	// context: 描画に使う即時Context。
	// points: 線分の端点の列。
	// startIndex: 今回描画する先頭の端点の位置。
	// vertexCount: 今回描画する頂点数。偶数で、Bufferの容量以下であること。
	// 戻り値: 描画できたらtrue。Mapに失敗したら、ログへ記録してfalse。
	[[nodiscard]] bool DrawChunk(ID3D11DeviceContext* context,const std::vector<XMFLOAT3>& points,std::size_t startIndex,std::size_t vertexCount);

	//========= Pipeline関連=========
	// BasicColorのShader・Input Layout・定数Bufferを所有する。
	BasicColorPipeline m_pipeline{};

	//========= Buffer関連=========
	// 描画ごとにCPUから書き換える動的な頂点Bufferの所有参照。容量は最大頂点数分。
	ComPtr<ID3D11Buffer> m_vertexBuffer{};
};