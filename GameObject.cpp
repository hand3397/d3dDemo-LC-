#include "GameObject.h"

void GameObject::Update(float dt)
{
    UpdateRenderItem();
}

MovingObject::MovingObject()
{
}

void MovingObject::Update(float dt)
{
    UpdatePhysics(dt);
    UpdateRenderItem();
}

void MovingObject::ApplyForce(const XMFLOAT3& force)
{
    acceleration_.x += force.x;
    acceleration_.y += force.y;
    acceleration_.z += force.z;
}

void MovingObject::UpdatePhysics(float dt)
{
    velocity_.x += acceleration_.x * dt;
    velocity_.y += acceleration_.y * dt;
    velocity_.z += acceleration_.z * dt;

    acceleration_ = { 0.0f, 0.0f, 0.0f };

    velocity_.x *= (1.0f - friction_ * dt);
    velocity_.y *= (1.0f - gravity_ * dt);
    velocity_.z *= (1.0f - friction_ * dt);

    position_.x += velocity_.x * dt;
    position_.y += velocity_.y * dt;
    position_.z += velocity_.z * dt;
}
