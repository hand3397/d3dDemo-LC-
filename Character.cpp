#include "Character.h"

Character::Character(const XMFLOAT3& scale, const XMFLOAT3& position) : 
    GameObject(XMFLOAT3(0.f, 0.f, 0.f), position, spe::RigidbodyType::KINEMATIC)
{
    // characterÀÇ Ãæµ¹Ã¼´Â Ä¸½¶
	spe::CapsuleShape* capsuleShape = new spe::CapsuleShape(XMFLOAT3(0.f, 1.f, 0.f), 0.3f, 0.5f);
	spe::Fixture* fixture = new spe::Fixture(capsuleShape);
	fixture->SetFriction(0.4f);
	fixture->SetRestitution(0.4f);

    rigidbody_.AddFixture(fixture);

	targetPos_ = position;
}

void Character::Update(float dt)
{

	GameObject::Update(dt);
}

void Character::MoveTargetPos(float dt)
{

}
