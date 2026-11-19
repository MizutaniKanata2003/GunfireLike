#pragma once

// 3D Objectが共有して使うRendererを所有する。Cube用のBasicMeshRendererは1個、OBJ用はOBJと画像のパスの組ごとに1個を保持する。
// Rendererへの借用ポインターを返すだけで、Objectの配置や描画順は担当しない。Resource生成は初期化と取得の時点だけで、描画中は行わない。
// 借用ポインターは、Uninitまたは自身の破棄でRendererが解放された時点で無効になる。コピー・移動は禁止する。

//========= C++標準ライブラリ インクルード=========
#include <memory> // std::unique_ptrで共有するOBJ Rendererを所有し、mapの再配置後もアドレスを変えない。
#include <string> // std::wstringでOBJ・画像のパスと、共有判定のKeyを扱う。
#include <unordered_map> // OBJと画像のパスの組をKeyに、生成済みのOBJ Rendererを検索・保持する。

//========= Framework インクルード=========
#include "Framework/3D/H/BasicMeshRenderer.h"
#include "Framework/3D/H/ObjModelRenderer.h"

//========= 前方宣言=========
class GraphicsSystem;

// Cube・OBJの共有Rendererを所有し、同じ組み合わせのResourceを重複して読み込まないようにする。
class ModelRegistry final
{
public:
  //========= 生成・コピー設定=========
  // 未初期化の状態で生成する。コピー禁止の宣言により暗黙の既定コンストラクターが消えるため、明示する。
  ModelRegistry() = default;
  // 借用ポインターの指す先を複製しないよう、コピー構築を禁止する。
  ModelRegistry(const ModelRegistry&) = delete;
  // 借用ポインターの指す先を複製しないよう、コピー代入を禁止する。
  ModelRegistry& operator=(const ModelRegistry&) = delete;

  //========= 初期化・終了関数=========
  // 旧Resourceを先にUninitし、Cube用Rendererを初期化して、成功後だけ描画基盤を登録する。
  // graphicsSystem: Rendererの生成に使うDeviceの取得元。登録後Uninitまで有効な実体を渡す。参照は保持する。
  // 戻り値: Cube用Rendererの初期化に成功すればtrue。失敗時は保持するResourceをUninitしてfalse。
  // 旧状態へは復帰しない。下位処理の例外は捕捉しない。
  bool Initialize(GraphicsSystem& graphicsSystem);
  // 取得済みのOBJ RendererとCube用Rendererを解放し、描画基盤の登録を解除する。未初期化・終了済みでも呼出可能。
  // 先に借用しているObjectを破棄または無効化してから呼ぶ。ContextのBind解除やGPU完了待機は行わない。
  void Uninit();

  //========= 取得関数=========
  // Cube用の共有Rendererへの借用ポインターを返す。所有権は移さない。
  // 戻り値: 初期化成功後はRendererへのポインター。未初期化または終了後はnullptr。
  [[nodiscard]] BasicMeshRenderer* GetCubeRenderer();
  // OBJと画像のパスの組に対応する共有Rendererの借用ポインターを返す。未登録の組だけ新しく生成して初期化する。
  // OBJFilePath: OBJファイルのパス。作業ディレクトリ基準。共有判定のKeyに含める。
  // textureFilePath: 画像のパス。空文字列なら単色のRendererになる。共有判定のKeyに含める。
  // 戻り値: 登録済みまたは新規生成したRendererへのポインター。未初期化・生成失敗ではnullptr。
  // 失敗した組は登録しない。Resourceを読み込むため、Scene初期化中にだけ呼ぶこと。
  [[nodiscard]] ObjModelRenderer* AcquireObjModel(const std::wstring& OBJFilePath,const std::wstring& textureFilePath);

private:
  //========= 描画基盤参照=========
  // OBJ Rendererの生成に使う描画基盤。非所有で、Initialize成功からUninitまで有効であること。未初期化はnullptr。
  GraphicsSystem* m_graphicsSystem{};

  //========= 共有Renderer=========
  // 全Cube Objectで共有するRenderer。Initializeで生成し、Uninitで解放する。
  BasicMeshRenderer m_cubeRenderer{};
  // OBJと画像のパスの組をKeyにした共有OBJ Renderer。unique_ptrにより、要素追加後もRendererのアドレスが変わらない。
  std::unordered_map<std::wstring,std::unique_ptr<ObjModelRenderer>> m_objModels{};
};
