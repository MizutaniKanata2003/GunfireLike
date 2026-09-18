#pragma once

// Update、Draw、終了処理を持つゲームオブジェクトの基底クラス。
class GameObject
{
public:
	//========= 生成・破棄関数=========
	// 派生クラスを安全に破棄するための仮想デストラクタ。
	virtual ~GameObject() = default;

	//========= 純粋仮想関数=========
	// フレームごとのゲームオブジェクト更新処理を行う。
	virtual void Update( float deltaTime ) = 0;
	// ゲームオブジェクトを描画する。
	virtual void Draw() = 0;

	//========= 任意実装関数=========
	// 派生クラスが必要に応じて初期化処理を実装する。
	virtual void Initialize() {}
	// 派生クラスが必要に応じて詳細な初期化処理を実装する。
	virtual bool Init()
	{
		return true;
	}
	// 派生クラスが必要に応じて終了処理を実装する。
	virtual void Uninit() {}

	//========= 状態変更関数=========
	// 次の管理タイミングで破棄するための要求を設定する。
	void SetDestroyRequested()
	{
		m_IsDestroyRequested = true;
	}

	//========= Getter関数=========
	// 破棄要求が設定されているかを返す。
	[[nodiscard]] bool IsDestroyRequested() const
	{
		return m_IsDestroyRequested;
	}

private:
	//========= 状態管理=========
	// 管理側がオブジェクトを削除するべきかを示すフラグ。
	bool m_IsDestroyRequested{};
};