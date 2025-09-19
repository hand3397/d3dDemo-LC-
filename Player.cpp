#include "Player.h"

Player::Player(const string& name) : MovingObject(name), fsm_(this)
{
    fsm_.Change(IdleState::Instance());

    speed_ = 250.0f;
    camera_.SetMode(CameraMode::TPS);
    SetPosition(XMFLOAT3(0.0f, 0.0f, 20.0f));
    SetCameraOffset(XMFLOAT3(0.0f, 2.0f, 0.0f));
    SetCameraAngle(XMConvertToRadians(30), 0.0f, 0.0f);
}

bool Player::IsMoving() const
{
    return isMoving_;
}

bool Player::IsRunning() const
{
    return isRunning_;
}

bool Player::IsFalling() const
{
    return isFalling_;
}

void Player::KeyInput(const KeyInputManager& keyInput, float dt)
{
    //mouse input
    if (keyInput.IsMouseDown(MouseButton::LMB)) {
        int dx, dy;
        keyInput.GetMouseDelta(dx, dy);

        camera_.RotatePitch(static_cast<float>(dy) * 0.01f);
        camera_.RotateYaw(static_cast<float>(dx) * 0.01f);
        camera_.UpdateViewMatrix();
    }

    //keyboard input
    XMVECTOR force = XMVectorZero();
    XMVECTOR lookXZDir = XMVector3NormalizeEst(XMVectorSet(camera_.GetLook3f().x, 0.0f, camera_.GetLook3f().z, 0.0f));
    XMVECTOR rightXZDir = XMVector3NormalizeEst(XMVectorSet(camera_.GetRight3f().x, 0.0f, camera_.GetRight3f().z, 0.0f));
    float moveSpeed = speed_;
    bool isMove = false, isRun = false;

    if (!isFalling_ && keyInput.IsKeyDown(VK_SPACE)) {
        isFalling_ = true;
        force += XMVectorSet(0, 10000.0f * dt, 0, 0);
        fsm_.Change(JumpState::Instance());
    }

    if (keyInput.IsKeyDown(VK_LSHIFT)) {
        moveSpeed *= 2.5f;
        isRun = true;
    }
    if (keyInput.IsKeyDown('W')) {
        force += lookXZDir * moveSpeed * dt;
        isMove = true;
    }
    if (keyInput.IsKeyDown('A')) {
        force += -rightXZDir * moveSpeed * dt;
        isMove = true;
    }
    if (keyInput.IsKeyDown('S')) {
        force += -lookXZDir * moveSpeed * dt;
        isMove = true;
    }
    if (keyInput.IsKeyDown('D')) {
        force += rightXZDir * moveSpeed * dt;
        isMove = true;
    }

    isMoving_ = isMove;
    isRunning_ = (isMove && isRun);

    XMFLOAT3 force3f;
    XMStoreFloat3(&force3f, force);
    ApplyForce(force3f);
}

void Player::Update(float dt)
{
    MovingObject::Update(dt);
    isFalling_ = true;
    if (position_.y < 0.0f) {
        position_.y = 0.0f;
        isFalling_ = false;
    }

    fsm_.Update();

    //update animaiton
    animationTime_ += dt;
    for(auto &ri : renderItems_)
        if (ri->skinnedModelInst_ && ri->skinnedCBIndex_ >= 0)
            ri->skinnedModelInst_->SetAnimtion(animationName_, animationTime_);

    camera_.SetTarget(position_);
    camera_.UpdateViewMatrix();
}

Camera* Player::GetCamera()
{
    return &camera_ ;
}

void Player::SetCameraOffset(const XMFLOAT3& offset)
{
    camera_.SetOffset(offset);
}

void Player::SetCameraAngle(float pitch, float yaw, float roll)
{
    camera_.SetPitch(pitch);
    camera_.SetYaw(yaw);
    camera_.SetRoll(roll);
}

string Player::GetAnimationName()
{
    return animationName_;
}

float Player::GetAnimtionTime()
{
    return animationTime_;
}

void Player::SetAnimation(const string& animName)
{
    animationTime_ = 0.0f;
    animationName_ = animName;
}

//-----------------------------------------------------------------
//  IdleState
//-----------------------------------------------------------------

IdleState* IdleState::Instance()
{
    static IdleState instance;
    return &instance;
}

void IdleState::Enter(Player* owner)
{
    owner->SetAnimation("Idle0");
}

void IdleState::Update(Player* owner, FSM<Player>& fsm)
{
    if (owner->IsMoving())
        fsm.Change(MoveState::Instance());
}

void IdleState::Exit(Player* owner)
{
}

//-----------------------------------------------------------------
//  MoveState
//-----------------------------------------------------------------

MoveState* MoveState::Instance()
{
    static MoveState instance;
    return &instance;
}

void MoveState::Enter(Player* owner)
{
    owner->SetAnimation("WalkForward0");
}

void MoveState::Update(Player* owner, FSM<Player>& fsm)
{
    if (!owner->IsMoving()) {
        fsm.Change(IdleState::Instance());
        return;
    }

    if (owner->IsRunning()) {
        if (owner->GetAnimationName() != "RunForward0")
            owner->SetAnimation("RunForward0");
    }
    else {
        if (owner->GetAnimationName() != "WalkForward0")
            owner->SetAnimation("WalkForward0");
    }
        
}

void MoveState::Exit(Player* owner)
{
}

//-----------------------------------------------------------------
//  JumpState
//-----------------------------------------------------------------

JumpState* JumpState::Instance()
{
    static JumpState instance;
    return &instance;
}

void JumpState::Enter(Player* owner)
{
    owner->SetAnimation("Jump0");
}

void JumpState::Update(Player* owner, FSM<Player>& fsm)
{
    if (!owner->IsFalling()) {
        fsm.Change(IdleState::Instance());
        return;
    }
    float animationTime = owner->GetAnimtionTime();
    float jumpDuration = 1.0f;
    if (animationTime >= jumpDuration)
        fsm.Change(FallingState::Instance());
}

void JumpState::Exit(Player* owner)
{
}

//-----------------------------------------------------------------
//  FallingState
//-----------------------------------------------------------------

FallingState* FallingState::Instance()
{
    static FallingState instance;
    return &instance;
}

void FallingState::Enter(Player* owner)
{
    owner->SetAnimation("Falling0");
}

void FallingState::Update(Player* owner, FSM<Player>& fsm)
{
    if (!owner->IsFalling()) {
        fsm.Change(IdleState::Instance());
        return;
    }
}

void FallingState::Exit(Player* owner)
{
}