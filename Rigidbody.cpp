#include "RigidBody.h"

Rigidbody::Rigidbody(uint8_t type, XMFLOAT3 scale, XMFLOAT4 rotateQuat, XMFLOAT3 position) :
    type_(type), scale_(scale), orientation_(rotateQuat), position_(position)
{
    localBoundingSphere_.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
    localBoundingSphere_.Radius = 1.414f;
    localBoundingBox_.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
    localBoundingBox_.Extents = XMFLOAT3(1.0f, 1.0f, 1.0f);

    UpdateBounds();
}

Rigidbody::Rigidbody(uint8_t type,
    XMFLOAT3 scale, XMFLOAT4 rotateQuat, XMFLOAT3 position,
    BoundingSphere boundingSphere, BoundingOrientedBox boundingBox) :
    type_(type), scale_(scale), orientation_(rotateQuat), position_(position),
    localBoundingSphere_(boundingSphere), localBoundingBox_(boundingBox)
{
    UpdateBounds();
}

void Rigidbody::UpdateBounds()
{
    XMMATRIX transform = XMMatrixAffineTransformation(
        XMLoadFloat3(&scale_),
        XMVectorZero(),
        XMLoadFloat4(&orientation_),
        XMLoadFloat3(&position_));

    localBoundingBox_.Transform(boundingBox_, transform);
    localBoundingSphere_.Transform(boundingSphere_, transform);
}

void Rigidbody::ApplyForce(const XMFLOAT3& f)
{
    force_.x += f.x;
    force_.y += f.y;
    force_.z += f.z;
}

void Rigidbody::Integrate(float dt)
{
    if (inverseMass_ <= 0.0f) return; // 고정체는 무시

    velocity_.x += (force_.x * inverseMass_) * dt;
    velocity_.y += (force_.y * inverseMass_) * dt;
    velocity_.z += (force_.z * inverseMass_) * dt;

    position_.x += velocity_.x * dt;
    position_.y += velocity_.y * dt;
    position_.z += velocity_.z * dt;

    XMStoreFloat3(&velocity_, XMLoadFloat3(&velocity_) * 0.5f);

    force_ = { 0.0f, 0.0f, 0.0f };

    UpdateBounds();
}

