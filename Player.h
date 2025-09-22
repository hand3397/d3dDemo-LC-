#pragma once
#include "GameObject.h"
#include "KeyInputManager.h" 
#include "Camera.h"
#include "FSM.h"

class IdleState;
class MoveState;
class JumpState;
class FallingState;

/*
    Player의 keyinput에서 
    isMoving_, isRunning_, isFalling_, moveDir_과 같은 이동에 관한 flag만 정함.
    Jump만 바로 State변환

    State는 해당 flag를 player로 부터받아와 player의 최종 애니메이션을 설정

    Player의 실제 이동은 MovingObject의 rigidBody를 phygics에 넘기고 phygics에서 모든 이동을 처리
*/

class Player : public MovingObject 
{
public:
    Player(const string& name);

    bool IsMoving() const;
    bool IsRunning() const;
    bool IsFalling() const;
    bool wasJump() const;
    XMFLOAT3 GetMoveDir() const;

    XMVECTOR GetLookXZ() const;
    XMVECTOR GetRightXZ() const;

    void KeyInput(const KeyInputManager& keyInput, float dt);
    virtual void Update(float dt) override;

    FSM<Player> GetFSM();

    Camera* GetCamera();
    void SetCameraOffset(const XMFLOAT3& offset);
    void SetCameraAngle(float pitch, float yaw, float roll);

    const string& GetAnimationName();
    float GetAnimtionTime();
    void SetAnimation(const string& animName);
    void SetAnimationTime();
private:
    FSM<Player> fsm_;

    Camera camera_;
    XMFLOAT4X4 cameraOffset_ = MathHelper::Identity4x4();

    float animationTime_;
    float blendAnimationTime_;
    float blendAnimationTimeMax_ = 0.3f;
    string currAnimation_;
    string prevAnimation_;
    float animationAlpha = 1.0f; // (prev) 0.0f ~ 1.0f (curr)

    bool isMoving_ = false;
    bool isRunning_ = false;
    bool isFalling_ = false;
    bool wasJump_ = false;
    // 실제 플레이어의 이동 벡터가 아닌 플레이어가 바라보는 방향을 z+, 우측은 x+라고 가정하고 방향성을 나타내는 값
    // forward = z+, right = x+ (Normalize)
    XMFLOAT3 moveDir_ = { 0.0f,0.0f, 0.0f, };
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
    static JumpState* Instance();
    void Enter(Player* owner) override;
    void Update(Player* owner, FSM<Player>& fsm) override;
    void Exit(Player* owner) override;
};

class FallingState : public State<Player>
{
public:
    static FallingState* Instance();
    void Enter(Player* owner) override;
    void Update(Player* owner, FSM<Player>& fsm) override;
    void Exit(Player* owner) override;
};
