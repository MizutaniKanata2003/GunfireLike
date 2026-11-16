#include "../H/HudRenderer.h"

// 固定Quadの生成・解放と、Canvas変換→矩形転送→Bind→Blend設定→DrawIndexedを実装する。
// 配置・色・判定・生成失敗時の終了順は維持し、描画状態の復元や機能追加は行わない。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::clampでHPバーの表示比率を0〜1へ制限する。
#include <array> // 単位Quadの4頂点と6Indexを、固定要素数の連続配列に保持する。
#include <string> // std::wstringで生成失敗のAPI名とHRESULT表示を組み立てる。
#include <vector> // std::vector<char>でVS・PSのCSOバイト列を読み込む。

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/DirectX/H/ShaderBinaryLoader.h"
#include "Framework/Etc/H/Logger.h"

// Canvas基準の配置と外観・CSOパス、およびGPU生成失敗のログ補助をまとめる。
namespace
{
	//========= 画面サイズ定数=========
	// 配置計算に使う基準幅。Canvas座標単位で、実Render Target幅ではない。
	constexpr float ns_SCREEN_WIDTH = 1280.0f;
	// 基準高さ720。HudCanvasの基準領域と対応する既存値。
	constexpr float ns_SCREEN_HEIGHT = 720.0f;

	//========= 照準描画定数=========
	// 基準幅の半分を照準中央Xとする。
	constexpr float ns_CROSSHAIR_CENTER_X = ns_SCREEN_WIDTH * 0.5f;
	// 基準高さの半分を照準中央Yとする。
	constexpr float ns_CROSSHAIR_CENTER_Y = ns_SCREEN_HEIGHT * 0.5f;
	// 照準線の長さ22。単位はCanvas基準座標。
	constexpr float ns_CROSSHAIR_LINE_LENGTH = 22.0f;
	// 照準線の太さ3。Canvas倍率はDrawQuadで適用する。
	constexpr float ns_CROSSHAIR_LINE_THICKNESS = 3.0f;

	//========= HPバー描画定数=========
	// 背景左上X=36。左端からのCanvas基準位置。
	constexpr float ns_HP_BAR_POSITION_X = 36.0f;
	// 背景左上Y=650。下向きを正とするCanvas基準位置。
	constexpr float ns_HP_BAR_POSITION_Y = 650.0f;
	// 背景幅300。現在値の内幅は左右余白分を引く。
	constexpr float ns_HP_BAR_WIDTH = 300.0f;
	// 背景高さ26。内側の高さは上下余白分を引く。
	constexpr float ns_HP_BAR_HEIGHT = 26.0f;
	// 各辺の内側余白4。両側分として寸法から2倍を引く。
	constexpr float ns_HP_BAR_BORDER = 4.0f;

	//========= 低HP警告定数=========
	// 低HP閾値25%。HPバーの色と警告枠に使用する。
	constexpr float ns_LOW_HP_RATIO = 0.25f;
	// Canvas四辺の警告枠の太さ12。四隅の重複は除去しない。
	constexpr float ns_LOW_HP_BORDER_THICKNESS = 12.0f;

	//========= Alpha定数=========
	// 不透明のAlpha1。既存コードではHP比率の上限1にも使用する。
	constexpr float ns_FULLY_OPAQUE_ALPHA = 1.0f;
	// 照準・現在HPのAlpha0.95。既存外観を維持する。
	constexpr float ns_HUD_FOREGROUND_ALPHA = 0.95f;
	// HP背景のAlpha0.70。前景より先に描く。
	constexpr float ns_HUD_BACKGROUND_ALPHA = 0.70f;
	// 赤い警告枠のAlpha0.28。個別Quadに適用する。
	constexpr float ns_WARNING_ALPHA = 0.28f;

	//========= Shaderファイルパス定数=========
	// コンパイル済みHudVSの相対パス。実行時作業ディレクトリを基準に読み、ここでHLSLをコンパイルしない。
	constexpr const char* ns_HUD_VERTEX_SHADER_PATH = "Shaders/HudVS.cso";
	// コンパイル済みHudPSの相対パス。位置・色の転送仕様は対応Shaderと維持する。
	constexpr const char* ns_HUD_PIXEL_SHADER_PATH = "Shaders/HudPS.cso";

	//========= 補助関数=========
	// API名と16進数8桁のHRESULTをGraphics分類で出力する。
	// functionName: 呼出中に有効な終端付きワイド文字列。nullptr不可。
	// result: 表示する生成失敗値。Resource解放は呼出元で行う。
	void WriteHUDGraphicsError(const wchar_t* functionName,HRESULT result)
	{
		// API名と16進表記の接頭辞を連結する。ログ用メモリ確保の例外は捕捉しない。
		std::wstring message{functionName};
		message += L" に失敗しました。HRESULT: 0x";

		// 4bitの値0〜15を対応する16進数字へ変換する表と、unsigned表現を用意する。
		constexpr wchar_t HEX_DIGITS[]{L"0123456789ABCDEF"};
		const unsigned long result_value = static_cast<unsigned long>(result);
		// 桁番号7から0へ走査し、上位から8桁を先頭0も含めて出力する。
		for(int digit_index = 7; digit_index >= 0; --digit_index){
			// 対象の4bitを最下位へ移し、マスクでその桁だけを抽出する。
			const unsigned long digit = (result_value >> digit_index * 4) & 0x0f;
			message += HEX_DIGITS[digit]; // 抽出値に対応する数字をログ末尾へ加える。
		}

		// 生成対象を示す完成ログを出力する。解放・復旧処理をログ関数へ移さない。
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_GRAPHICS,message);
	}
}

//========= 初期化関数=========
// Shader・Layout・単位Quad用Buffer・可変の定数Bufferを順に生成する。
bool HudRenderer::Initialize(GraphicsSystem& graphicsSystem)
{
	// Deviceを借用する。先頭でUninitしないため、再初期化前の解放は利用側の前提。
	ID3D11Device* device = graphicsSystem.GetDevice();
	// Device未取得ではそのままfalseを返す。この分岐では既存Resourceを解放しない。
	if(device == nullptr)return false;

	// CSOの先頭とバイト長を生成APIへ渡せる連続領域に保持する。
	std::vector<char> vertex_shader_data{};
	std::vector<char> pixel_shader_data{};
	// VSバイナリの読込失敗なら、所有Resourceを部分解放して戻る。例外の捕捉は追加しない。
	if(!ShaderBinaryLoader::Load(ns_HUD_VERTEX_SHADER_PATH,vertex_shader_data)){
		Uninit();
		return false;
	}
	// PSバイナリの読込失敗でもUninitする。読込済みCPU配列はローカルの寿命で解放する。
	if(!ShaderBinaryLoader::Load(ns_HUD_PIXEL_SHADER_PATH,pixel_shader_data)){
		Uninit();
		return false;
	}

	// CSOからVSを生成する。ClassLinkageは指定せず、所有先はメンバーComPtr。
	const HRESULT vertex_shader_result = device->CreateVertexShader(vertex_shader_data.data(),vertex_shader_data.size(),nullptr,m_vertexShader.GetAddressOf());
	// VS生成失敗ならAPI名と結果を記録し、部分Resourceを解放してfalseを返す。
	if(FAILED(vertex_shader_result)){
		WriteHUDGraphicsError(L"ID3D11Device::CreateVertexShader",vertex_shader_result);
		Uninit();
		return false;
	}

	// CSOからPSを生成する。ShaderのBindと定数接続は描画時に別途行う。
	const HRESULT pixel_shader_result = device->CreatePixelShader(pixel_shader_data.data(),pixel_shader_data.size(),nullptr,m_pixelShader.GetAddressOf());
	// PS生成失敗では、先に生成したVSも含めてUninitする。
	if(FAILED(pixel_shader_result)){
		WriteHUDGraphicsError(L"ID3D11Device::CreatePixelShader",pixel_shader_result);
		Uninit();
		return false;
	}

	// POSITION0=float2をSlot0・Offset0から頂点単位で読む。StepRate0でInstance単位の属性にはしない。
	const D3D11_INPUT_ELEMENT_DESC input_elements[]
	{
		{"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0}
	};
	// VSの入力signatureとLayout記述を対応付ける。Strideは描画時に別途指定する。
	const HRESULT input_layout_result = device->CreateInputLayout(input_elements,ARRAYSIZE(input_elements),
																															 vertex_shader_data.data(),vertex_shader_data.size(),m_inputLayout.GetAddressOf());
	// Layout生成失敗なら、成功済みShaderを含む所有Resourceを解放して戻る。
	if(FAILED(input_layout_result)){
		WriteHUDGraphicsError(L"ID3D11Device::CreateInputLayout",input_layout_result);
		Uninit();
		return false;
	}

	// 左上から順に固定4頂点を作る。実pixel位置ではなく0〜1のQuadで、矩形の拡縮・移動はVSが行う。
	const std::array<VERTEX,4> vertices
	{
		VERTEX{XMFLOAT2{0.0f,0.0f}},
		VERTEX{XMFLOAT2{1.0f,0.0f}},
		VERTEX{XMFLOAT2{1.0f,1.0f}},
		VERTEX{XMFLOAT2{0.0f,1.0f}}
	};

	// ByteWidthは総バイト数。IMMUTABLEなので生成時に初期データを渡し、以後は頂点Bufferを更新しない。
	D3D11_BUFFER_DESC vertex_buffer_description{};
	vertex_buffer_description.ByteWidth = static_cast<UINT>(sizeof(VERTEX) * vertices.size());
	vertex_buffer_description.Usage = D3D11_USAGE_IMMUTABLE;
	vertex_buffer_description.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	// CPU配列の所有権は移さず読取元として渡す。Buffer用途なのでPitch類はゼロのまま。
	D3D11_SUBRESOURCE_DATA vertex_data{};
	vertex_data.pSysMem = vertices.data();
	const HRESULT vertex_buffer_result = device->CreateBuffer(&vertex_buffer_description,&vertex_data,m_vertexBuffer.GetAddressOf());
	// Vertex Buffer生成失敗なら、先行Shader等も含めて部分解放する。
	if(FAILED(vertex_buffer_result)){
		WriteHUDGraphicsError(L"ID3D11Device::CreateBuffer(VertexBuffer)",vertex_buffer_result);
		Uninit();
		return false;
	}

	// 4頂点を(0,1,2)と(0,2,3)の2三角形へ分割する。Index順と面の向きは維持する。
	const std::array<unsigned short,6> indices{0,1,2,0,2,3};

	// 16bit Indexを6個持つ不変Bufferを生成する。Draw時のR16_UINTと要素型を対応させる。
	D3D11_BUFFER_DESC index_buffer_description{};
	index_buffer_description.ByteWidth = static_cast<UINT>(sizeof(unsigned short) * indices.size());
	index_buffer_description.Usage = D3D11_USAGE_IMMUTABLE;
	index_buffer_description.BindFlags = D3D11_BIND_INDEX_BUFFER;
	D3D11_SUBRESOURCE_DATA index_data{};
	index_data.pSysMem = indices.data();
	const HRESULT index_buffer_result = device->CreateBuffer(&index_buffer_description,&index_data,m_indexBuffer.GetAddressOf());
	// Index Buffer生成失敗なら、作成済みVertex BufferやShaderも解放する。
	if(FAILED(index_buffer_result)){
		WriteHUDGraphicsError(L"ID3D11Device::CreateBuffer(IndexBuffer)",index_buffer_result);
		Uninit();
		return false;
	}

	// 可変の矩形情報用に48バイトの定数Bufferを生成する。Init時はCPU初期値をGPUへ転送せず、各描画でUpdateする。
	HRESULT HUD_buffer_result{};
	// 定数Buffer生成失敗ならHRESULTを記録し、QuadとShaderもUninitして戻る。
	if(!m_HUDBuffer.Init(device,HUD_buffer_result)){
		WriteHUDGraphicsError(L"ID3D11Device::CreateBuffer(HudBuffer)",HUD_buffer_result);
		Uninit();
		return false;
	}

	// 全生成後だけIndex数を6へ確定する。失敗経路ではUninitにより0へ戻る。
	m_indexCount = static_cast<unsigned int>(indices.size());
	return true;
}

//========= 描画関数=========
// Canvas矩形を実描画先へ変換し、固定Quadへ適用して描画する。
void HudRenderer::DrawQuad(GraphicsSystem& graphicsSystem,float positionX,float positionY,float width,float height,const XMFLOAT4& color)
{
	// 幅・高さが0以下なら描かない。HP0の現在値バーなどもこの条件で省略する。
	if(width <= 0.0f || height <= 0.0f)return;

	// 借用Contextと現在の描画先pixel寸法を取得する。
	ID3D11DeviceContext* context = graphicsSystem.GetContext();
	const unsigned int render_width = graphicsSystem.GetRenderWidth();
	const unsigned int render_height = graphicsSystem.GetRenderHeight();
	// 必須Resource・Context・正の描画先サイズが揃わなければ転送・Bindを行わない。値の有限性の追加検証はない。
	if(context == nullptr || render_width == 0 || render_height == 0 ||
		 !m_vertexShader || !m_pixelShader || !m_inputLayout ||
		 !m_vertexBuffer || !m_indexBuffer || !m_HUDBuffer.IsValid())return;

	// 位置にはCanvas倍率と中央寄せOffset、サイズには倍率だけを適用する。結果は実pixel単位。
	const HudCanvas HUD_canvas{render_width,render_height};
	const XMFLOAT2 screen_position = HUD_canvas.ToScreenPosition(XMFLOAT2{positionX,positionY});
	const XMFLOAT2 screen_size = HUD_canvas.ToScreenSize(XMFLOAT2{width,height});

	// rectangle.xyは左上、zwは寸法。renderSizeはCanvas領域ではなくRender Target全体のpixel寸法。
	// HudVSがpixel→正規化→Y反転したClip座標へ変換する。余白は0として48バイトの配置を保つ。
	const HUD_BUFFER HUD_buffer
	{
		XMFLOAT4{screen_position.x,screen_position.y,screen_size.x,screen_size.y},
		color,
		XMFLOAT2{static_cast<float>(render_width),static_cast<float>(render_height)},
		XMFLOAT2{}
	};

	// Strideは頂点1個のバイト間隔、Offset0は先頭読取。借用ポインター配列で、所有はメンバーに残る。
	const UINT stride = sizeof(VERTEX);
	const UINT offset{};
	ID3D11Buffer* vertex_buffers[]{m_vertexBuffer.Get()};

	// 各Shader Stageのb0へ渡す同じ定数Bufferを準備する。配列の要素数は1。
	ID3D11Buffer* constant_buffers[]{m_HUDBuffer.Get()};

	// 今回の矩形・色で全体更新する。UpdateだけではslotへBindされず、戻り値による成否通知もない。
	m_HUDBuffer.Update(context,HUD_buffer);

	// IAへfloat2頂点、16bit Index、Triangle Listを設定する。3Indexごとに1三角形として描画する。
	context->IASetInputLayout(m_inputLayout.Get());
	context->IASetVertexBuffers(0,1,vertex_buffers,&stride,&offset);
	context->IASetIndexBuffer(m_indexBuffer.Get(),DXGI_FORMAT_R16_UINT,0);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// VS/PSを切り替え、両Stageのb0を上書きする。HudVSはb0を読み、HudPSはVSから受け取る色だけを使う。
	// 3D Camera等の以前のBindは復元しない。後続描画が必要なStateを設定する。
	context->VSSetShader(m_vertexShader.Get(),nullptr,0);
	context->PSSetShader(m_pixelShader.Get(),nullptr,0);
	context->VSSetConstantBuffers(0,1,constant_buffers);
	context->PSSetConstantBuffers(0,1,constant_buffers);

	// Alpha<1ならBlend有効、それ以外は無効。Alpha0の省略や値のclamp、Depth/Cullの設定は追加しない。
	graphicsSystem.SetAlphaBlendEnabled(color.w < ns_FULLY_OPAQUE_ALPHA);

	// 保持する6Indexを先頭Index0・BaseVertex0で描く。IA・Shader・b0・Blendはこの後も残る。
	context->DrawIndexed(m_indexCount,0,0);
}

// 基準画面中央へ横線と縦線を重ねる。
void HudRenderer::DrawCrosshair(GraphicsSystem& graphicsSystem)
{
	// RGB1の白に前景Alphaを適用する。線の交点で2Quadが重なる既存構成を維持する。
	const XMFLOAT4 crosshair_color{1.0f,1.0f,1.0f,ns_HUD_FOREGROUND_ALPHA};

	// 中央から寸法の半分を引いて横線の左上を求める。
	DrawQuad(graphicsSystem,ns_CROSSHAIR_CENTER_X - ns_CROSSHAIR_LINE_LENGTH * 0.5f,ns_CROSSHAIR_CENTER_Y - ns_CROSSHAIR_LINE_THICKNESS * 0.5f,
					 ns_CROSSHAIR_LINE_LENGTH,ns_CROSSHAIR_LINE_THICKNESS,crosshair_color);

	// 長さと太さを入れ替えた縦線を後から描く。中心座標は横線と同じ。
	DrawQuad(graphicsSystem,ns_CROSSHAIR_CENTER_X - ns_CROSSHAIR_LINE_THICKNESS * 0.5f,ns_CROSSHAIR_CENTER_Y - ns_CROSSHAIR_LINE_LENGTH * 0.5f,
					 ns_CROSSHAIR_LINE_THICKNESS,ns_CROSSHAIR_LINE_LENGTH,crosshair_color);
}

// 背景と内側の現在値を順に描画する。
void HudRenderer::DrawPlayerHealthBar(GraphicsSystem& graphicsSystem,float currentHP,float maxHP)
{
	// 最大HPが0以下なら除算も背景描画も行わない。追加のNaN等の検証はしない。
	if(maxHP <= 0.0f)return;

	// 現在HP/最大HPを0〜1へ制限し、負HPや最大超過でバー幅が範囲外にならない表示比率にする。
	const float HP_ratio = std::clamp(currentHP / maxHP,0.0f,ns_FULLY_OPAQUE_ALPHA);

	// 黒い背景を先に描き、内側の現在値を後から重ねる。
	DrawQuad(graphicsSystem,ns_HP_BAR_POSITION_X,ns_HP_BAR_POSITION_Y,ns_HP_BAR_WIDTH,ns_HP_BAR_HEIGHT,XMFLOAT4{0.0f,0.0f,0.0f,ns_HUD_BACKGROUND_ALPHA});

	// 両側余白を引いた内幅へ比率を掛ける。25%より大きければ緑、それ以外は赤寄りの既存色。
	const float health_width = (ns_HP_BAR_WIDTH - ns_HP_BAR_BORDER * 2.0f) * HP_ratio;
	const XMFLOAT4 health_color = HP_ratio > ns_LOW_HP_RATIO ?
		XMFLOAT4{0.10f,0.95f,0.20f,ns_HUD_FOREGROUND_ALPHA} :
		XMFLOAT4{ns_FULLY_OPAQUE_ALPHA,0.15f,0.10f,ns_HUD_FOREGROUND_ALPHA};

	// 左上を余白分ずらし、上下余白分を引いた高さで描く。比率0の幅はDrawQuad側で省略される。
	DrawQuad(graphicsSystem,ns_HP_BAR_POSITION_X + ns_HP_BAR_BORDER,ns_HP_BAR_POSITION_Y + ns_HP_BAR_BORDER,
					 health_width,ns_HP_BAR_HEIGHT - ns_HP_BAR_BORDER * 2.0f,health_color);
}

// 低HP時に基準Canvas四辺へ半透明の赤い枠を描く。
void HudRenderer::DrawLowHealthWarning(GraphicsSystem& graphicsSystem,float currentHP,float maxHP)
{
	// 最大HPが非正なら短絡評価で除算しない。比率が25%より大きい場合も描かず、HPバーと違い比率clampはしない。
	if(maxHP <= 0.0f || currentHP / maxHP > ns_LOW_HP_RATIO)return;

	// RGB(1,0,0)の赤と警告Alphaを使う。値の点滅・時間変化は行わない。
	const XMFLOAT4 warning_color{ns_FULLY_OPAQUE_ALPHA,0.0f,0.0f,ns_WARNING_ALPHA};

	// 上→下→左→右の順に描く。四隅の重複を維持し、Render Target余白の外周へ広げる変更はしない。
	DrawQuad(graphicsSystem,0.0f,0.0f,ns_SCREEN_WIDTH,ns_LOW_HP_BORDER_THICKNESS,warning_color);
	DrawQuad(graphicsSystem,0.0f,ns_SCREEN_HEIGHT - ns_LOW_HP_BORDER_THICKNESS,ns_SCREEN_WIDTH,ns_LOW_HP_BORDER_THICKNESS,warning_color);
	DrawQuad(graphicsSystem,0.0f,0.0f,ns_LOW_HP_BORDER_THICKNESS,ns_SCREEN_HEIGHT,warning_color);
	DrawQuad(graphicsSystem,ns_SCREEN_WIDTH - ns_LOW_HP_BORDER_THICKNESS,0.0f,ns_LOW_HP_BORDER_THICKNESS,ns_SCREEN_HEIGHT,warning_color);
}

//========= 終了関数=========
// メンバーの所有参照を解放し、描画数も初期状態へ戻す。
void HudRenderer::Uninit()
{
	// 定数→Index→頂点Bufferの所有参照を解放する。Contextが持つBind参照の解除やGPU完了待機は行わない。
	m_HUDBuffer.Uninit();
	m_indexBuffer.Reset();
	m_vertexBuffer.Reset();

	// Layout→PS→VSの順に所有参照を解放する。描画Stateの復元はしない。
	m_inputLayout.Reset();
	m_pixelShader.Reset();
	m_vertexShader.Reset();

	// 全生成成功後のIndex数を残さず、0へ戻す。
	m_indexCount = {};
}