#pragma once

// 更新・描画・任意の初期化と終了・破棄要求を共通化するゲームオブジェクトの基底クラス。
// オブジェクトの所有や管理コンテナからの削除は担当しない。呼出順は管理側が決める。

// 派生クラスに更新と描画を要求し、必要に応じた初期化・終了の実装箇所を提供する。
class GameObject
{
public:
	//========= 生成・破棄関数=========
	// 基底ポインター経由でも派生クラスのデストラクターを呼べるようにする。
	// Uninitを自動では呼ばない。必要な終了処理は破棄前に管理側が呼び出す。
	virtual ~GameObject() = default;

	//========= 純粋仮想関数=========
	// フレームごとの更新を派生クラスで実装する。
	// deltaTime: この更新で進める経過時間。単位は秒。基底クラスでは値を検証しない。
	virtual void Update(float deltaTime) = 0;
	// 表示内容の描画を派生クラスで実装する。描画先や必要な依存先は派生クラス側で扱う。
	virtual void Draw() = 0;

	//========= 任意実装関数=========
	// 派生クラスが必要とする初期状態を準備する。基底実装は何もしない。
	virtual void Initialize() {}
	// 成否を返す初期化処理を派生クラスで必要に応じて実装する。
	// 戻り値: 基底実装は常にtrue。派生実装の成功・失敗は呼出側で確認する。
	virtual bool Init() { return true; }
	// 派生クラスが必要とする終了処理を実装する。基底実装は何もしない。
	virtual void Uninit() {}

	//========= 状態変更関数=========
	// 管理側が破棄対象を判定するための要求を設定する。その場で自身を削除する処理ではない。
	void SetDestroyRequested() { m_isDestroyRequested = true; }

	//========= Getter関数=========
	// 破棄要求の設定有無を返す。要求の取り消しや、実際の破棄は行わない。
	[[nodiscard]] bool IsDestroyRequested() const { return m_isDestroyRequested; }

private:
	//========= 状態管理=========
	// 管理側が破棄対象を判定するためのフラグ。初期値はfalseで、要求時にtrueとなる。
	bool m_isDestroyRequested{};
};