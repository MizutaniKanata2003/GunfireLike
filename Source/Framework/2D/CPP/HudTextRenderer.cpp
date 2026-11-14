#include "../H/HudRenderer.h"

// 単位Quadの不変BufferとHUD Shaderを所有し、各描画で矩形・色・描画先サイズだけを定数転送する。
// Virtual Canvasの配置計算、CPU転送型とHLSLの対応、Bind/Blendの副作用と失敗経路を明示する。

//========= C++標準ライブラリ インクルード=========
#include <algorithm>
#include <array>
#include <string>
#include <vector>

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/DirectX/H/ShaderBinaryLoader.h"
#include "Framework/Etc/H/Logger.h"

// 1280x720基準のHUD配置・外観・Shaderパスと、内部用のGPU生成失敗ログをまとめる。
namespace
{
	//========= 画面サイズ定数=========
	// HUD描画の基準画面サイズ。
	constexpr float ns_SCREEN_WIDTH = 1280.0f;
	constexpr float ns_SCREEN_HEIGHT = 720.0f;

	//========= 照準描画定数=========
	// 画面中央へ表示する照準の位置とサイズ。
	constexpr float ns_CROSSHAIR_CENTER_X = ns_SCREEN_WIDTH * 0.5f;
	constexpr float ns_CROSSHAIR_CENTER_Y = ns_SCREEN_HEIGHT * 0.5f;
	constexpr float ns_CROSSHAIR_LINE_LENGTH = 22.0f;
	constexpr float ns_CROSSHAIR_LINE_THICKNESS = 3.0f;

	//========= HPバー描画定数=========
	// プレイヤーHPバーの位置、サイズ、内側余白。
	constexpr float ns_HP_BAR_POSITION_X = 36.0f;
	constexpr float ns_HP_BAR_POSITION_Y = 650.0f;
	constexpr float ns_HP_BAR_WIDTH = 300.0f;
	constexpr float ns_HP_BAR_HEIGHT = 26.0f;
	constexpr float ns_HP_BAR_BORDER = 4.0f;

	//========= 低HP警告定数=========
	// 低HP判定の割合と画面端の警告枠の太さ。
	constexpr float ns_LOW_HP_RATIO = 0.25f;
	constexpr float ns_LOW_HP_BORDER_THICKNESS = 12.0f;

	//========= Alpha定数=========
	// HUD描画に使用する不透明度。
	constexpr float ns_FULLY_OPAQUE_ALPHA = 1.0f;
	constexpr float ns_HUD_FOREGROUND_ALPHA = 0.95f;
	constexpr float ns_HUD_BACKGROUND_ALPHA = 0.70f;
	constexpr float ns_WARNING_ALPHA = 0.28f;

	//========= Shaderファイルパス定数=========
	// HUD描画に使用するShaderのCSOファイルパス。
	constexpr const char* ns_HUD_VERTEX_SHADER_PATH = "Shaders/HudVS.cso";
	constexpr const char* ns_HUD_PIXEL_SHADER_PATH = "Shaders/HudPS.cso";

	//========= 補助関数=========
	// GraphicsカテゴリへHUD Resource生成失敗を記録する。
	// functionName: 失敗したAPIの有効な終端付きワイド文字列。nullptr不可。
	// result: 16進数8桁で表示するHRESULT。
	void WriteHUDGraphicsError(const wchar_t* functionName,HRESULT result)
	{
		// API名へ失敗値を連結する。Resource解放はこのログ関数ではなく、呼出元の失敗経路が行う。
		std::wstring message{functionName};
		message += L" に失敗しました。HRESULT: 0x";

		// 4bitずつ上位から抽出するためunsigned表現へ変換し、8桁の0埋めに相当する表示を作る。
		constexpr wchar_t HEX_DIGITS[]{L"0123456789ABCDEF"};
		const unsigned long result_value = static_cast<unsigned long>(result);

		for(int digit_index = 7; digit_index >= 0; --digit_index){
			const unsigned long digit = (result_value >> digit_index * 4) & 0x0f;
			message += HEX_DIGITS[digit];
		}

		// 生成済みの文字列を既存の分類へ出力する。ログ生成の例外捕捉は追加しない。
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_GRAPHICS,message);
	}
}

//========= 初期化関数=========
// CSO読込、Shader・Layout、不変Quad Buffer、矩形転送Bufferの順に準備する。
bool HudRenderer::Initialize(GraphicsSystem& graphicsSystem)
{
	// GPUリソースの生成に使用するDirect3D Deviceを取得する。
	// Deviceは借用する。この関数の先頭にUninitはなく、再生成前の解放は呼出側の事前条件。
	// Deviceなしではそのままfalseへ戻り、この経路に既存Resourceの解放は追加しない。
	ID3D11Device* device = graphicsSystem.GetDevice();
	if(device == nullptr)return false;

	// Vertex ShaderとPixel ShaderのCSOデータを読み込む。
	std::vector<char> vertex_shader_data{};
	std::vector<char> pixel_shader_data{};

	if(!ShaderBinaryLoader::Load(ns_HUD_VERTEX_SHADER_PATH,vertex_shader_data)){
		Uninit();
		return false;
	}

	if(!ShaderBinaryLoader::Load(ns_HUD_PIXEL_SHADER_PATH,pixel_shader_data)){
		Uninit();
		return false;
	}

	// HUD Quadの描画に使用するVertex Shaderを生成する。
	// HLSL本文ではなく、読み込んだCSOの先頭とバイト数からVSを生成する。動的リンクのClassLinkageは指定しない。
	// 以後の検出失敗ではUninitで部分解放する。下位処理やメモリ確保の例外はここでは捕捉しない。
	const HRESULT vertex_shader_result = device->CreateVertexShader(vertex_shader_data.data(),vertex_shader_data.size(),
																																	nullptr,m_vertexShader.GetAddressOf());

	if(FAILED(vertex_shader_result)){
		WriteHUDGraphicsError(L"ID3D11Device::CreateVertexShader",vertex_shader_result);
		Uninit();
		return false;
	}

	// HUD Quadの描画に使用するPixel Shaderを生成する。
	// HudPSのバイナリからPSを生成する。実際のShader選択と定数BufferのBindはDrawQuadで行う。
	const HRESULT pixel_shader_result = device->CreatePixelShader(pixel_shader_data.data(),pixel_shader_data.size(),
																																nullptr,m_pixelShader.GetAddressOf());

	if(FAILED(pixel_shader_result)){
		WriteHUDGraphicsError(L"ID3D11Device::CreatePixelShader",pixel_shader_result);
		Uninit();
		return false;
	}

	// 頂点座標をVertex Shader入力へ対応付けるInput Layoutを生成する。
	// HUDは3Dのfloat3ではなくfloat2のPOSITION0を使う。Slot 0・Offset 0の頂点単位データとして読む。
	// PER_VERTEX_DATAとStepRate 0で、Instanceごとの属性にはしない。頂点間隔のStrideは描画時に別途指定する。
	const D3D11_INPUT_ELEMENT_DESC input_elements[]
	{
		{"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0}
	};

	// VSの入力signatureと上の記述を対応付ける。CSO配列はLayout生成が終わるまで保持する。
	const HRESULT input_layout_result = device->CreateInputLayout(input_elements,ARRAYSIZE(input_elements),
																															 vertex_shader_data.data(),vertex_shader_data.size(),m_inputLayout.GetAddressOf());

	if(FAILED(input_layout_result)){
		WriteHUDGraphicsError(L"ID3D11Device::CreateInputLayout",input_layout_result);
		Uninit();
		return false;
	}

	// 0.0から1.0の範囲で表すHUD Quadの頂点を作成する。
	// 頂点は矩形の実pixel位置ではなく(0,0)から(1,1)の単位Quad。Shaderが定数の位置とサイズを適用する。
	// 各Quadで頂点Bufferを作り直さずに済む構成であり、位置・色などの可変値はHUD_BUFFERへ分離する。
	const std::array<VERTEX,4> vertices
	{
		VERTEX{XMFLOAT2{0.0f,0.0f}},
		VERTEX{XMFLOAT2{1.0f,0.0f}},
		VERTEX{XMFLOAT2{1.0f,1.0f}},
		VERTEX{XMFLOAT2{0.0f,1.0f}}
	};

	// HUD QuadのVertex Bufferを生成する。
	// ByteWidthは頂点数ではなくsizeof(VERTEX)*4の総バイト数。IMMUTABLEなので生成時に初期データを渡す。
	// BindFlagsはVertex用途、ゼロ初期化したCPUAccessFlags等は0を維持する。作成後にこのBufferは更新しない。
	D3D11_BUFFER_DESC vertex_buffer_description{};
	vertex_buffer_description.ByteWidth = static_cast<UINT>(sizeof(VERTEX) * vertices.size());
	vertex_buffer_description.Usage = D3D11_USAGE_IMMUTABLE;
	vertex_buffer_description.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	// CPU配列の所有は渡さず、CreateBuffer呼出中の読取元として先頭を渡す。Buffer用途のPitchは0を維持する。
	D3D11_SUBRESOURCE_DATA vertex_data{};
	vertex_data.pSysMem = vertices.data();

	const HRESULT vertex_buffer_result = device->CreateBuffer(&vertex_buffer_description,&vertex_data,m_vertexBuffer.GetAddressOf());
	if(FAILED(vertex_buffer_result)){
		WriteHUDGraphicsError(L"ID3D11Device::CreateBuffer(VertexBuffer)",vertex_buffer_result);
		Uninit();
		return false;
	}

	// HUD Quadを2つの三角形として描画するIndex配列を作成する。
	// 4頂点を(0,1,2)と(0,2,3)の2三角形として読む。Indexの順序と面の向きは変更しない。
	const std::array<unsigned short,6> indices{0,1,2,0,2,3};

	// HUD QuadのIndex Bufferを生成する。
	// unsigned shortの6要素分を確保し、Draw時のR16_UINTへ対応させる。IndexもIMMUTABLEで生成後は変更しない。
	D3D11_BUFFER_DESC index_buffer_description{};
	index_buffer_description.ByteWidth = static_cast<UINT>(sizeof(unsigned short) * indices.size());
	index_buffer_description.Usage = D3D11_USAGE_IMMUTABLE;
	index_buffer_description.BindFlags = D3D11_BIND_INDEX_BUFFER;

	// CPU側Index配列を初期データとして渡す。初期化が戻った後はローカル配列を保持し続ける必要はない。
	D3D11_SUBRESOURCE_DATA index_data{};
	index_data.pSysMem = indices.data();

	const HRESULT index_buffer_result = device->CreateBuffer(&index_buffer_description,&index_data,m_indexBuffer.GetAddressOf());
	if(FAILED(index_buffer_result)){
		WriteHUDGraphicsError(L"ID3D11Device::CreateBuffer(IndexBuffer)",index_buffer_result);
		Uninit();
		return false;
	}

	// Quadの画面座標、サイズ、色を渡す定数バッファを生成する。
	// 48バイトの転送型を持つDEFAULT定数Bufferを作る。初期データはなく、各DrawQuadでUpdateする。
	// rectangle=float4、color=float4、renderSize=float2、padding=float2の順をHudVSのcbufferと一致させる。
	HRESULT HUD_buffer_result{};

	if(!m_HUDBuffer.Init(device,HUD_buffer_result)){
		WriteHUDGraphicsError(L"ID3D11Device::CreateBuffer(HudBuffer)",HUD_buffer_result);
		Uninit();
		return false;
	}

	// 全生成に成功した後だけ描画数を6へ確定する。失敗経路ではUninitにより描画数も0となる。
	m_indexCount = static_cast<unsigned int>(indices.size());

	return true;
}

//========= 描画関数=========
// 基準座標の矩形を実描画先へ変換し、単色QuadとしてGPUへ描画要求する。
void HudRenderer::DrawQuad(GraphicsSystem& graphicsSystem,float positionX,float positionY,float width,float height,
													 const XMFLOAT4& color)
{
	// 非正の矩形サイズは何もしない。HPが0のバー幅などもこの経路で描画を省略する。
	if(width <= 0.0f || height <= 0.0f)return;

	// 描画に使用するDirect3D ContextとRender Targetサイズを確認する。
	ID3D11DeviceContext* context = graphicsSystem.GetContext();
	const unsigned int render_width = graphicsSystem.GetRenderWidth();
	const unsigned int render_height = graphicsSystem.GetRenderHeight();
	if(context == nullptr || render_width == 0 || render_height == 0 ||
		 !m_vertexShader || !m_pixelShader || !m_inputLayout ||
		 !m_vertexBuffer || !m_indexBuffer || !m_HUDBuffer.IsValid())return;

	// Virtual Canvas座標とサイズを現在のRender Target座標へ変換する。
	// Canvasは基準領域を収める均一倍率と中央寄せOffsetを計算する。位置にはOffsetを足し、サイズには倍率だけを使う。
	// ここでRender Targetのpixel座標へ変換済みなので、ShaderへVirtual Canvas座標をそのまま渡さない。
	const HudCanvas HUD_canvas{render_width,render_height};
	const XMFLOAT2 screen_position = HUD_canvas.ToScreenPosition(XMFLOAT2{positionX,positionY});
	const XMFLOAT2 screen_size = HUD_canvas.ToScreenSize(XMFLOAT2{width,height});

	// Shaderへ渡すQuadの画面座標、サイズ、色、Render Targetサイズをまとめる。
	// rectangle.xy=実pixel左上、zw=実pixel幅高さ。renderSizeはCanvas領域ではなくRender Target全体のサイズ。
	// HudVSはpixel/renderSizeを使ってClip座標へ変換し、Yを反転する。CPUでは行列やtransposeを用意しない。
	// paddingは0のまま転送し、CPU型とHLSLの48バイトの配置を維持する。
	const HUD_BUFFER HUD_buffer
	{
		XMFLOAT4{screen_position.x,screen_position.y,screen_size.x,screen_size.y},
		color,
		XMFLOAT2{static_cast<float>(render_width),static_cast<float>(render_height)},
		XMFLOAT2{}
	};

	// Input Assemblerへ設定する頂点Buffer情報をまとめる。
	// Strideはfloat2頂点一つ分のバイト間隔、Offset 0はBuffer先頭。配列内ポインターは借用で、COM所有はメンバーが持つ。
	const UINT stride = sizeof(VERTEX);
	const UINT offset{};
	ID3D11Buffer* vertex_buffers[]{m_vertexBuffer.Get()};

	// Shaderへ設定する定数Bufferをまとめる。
	ID3D11Buffer* constant_buffers[]{m_HUDBuffer.Get()};

	// 今回の矩形・色でBuffer全体を更新する。UpdateだけではShader slotへBindされず、実行成否の戻り値もない。
	m_HUDBuffer.Update(context,HUD_buffer);

	// IAのLayout・頂点・16bit Index・Triangle Listを設定し、単位Quadの6Indexを読み取る構成へ揃える。
	context->IASetInputLayout(m_inputLayout.Get());
	context->IASetVertexBuffers(0,1,vertex_buffers,&stride,&offset);
	context->IASetIndexBuffer(m_indexBuffer.Get(),DXGI_FORMAT_R16_UINT,0);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// VS/PSをHUD用へ切り替える。VSのb0は3D描画のCamera等を置き換えるため、後続の描画側で必要なBindを行う。
	// PS b0にも既存どおりBindするが、現行HudPSは定数Bufferを参照せず、VSから受け取った色を出力する。
	context->VSSetShader(m_vertexShader.Get(),nullptr,0);
	context->PSSetShader(m_pixelShader.Get(),nullptr,0);
	context->VSSetConstantBuffers(0,1,constant_buffers);
	context->PSSetConstantBuffers(0,1,constant_buffers);

	// Alpha<1だけを条件としてBlendを切り替える。色・AlphaのclampやDepth/Rasterizer設定は追加しない。
	// 不透明QuadではBlendを無効にするため、半透明描画から状態が変化することも呼出側は考慮する。
	graphicsSystem.SetAlphaBlendEnabled(color.w < ns_FULLY_OPAQUE_ALPHA);

	// Index数6・先頭Index0・頂点Base0で描画する。Shader・IA・b0・Blendは描画後も残し、以前の状態へ復元しない。
	context->DrawIndexed(m_indexCount,0,0);
}

// 画面中央に照準を描画する。
void HudRenderer::DrawCrosshair(GraphicsSystem& graphicsSystem)
{
	// 横線と縦線で構成する照準の色を設定する。
	// 照準は単位Quadを横線・縦線として使い、基準画面中央から半分の寸法を引いて左上位置を作る。
	const XMFLOAT4 crosshair_color{1.0f,1.0f,1.0f,ns_HUD_FOREGROUND_ALPHA};

	// 画面中央へ横線と縦線のQuadを描画する。
	DrawQuad(graphicsSystem,ns_CROSSHAIR_CENTER_X - ns_CROSSHAIR_LINE_LENGTH * 0.5f,ns_CROSSHAIR_CENTER_Y - ns_CROSSHAIR_LINE_THICKNESS * 0.5f,
					 ns_CROSSHAIR_LINE_LENGTH,ns_CROSSHAIR_LINE_THICKNESS,crosshair_color);

	DrawQuad(graphicsSystem,ns_CROSSHAIR_CENTER_X - ns_CROSSHAIR_LINE_THICKNESS * 0.5f,ns_CROSSHAIR_CENTER_Y - ns_CROSSHAIR_LINE_LENGTH * 0.5f,
					 ns_CROSSHAIR_LINE_THICKNESS,ns_CROSSHAIR_LINE_LENGTH,crosshair_color);
}

// 現在HPと最大HPに応じたプレイヤーHPバーを描画する。
void HudRenderer::DrawPlayerHealthBar(GraphicsSystem& graphicsSystem,float currentHP,float maxHP)
{
	// 最大HPが非正なら比率を計算せず戻る。現在HPの表示割合だけは後段で0から1へ制限する。
	if(maxHP <= 0.0f)return;

	// 現在HPを0.0から1.0の表示割合へ変換する。
	const float HP_ratio = std::clamp(currentHP / maxHP,0.0f,ns_FULLY_OPAQUE_ALPHA);

	// HPバーの背景を描画する。
	DrawQuad(graphicsSystem,ns_HP_BAR_POSITION_X,ns_HP_BAR_POSITION_Y,ns_HP_BAR_WIDTH,ns_HP_BAR_HEIGHT,XMFLOAT4{0.0f,0.0f,0.0f,ns_HUD_BACKGROUND_ALPHA});

	// 現在HPに応じたHPバーの幅と色を決定する。
	// 左右にBorder分を残す内幅へHP割合を掛ける。25%以下では警告色を選び、背景→内側バーの順序を保つ。
	const float health_width = (ns_HP_BAR_WIDTH - ns_HP_BAR_BORDER * 2.0f) * HP_ratio;
	const XMFLOAT4 health_color = HP_ratio > ns_LOW_HP_RATIO ?
		XMFLOAT4{0.10f,0.95f,0.20f,ns_HUD_FOREGROUND_ALPHA} :
		XMFLOAT4{ns_FULLY_OPAQUE_ALPHA,0.15f,0.10f,ns_HUD_FOREGROUND_ALPHA};

	// HPバーの現在値を背景の内側へ描画する。
	DrawQuad(graphicsSystem,ns_HP_BAR_POSITION_X + ns_HP_BAR_BORDER,ns_HP_BAR_POSITION_Y + ns_HP_BAR_BORDER,health_width,ns_HP_BAR_HEIGHT - ns_HP_BAR_BORDER * 2.0f,health_color);
}

// HPが低い場合に画面端の警告枠を描画する。
void HudRenderer::DrawLowHealthWarning(GraphicsSystem& graphicsSystem,float currentHP,float maxHP)
{
	// ||の短絡評価で最大HPが非正なら除算しない。HPバーとは異なり、比率のclampは行わず閾値と比較する。
	if(maxHP <= 0.0f || currentHP / maxHP > ns_LOW_HP_RATIO)return;

	// 画面端へ描画する半透明の赤い警告色を設定する。
	const XMFLOAT4 warning_color{ns_FULLY_OPAQUE_ALPHA,0.0f,0.0f,ns_WARNING_ALPHA};

	// 上・下・左・右の順に基準Canvas四辺へ描く。余白込みのRender Target外周へ自動拡張する構成ではない。
	// 四隅でQuadが重なる既存構成も維持し、重複描画を除去する変更は混ぜない。
	DrawQuad(graphicsSystem,0.0f,0.0f,ns_SCREEN_WIDTH,ns_LOW_HP_BORDER_THICKNESS,warning_color);
	DrawQuad(graphicsSystem,0.0f,ns_SCREEN_HEIGHT - ns_LOW_HP_BORDER_THICKNESS,ns_SCREEN_WIDTH,ns_LOW_HP_BORDER_THICKNESS,warning_color);
	DrawQuad(graphicsSystem,0.0f,0.0f,ns_LOW_HP_BORDER_THICKNESS,ns_SCREEN_HEIGHT,warning_color);
	DrawQuad(graphicsSystem,ns_SCREEN_WIDTH - ns_LOW_HP_BORDER_THICKNESS,0.0f,ns_LOW_HP_BORDER_THICKNESS,ns_SCREEN_HEIGHT,warning_color);
}

//========= 終了関数=========
// 所有COM参照とIndex数を解放済み状態へ戻す。
void HudRenderer::Uninit()
{
	// HUD描画に使用したGPU Bufferを解放する。
	// 所有者の参照を解放する。ContextがBindで保持する参照は別であり、この処理はUnbindやGPU完了待機ではない。
	m_HUDBuffer.Uninit();
	m_indexBuffer.Reset();
	m_vertexBuffer.Reset();

	// HUD描画に使用したShaderとInput Layoutを解放する。
	m_inputLayout.Reset();
	m_pixelShader.Reset();
	m_vertexShader.Reset();

	// 所有Bufferと描画数を対応させ、解放後に描画カウントだけを残さない。
	m_indexCount = {};
}