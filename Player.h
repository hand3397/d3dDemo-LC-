#pragma once
#include "GameObject.h"
#include "KeyInputManager.h" 
#include "Camera.h"
#include "FSM.h"

class IdleState;
class MoveState;
class JumpState;
class FallingState;

class Player : public MovingObject 
{
public:
    Player(const string& name);

    void KeyInput(const KeyInputManager& keyInput, float dt);
    virtual void Update(float dt) override;

    Camera* GetCamera();
    void SetCameraOffset(const XMFLOAT3& offset);
    void SetCameraAngle(float pitch, float yaw, float roll);

    string GetAnimtionName();
    float GetAnimtionTime();
    void SetAnimation(const string& animName);
private:
    FSM<Player> fsm_;

    Camera camera_;
    XMFLOAT4X4 cameraOffset_ = MathHelper::Identity4x4();

    float animationTime_;
    string animationName_;
};

class IdleState : public State<Player>
{
public:
    static IdleState* Instance();
    void Enter(Player* owner) override;
    void Update(Player* owner, FSM<Player>& fsm) override;
    void Exit(Player* owner) override;
};

class MoveState : public State<Player>
{
public:
    static MoveState* Instance();
    void Enter(Player* owner) override;
    void Update(Player* owner, FSM<Player>& fsm) override;
    void Exit(Player* owner) override;
};

class JumpState : public State<Player>
{
public:
    static JumpState* Instance()
    {
        static JumpState instance;
        return &instance;
    }

    void Enter(Player* owner) override
    {
        owner->SetAnimation("Idle0");
    }

    void Update(Player* owner, FSM<Player>& fsm) override
    {
    }

    void Exit(Player* owner) override
    {
    }
};

class FallingState : public State<Player>
{
public:
    static FallingState* Instance()
    {
        static FallingState instance;
        return &instance;
    }

    void Enter(Player* owner) override
    {
        owner->SetAnimation("Idle0");
    }

    void Update(Player* owner, FSM<Player>& fsm) override
    {
    }

    void Exit(Player* owner) override
    {
    }
};
