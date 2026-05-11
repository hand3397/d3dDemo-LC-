#include "Character.h"

Character::Character(uint32_t id, const XMFLOAT3& position, const AtlasAnimatorProfile* profile) :
    GameObject(XMFLOAT3(0.f, 0.f, 0.f), position, spe::RigidbodyType::KINEMATIC), fsm_(this), ID_(id)
{
    // character의 충돌체는 캡슐
    spe::CapsuleShape* capsuleShape = new spe::CapsuleShape(XMFLOAT3(0.f, 1.f, 0.f), 0.1f, 0.3f);
    spe::Fixture* fixture = new spe::Fixture(capsuleShape);
    fixture->SetFriction(0.4f);
    fixture->SetRestitution(0.4f);

    rigidbody_.AddFixture(fixture);

    targetPos_ = position;

    // 초기 상태는 Idle로 설정
    animator_.SetAnimationProfile(profile);
    fsm_.Change(CharacterIdleState::Instance());
}

void Character::Update(float dt)
{
    position_.y = -0.75f + 1.0f;

    // 현재 위치와 targetPos가 가까우면 targetPos로 이동
    XMVECTOR currentPos = XMVectorSet(position_.x, 0.f, position_.z, 1.0f);
    XMVECTOR targetPos = XMVectorSet(targetPos_.x, 0.f, targetPos_.z, 1.0f);
    if (XMVectorGetX(XMVector2LengthEst(targetPos - currentPos)) < 0.3f) {
        MoveTargetPosXZ(dt);
    }
    // 멀다면 flowField를 따라 이동
    else {
        XMFLOAT3 vel;
        XMStoreFloat3(&vel, XMLoadFloat3(&flowFieldDir_) * moveSpeed_);
        rigidbody_.SetLinearVelocity(vel);
    }
    
    animator_.Update(dt);
    fsm_.Update();
    
    for (const auto& ri : renderItems_)
        ri->atlasIndex_ = animator_.GetCurrentAtlasIndex();

	GameObject::Update(dt);
}

void Character::Move(const XMFLOAT3& dir)
{
    isMoving_ = true;

    flowFieldDir_ = dir;
}

void Character::MoveTargetPosXZ(float dt)
{
    rigidbody_.SetAwake(true);
    isMoving_ = true;

    XMVECTOR curPos = XMLoadFloat3(&position_);
    XMVECTOR targetPos = XMLoadFloat3(&targetPos_);

    XMVECTOR diff = targetPos - curPos;
    diff = XMVectorSetY(diff, 0.0f); // Y축 이동은 무시

    float distance = XMVectorGetX(XMVector3Length(diff));

    // 현재 Y축 속력 백업 (중력이나 점프 등 기존 물리값 유지)
    float currentVelocityY = rigidbody_.GetLinearVelocity().y;

    if (distance < 0.01f) {
        XMFLOAT3 currentPosF3 = position_;
        currentPosF3.x = targetPos_.x;
        currentPosF3.z = targetPos_.z;
        SetPosition(currentPosF3);

        // [핵심] 완전히 도착했을 때, XZ 속력을 0으로 초기화하여 더 이상 밀려나지 않게 합니다.
        rigidbody_.SetLinearVelocity(XMFLOAT3(0.0f, currentVelocityY, 0.0f));
        isMoving_ = false;
        return;
    }

    XMVECTOR direction = XMVector3Normalize(diff);
    float moveDist = moveSpeed_ * dt;

    if (moveDist > distance) {
        // 이번 프레임에 도착 가능한 경우
        XMFLOAT3 finalPosF3 = position_;
        finalPosF3.x = targetPos_.x;
        finalPosF3.z = targetPos_.z;
        SetPosition(finalPosF3);

        rigidbody_.SetLinearVelocity(XMFLOAT3(0.0f, currentVelocityY, 0.0f));
        isMoving_ = false;
    }
    else {
        // 방향에 따라 XZ 평면으로 이동 (Y축 속력은 기존 값 유지)
        rigidbody_.SetLinearVelocity(XMFLOAT3(
            XMVectorGetX(direction) * moveSpeed_,
            currentVelocityY,
            XMVectorGetZ(direction) * moveSpeed_
        ));
    }
}

void Character::SetTargetPos(int tx, int tz, const XMFLOAT3& targetPos)
{
    currentTileX_ = tx;
    currentTileZ_ = tz; 
    targetPos_ = targetPos;
}

AtlasAnimator* Character::GetAnimator()
{
    return &animator_;
}

uint32_t Character::GetAtlasIndex() const
{
    return animator_.GetCurrentAtlasIndex();
}

bool Character::IsMoving() const
{
    return isMoving_;
}

int32_t Character::GetID() const
{
    return ID_;
}

pair<int, int> Character::GetCurrentTileXZ() const
{
    return {currentTileX_, currentTileZ_};
}

void Character::SetCurrentTileXZ(int x, int z)
{
    currentTileX_ = x;
    currentTileZ_ = z;
}

// FSM States

// CharacterIdleState는 캐릭터가 목표 위치에 도착하여 멈춰있는 상태입니다.
CharacterIdleState* CharacterIdleState::Instance()
{
    static CharacterIdleState instance;
    return &instance;
}

void CharacterIdleState::Enter(Character* owner)
{
    owner->GetAnimator()->Play("KnightIdle", true);
}

void CharacterIdleState::Update(Character* owner, FSM<Character>& fsm)
{
    if (owner->IsMoving()) {
        fsm.Change(CharacterMoveState::Instance());
    }   
}

void CharacterIdleState::Exit(Character* owner)
{
    owner->GetAnimator()->Stop();
}

// CharacterMoveState는 캐릭터가 목표 위치를 향해 이동하는 상태입니다.
CharacterMoveState* CharacterMoveState::Instance()
{
    static CharacterMoveState instance;
    return &instance;
}

void CharacterMoveState::Enter(Character* owner)
{
    owner->GetAnimator()->Play("KnightMove", true);
}

void CharacterMoveState::Update(Character* owner, FSM<Character>& fsm)
{
    if (!owner->IsMoving()) {
        fsm.Change(CharacterIdleState::Instance());
    }
}

void CharacterMoveState::Exit(Character* owner)
{
    owner->GetAnimator()->Stop();
}