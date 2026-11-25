#include "../H/EnemyCorpseFader.h"

// 倒された敵の死体の、預かり・時間の進行・描画・解放を実装する。

//========= 預かり・解放関数=========
// 死体を1つ預かる。
bool EnemyCorpseFader::Add(const EnemyModelPlayback& playback,const ENEMY_PLAYBACK_STATE& state)
{
  // 生きている敵と、モデルで描けない（Cubeで描く）敵、すでに消えている死体は、預からない。
  if(state.isAlive || playback.IsCorpseGone(state))return false;

  // 最初の空きへ、コピーして預ける。空きがなければ、預からない。
  for(CORPSE& corpse : m_corpses){
    if(corpse.isActive)continue;

    corpse.playback = playback;
    corpse.state = state;
    corpse.isActive = true;
    return true;
  }
  return false;
}

// 預かっている死体を、全て手放す。
void EnemyCorpseFader::Clear()
{
  for(CORPSE& corpse : m_corpses){
    corpse = CORPSE{};
  }
}

//========= 更新・描画関数=========
// 死体の時間を進め、完全に消えたものを手放す。
void EnemyCorpseFader::Update(float deltaTime)
{
  // 時間が進まない（ヒットストップ中）場合は、何もしない。
  if(deltaTime <= 0.0f)return;

  for(CORPSE& corpse : m_corpses){
    if(!corpse.isActive)continue;

    // 死亡の動きの時刻を進め、保持と消えるまでの時間が過ぎたら、空きに戻す。
    corpse.playback.Update(corpse.state,deltaTime);
    if(corpse.playback.IsCorpseGone(corpse.state)){
      corpse = CORPSE{};
    }
  }
}

// 預かっている死体を、全て描く。
void EnemyCorpseFader::Draw(GraphicsSystem& graphicsSystem,const CAMERA_MATRICES& camera) const
{
  for(const CORPSE& corpse : m_corpses){
    if(!corpse.isActive)continue;

    // 描けない場合（読込の解放後など）は、何も描かない。Cubeでは、描かない。
    static_cast<void>(corpse.playback.Draw(graphicsSystem,camera,corpse.state));
  }
}

//========= Getter関数=========
// 預かっている死体の数を返す。
std::size_t EnemyCorpseFader::GetCount() const
{
  std::size_t count = 0;
  for(const CORPSE& corpse : m_corpses){
    if(corpse.isActive)++count;
  }
  return count;
}
