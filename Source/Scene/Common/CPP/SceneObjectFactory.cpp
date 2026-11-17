#include "../H/SceneObjectFactory.h"

// 設定表の各行をSceneObjectへ変換し、全件成功した場合だけ出力配列へ追加する処理を実装する。
// Rendererの生成はRegistryへ委譲する。失敗は戻り値とログで通知し、例外の捕捉や再試行は行わない。

//========= C++標準ライブラリ インクルード=========
#include <string> // std::to_wstringで失敗した設定表の行番号を、ログ文字列へ変換する。

//========= Framework インクルード=========
#include "Framework/Etc/H/Logger.h"

//========= Scene インクルード=========
#include "Scene/Common/H/ModelRegistry.h"

//========= 生成関数=========
// 設定表を順にObjectへ変換し、全件成功した場合だけ出力配列へ追加する。
bool SceneObjectFactory::Build(const OBJECT_CONFIG* configs,std::size_t configCount,ModelRegistry& registry,std::vector<SceneObject>& objects)
{
  // 配列の先頭がなければ要素を読めないため、要素数に関係なく失敗として戻す。
  if(configs == nullptr)return false;

  // 全件成功するまでobjectsを変更しないため、いったんローカル配列へ生成する。
  std::vector<SceneObject> built_objects{};
  built_objects.reserve(configCount);

  // 表の先頭から末尾まで1行ずつ生成する。この順序が描画順になるため、並べ替えや並列化はしない。
  for(std::size_t index = 0; index < configCount; ++index){
    // 1行分のObjectを初期化する。Rendererの借用に失敗するとfalseになる。
    SceneObject object{};
    // 初期化失敗を検出したら、行番号(0始まり)をログへ記録して全体を失敗とする。ローカル配列は破棄され、objectsは変更されない。
    if(!object.Initialize(configs[index],registry)){
      Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_SCENE,L"SceneObjectの生成に失敗しました。表の行番号: " + std::to_wstring(index));
      return false;
    }

    built_objects.push_back(object);
  }

  // 全件成功したため、生成順のままobjectsの末尾へ追加する。
  objects.insert(objects.end(),built_objects.begin(),built_objects.end());
  return true;
}
