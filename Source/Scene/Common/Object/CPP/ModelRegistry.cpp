#include "../H/ModelRegistry.h"

// Cube・OBJの共有Rendererの初期化・取得・解放を実装する。
// Rendererの描画やObjectの配置は担当しない。生成失敗は戻り値で返し、例外の捕捉や再試行は行わない。

//========= C++標準ライブラリ インクルード=========
#include <utility> // std::moveで生成済みOBJ Rendererのunique_ptrの所有をmapへ移す。

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"

//========= 初期化・終了関数=========
// 旧Resourceを解放した後、Cube用Rendererを初期化し、成功後に描画基盤を登録する。
bool ModelRegistry::Initialize(GraphicsSystem& graphicsSystem)
{
  // 旧状態を先に解放し、再初期化で前回のRendererや登録を残さない。以後の失敗でも旧状態へは復帰しない。
  Uninit();

  // Cube用Rendererの初期化失敗を検出し、部分Resourceを含めて解放してfalseを返す。描画基盤は登録しない。
  if(!m_cubeRenderer.Initialize(graphicsSystem)){
    Uninit();
    return false;
  }

  // 成功後にだけ描画基盤を登録し、以後のOBJ取得で同じDeviceを使えるようにする。
  m_graphicsSystem = &graphicsSystem;
  return true;
}

// 借用されているRendererを解放する。呼出前に、借用しているObjectを破棄または無効化しておく必要がある。
void ModelRegistry::Uninit()
{
  // OBJ Rendererを全て破棄し、各Rendererが所有するCOM参照を解放する。Context側の別参照は残り得る。
  m_objModels.clear();

  // Cube用Rendererの所有参照を解放し、未初期化と同じ状態へ戻す。
  m_cubeRenderer.Uninit();
  m_graphicsSystem = nullptr;
}

//========= 取得関数=========
// 初期化成功後だけCube用Rendererを借用させる。
BasicMeshRenderer* ModelRegistry::GetCubeRenderer()
{
  // 描画基盤の登録はInitialize成功後だけ行うため、nullptrなら未初期化または終了後として借用させない。
  if(m_graphicsSystem == nullptr)return nullptr;

  return &m_cubeRenderer;
}

// OBJと画像のパスの組から共有Rendererを取得し、未登録の組だけ生成する。
ObjModelRenderer* ModelRegistry::AcquireObjModel(const std::wstring& OBJFilePath,const std::wstring& textureFilePath)
{
  // 描画基盤の登録がなければRendererを生成できないため、未初期化として取得を断る。
  if(m_graphicsSystem == nullptr)return nullptr;

  // OBJと画像のパスを区切り文字で連結し、共有判定のKeyにする。パスが同じ組だけが同じRendererを共有する。
  const std::wstring key = OBJFilePath + L"|" + textureFilePath;
  const auto found = m_objModels.find(key);
  // 同じ組が登録済みなら、読込を省いて既存Rendererの借用ポインターを返す。
  if(found != m_objModels.end())return found->second.get();

  // 未登録の組のRendererを生成し、OBJ・画像・Shaderを読み込んで初期化する。
  std::unique_ptr<ObjModelRenderer> renderer = std::make_unique<ObjModelRenderer>();
  // OBJ Rendererの初期化失敗を検出したら、登録せずnullptrを返す。部分Resourceは初期化関数が解放し、ログも記録済み。
  if(!renderer->Initialize(*m_graphicsSystem,OBJFilePath,textureFilePath))return nullptr;

  // 成功したRendererをmapへ登録し、所有を移した後も同じアドレスの借用ポインターを返す。
  ObjModelRenderer* borrowed_renderer = renderer.get();
  m_objModels.emplace(key,std::move(renderer));
  return borrowed_renderer;
}
