#pragma once
#include "GameObject.h"
#include "FSM.h"
#include "AtlasAnimator.h"

constexpr uint32_t INVALID_ID = 0;

class Character : public GameObject
{
public:

    Character(uint32_t id, const XMFLOAT3& position, const AtlasAnimatorProfile* profile);
    virtual ~Character() {}

    virtual void Update(float dt) override;

    void Move(const XMFLOAT3& dir);
    void MoveTargetPosXZ(float dt);

    void SetTargetPos(int tx, int tz,const XMFLOAT3& targetPos);

    // animation
    AtlasAnimator* GetAnimator();
    uint32_t GetAtlasIndex() const;

    bool IsMoving() const;

    int32_t GetID() const;
    pair<int, int> GetCurrentTileXZ() const;
    void SetCurrentTileXZ(int x, int z);

private:

    const uint32_t ID_ = 0;

    FSM<Character> fsm_;

    AtlasAnimator animator_;

    XMFLOAT3 targetPos_ = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 flowFieldDir_ = { 0.0f, 0.0f, 0.0f };
    float moveSpeed_ = 1.0f;
    bool isMoving_ = false;

    // 현재 캐릭터가 위치한 타일의 xz 인덱스 == -1이면 아직 타일에 배치되지 않은 상태를 의미한다.
    int currentTileX_ = -1; 
    int currentTileZ_ = -1;
};

class CharacterIdleState : public State<Character>
{
public:
    static CharacterIdleState* Instance();
    void Enter(Character* owner) override;
    void Update(Character* owner, FSM<Character>& fsm) override;
    void Exit(Character* owner) override;
};

class CharacterMoveState : public State<Character>
{
public:
    static CharacterMoveState* Instance();
    void Enter(Character* owner) override;
    void Update(Character* owner, FSM<Character>& fsm) override;
    void Exit(Character* owner) override;
};


