#pragma once
#include "GameObject.h"
#include "FSM.h"
#include "AtlasAnimator.h"

class Character : public GameObject
{
public:

    Character(const XMFLOAT3& position, const AtlasAnimatorProfile* profile);
    virtual ~Character() {}

    virtual void Update(float dt) override;

    void MoveTargetPosXZ(float dt);

    void SetTargetPos(const XMFLOAT3& targetPos);

    // animation
    AtlasAnimator* GetAnimator();
    uint32_t GetAtlasIndex() const;

private:

    FSM<Character> fsm_;

    AtlasAnimator animator_;

    XMFLOAT3 targetPos_ = { 0.0f, 0.0f, 0.0f };
    float moveSpeed_ = 2.0f;
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


