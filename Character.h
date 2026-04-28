#pragma once
#include "GameObject.h"
#include "FSM.h"

class Character : public GameObject
{
    public:
    Character(const XMFLOAT3& position);
    virtual ~Character() {}

    virtual void Update(float dt) override;

    void MoveTargetPosXZ(float dt);

    void SetTargetPos(const XMFLOAT3& targetPos);
private:
    FSM<Character> fsm_;

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


