#include "Player.h"

Player::Player(const string& name) : MovingObject(name), fsm_(this)
{
    fsm_.Change(IdleState::Instance());

    speed_ = 500.0f;
    camera_.SetMode(CameraMode::TPS);
    SetPosition(XMFLOAT3(0.0f, 0.0f, 20.0f));
    SetCameraOffset(XMFLOAT3(0.0f, 2.0f, 0.0f));
    SetCameraAngle(XMConvertToRadians(30), 0.0f, 0.0f);
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
    if (keyInput.IsKeyDown('W')) {
        force += lookXZDir * speed_ * dt;
    }
    if (keyInput.IsKeyDown('A')) {
        force += -rightXZDir * speed_ * dt;
    }
    if (keyInput.IsKeyDown('S')) {
        force += -lookXZDir * speed_ * dt;
    }
    if (keyInput.IsKeyDown('D')) {
        force += rightXZDir * speed_ * dt;
    }

    XMFLOAT3 force3f;
    XMStoreFloat3(&force3f, force);
    ApplyForce(force3f);
}

void Player::Update(float dt)
{
    fsm_.Update();
    //pos Update
    MovingObject::Update(dt);

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

string Player::GetAnimtionName()
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
    if (owner->IsAccelerating())
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
    if (!owner->IsAccelerating())
        fsm.Change(IdleState::Instance());
}

void MoveState::Exit(Player* owner)
{
}