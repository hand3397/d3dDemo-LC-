#include "Character.h"

Character::Character(const XMFLOAT3& position) : 
    GameObject(XMFLOAT3(0.f, 0.f, 0.f), position, spe::RigidbodyType::KINEMATIC)
{
    // character의 충돌체는 캡슐
	spe::CapsuleShape* capsuleShape = new spe::CapsuleShape(XMFLOAT3(0.f, 1.f, 0.f), 0.1f, 0.3f);
	spe::Fixture* fixture = new spe::Fixture(capsuleShape);
	fixture->SetFriction(0.4f);
	fixture->SetRestitution(0.4f);

    rigidbody_.AddFixture(fixture);

	targetPos_ = position;
}

void Character::Update(float dt)
{
    position_.y = -0.75f;

    MoveTargetPosXZ(dt);

	GameObject::Update(dt);
}

void Character::MoveTargetPosXZ(float dt)
{
    rigidbody_.SetAwake(true);

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

void Character::SetTargetPos(const XMFLOAT3& targetPos)
{
    targetPos_ = targetPos;
}
