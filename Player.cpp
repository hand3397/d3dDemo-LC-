#include "stdafx.h"
#include "Player.h"

Player::Player(const string& name) : MovingObject(name), fsm_(this)
{
    SetAnimation({ "Idle" });
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

bool Player::wasJump() const
{
    return wasJump_;
}

XMFLOAT3 Player::GetMoveDir() const
{
    return moveDir_;
}

XMVECTOR Player::GetLookXZ() const
{
    XMFLOAT3 look = camera_.GetLook3f();
    return XMVector3NormalizeEst(XMVectorSet(look.x, 0.0f, look.z, 0.0f));;
}

XMVECTOR Player::GetRightXZ() const
{
    XMFLOAT3 right = camera_.GetRight3f();
    return XMVector3NormalizeEst(XMVectorSet(right.x, 0.0f, right.z, 0.0f));;
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
    wasJump_ = false;
    if (!isFalling_ && keyInput.IsKeyDown(VK_SPACE)) {
        isFalling_ = true;
        wasJump_ = true;
        fsm_.Change(JumpState::Instance());
    }

    isMoving_ = false;
    isRunning_ = false;
    moveDir_ = { 0.0f, 0.0f, 0.0f };
    if (keyInput.IsKeyDown('W')) {
        moveDir_.z += 1.0f;
    }
    if (keyInput.IsKeyDown('A')) {
        moveDir_.x -= 1.0f;
    }
    if (keyInput.IsKeyDown('S')) {
        moveDir_.z -= 1.0f;
    }
    if (keyInput.IsKeyDown('D')) {
        moveDir_.x += 1.0f;
    }
    if (moveDir_.x != 0.0f || moveDir_.z != 0.0f)
        isMoving_ = true;

    if (isMoving_ && keyInput.IsKeyDown(VK_LSHIFT))
        isRunning_ = true;
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
    blendAnimationTime_ += dt;

    for(auto &ri : renderItems_)
        if (ri->skinnedModelInst_ && ri->skinnedCBIndex_ >= 0)
            ri->skinnedModelInst_->SetAnimtion(currAnimation_, animationTime_, prevAnimation_, 
                clamp(blendAnimationTime_ / blendAnimationTimeMax_, 0.0f, 1.0f));

    camera_.SetTarget(position_);
    camera_.UpdateViewMatrix();
}

FSM<Player> Player::GetFSM()
{
    return fsm_;
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

const string& Player::GetAnimationName()
{
    return currAnimation_;
}

float Player::GetAnimtionTime()
{
    return animationTime_;
}

void Player::SetAnimation(const string& animName)
{
    prevAnimation_ = currAnimation_;
    currAnimation_ = animName;
    // 0.3초 동안 과거의 애니메이션과 블렌딩됨
    blendAnimationTime_ = 0.0f;
}

void Player::SetAnimationTime()
{
    animationTime_ = 0.0f;
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
    owner->SetAnimationTime();
    owner->SetAnimation({ "Idle" });
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
    owner->SetAnimationTime();
}

void MoveState::Update(Player* owner, FSM<Player>& fsm)
{
    if (!owner->IsMoving()) {
        fsm.Change(IdleState::Instance());
        return;
    }

    XMFLOAT3 moveDir = owner->GetMoveDir();

    // 걷는 방향에 따라 애니메이션 설정
    static const string dirs[8] = {
        "WalkF",  "WalkFR", "WalkR1", "WalkBR",
        "WalkB",  "WalkBL", "WalkL1", "WalkFL"
    };

    if (moveDir.x == 0.0f && moveDir.z == 0.0f)
        return owner->SetAnimation("WalkF");

    float angle = std::atan2(moveDir.x, moveDir.z);
    float deg = XMConvertToDegrees(angle);
    if (deg < 0) deg += 360.0f;
    int index = static_cast<int>((deg + 22.5f) / 45.0f) % 8;

    return owner->SetAnimation(dirs[index]);
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
    owner->SetAnimationTime();
    owner->SetAnimation({ "Jump" });
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
    owner->SetAnimationTime();
    owner->SetAnimation({ "Falling" });
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