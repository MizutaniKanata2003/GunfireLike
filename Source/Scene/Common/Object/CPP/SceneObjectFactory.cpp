#include "../H/SceneObjectFactory.h"

// 設定表の各行をSceneObjectへ変換し、必要ならColliderを登録して、全件成功した場合だけ出力配列へ追加する処理を実装する。
// Rendererの生成はRegistryへ委譲する。失敗は戻り値とログで通知し、例外の捕捉や再試行は行わない。

//========= C++標準ライブラリ インクルード=========
#include <cstdint> // std::uint32_tで、表の行番号をColliderの識別用の値へ変換して渡す。
#include <string> // std::to_wstringで失敗した設定表の行番号を、ログ文字列へ変換する。

//========= Framework インクルード=========
#include "Framework/Etc/H/Logger.h"

//========= Scene インクルード=========
#include "Scene/Common/Object/H/ModelRegistry.h"

// 生成の失敗時に使う補助関数をまとめる。
namespace
{
	//========= 補助関数=========
	// この生成中に登録したColliderを全て解除する。
	// collisionWorld: 解除先。nullptrなら何もしない。
	// handles: 解除するColliderの識別値の列。
	void RemoveColliders(CollisionWorld* collisionWorld,const std::vector<ColliderHandle>& handles)
	{
		// 登録先がなければ、解除する対象もないため戻る。
		if(collisionWorld == nullptr)return;

		// 記録した全ての識別値を解除する。登録順と逆順にする必要はない。
		for(const ColliderHandle handle : handles){
			collisionWorld->Remove(handle);
		}
	}
}

//========= 生成関数=========
// 設定表を順にObjectへ変換し、全件成功した場合だけ出力配列へ追加する。
bool SceneObjectFactory::Build(const OBJECT_CONFIG* configs,std::size_t configCount,ModelRegistry& registry,CollisionWorld* collisionWorld,
															 std::vector<SceneObject>& objects)
{
	// 配列の先頭がなければ要素を読めないため、要素数に関係なく失敗として戻す。
	if(configs == nullptr)return false;

	// 全件成功するまでobjectsを変更しないため、いったんローカル配列へ生成する。登録したColliderの識別値も、失敗時の解除用に記録する。
	std::vector<SceneObject> built_objects{};
	std::vector<ColliderHandle> added_handles{};
	built_objects.reserve(configCount);

	// 表の先頭から末尾まで1行ずつ生成する。この順序が描画順になるため、並べ替えや並列化はしない。
	for(std::size_t index = 0; index < configCount; ++index){
		// 1行分のObjectを初期化する。Rendererの借用に失敗するとfalseになる。
		SceneObject object{};
		// 初期化失敗を検出したら、行番号(0始まり)をログへ記録し、登録済みのColliderを解除して全体を失敗とする。objectsは変更されない。
		if(!object.Initialize(configs[index],registry)){
			Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_SCENE,L"SceneObjectの生成に失敗しました。表の行番号: " + std::to_wstring(index));
			RemoveColliders(collisionWorld,added_handles);
			return false;
		}

		// Colliderの指定があり、登録先がある行だけ、AABB Colliderを登録する。指定がない行や、登録先がない場合は何もしない。
		if(configs[index].collisionLayer != 0 && collisionWorld != nullptr){
			// OBJのAABBはモデル形状から作る必要があり、現行では未対応のため、この行の生成を失敗とする。
			if(configs[index].meshKind != OBJECT_MESH_KIND::e_CUBE){
				Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_SCENE,
											L"CubeではないObjectのCollider登録は未対応です。表の行番号: " + std::to_wstring(index));
				RemoveColliders(collisionWorld,added_handles);
				return false;
			}

			// Cubeは1辺1で中心が原点のため、位置を中心に、各軸の倍率の半分を左右に取ったAABBになる。回転は反映しない。
			const OBJECT_CONFIG& config = configs[index];
			COLLISION_AABB aabb{};
			aabb.minPoint = XMFLOAT3{config.position.x - config.scale.x * 0.5f,config.position.y - config.scale.y * 0.5f,
				config.position.z - config.scale.z * 0.5f};
			aabb.maxPoint = XMFLOAT3{config.position.x + config.scale.x * 0.5f,config.position.y + config.scale.y * 0.5f,
				config.position.z + config.scale.z * 0.5f};

			// 登録して、失敗時の解除用に識別値を記録する。呼出側の識別値には、表の行番号を入れる。
			added_handles.push_back(collisionWorld->AddAABB(aabb,config.collisionLayer,static_cast<std::uint32_t>(index)));
		}

		built_objects.push_back(object);
	}

	// 全件成功したため、生成順のままobjectsの末尾へ追加する。登録したColliderは残す。
	objects.insert(objects.end(),built_objects.begin(),built_objects.end());
	return true;
}