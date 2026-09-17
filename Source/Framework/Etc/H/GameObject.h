#pragma once

// Update、Draw、終了処理を持つゲームオブジェクトの基底クラス。
class GameObject
{
public:
	// 派生クラスを安全に破棄するための仮想デストラクタ。
	virtual ~GameObject() = default;

	// 派生クラスが必要に応じて初期化処理を実装する。
	virtual void Initialize() {}

	// 派生クラスが必要に応じて詳細な初期化処理を実装する。
	virtual bool Init()
	{
		return true;
	}

	// フレームごとのゲームオブジェクト更新処理を行う。
	virtual void Update( float deltaTime ) = 0;

	// ゲームオブジェクトを描画する。
	virtual void Draw() = 0;

	// 派生クラスが必要に応じて終了処理を実装する。
	virtual void Uninit() {}

	// 次の管理タイミングで破棄するための要求を設定する。
	void SetDestroyRequested()
	{
		m_IsDestroyRequested = true;
	}

	// 破棄要求が設定されているかを返す。
	[[nodiscard]] bool IsDestroyRequested() const
	{
		return m_IsDestroyRequested;
	}

private:
	// 管理側がオブジェクトを削除するべきかを示すフラグ。
	bool m_IsDestroyRequested{};
};