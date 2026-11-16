#pragma once

// 設定表の配列からSceneObjectを順に生成する。状態を持たず、静的関数だけを提供する。
// 共有Rendererの所有はModelRegistryが担当し、このクラスはObjectの初期化と配列への追加だけを行う。

//========= C++標準ライブラリ インクルード=========
#include <cstddef> // std::size_tで設定表の行数を受け取る。
#include <vector> // 生成したSceneObjectを、設定表の順に保持する可変長配列として使用する。

//========= Scene インクルード=========
#include "Scene/Common/H/ObjectConfig.h"
#include "Scene/Common/H/SceneObject.h"

//========= 前方宣言=========
class ModelRegistry;

// 設定表から3D Objectの配列を作るFactory。
class SceneObjectFactory final
{
public:
  //========= 生成関数=========
  // 設定表を先頭から順にSceneObjectへ変換し、全件成功した場合だけobjectsの末尾へ追加する。
  // configs: 設定の配列の先頭。nullptrならfalse。参照は保持しない。
  // configCount: 配列の要素数。0なら何も追加せずtrue。
  // registry: 共有Rendererの取得先。Initialize済みであること。生成したObjectが借用するため、objectsより長く生存させる。
  // objects: 生成したObjectを末尾へ追加する出力先。失敗時は変更しない。追加順が描画順になる。
  // 戻り値: 全件を生成できればtrue。いずれかの行の初期化に失敗した場合はログへ行番号を記録してfalse。
  // 失敗するまでにRegistryが取得したRendererは保持される。例外は捕捉しない。
  [[nodiscard]] static bool Build(const OBJECT_CONFIG* configs,std::size_t configCount,ModelRegistry& registry,std::vector<SceneObject>& objects);

private:
  //========= 生成禁止関数=========
  // 状態を持たず、静的関数だけを使用するため構築を禁止する。
  SceneObjectFactory() = delete;
};
