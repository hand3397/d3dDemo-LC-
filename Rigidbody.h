#pragma once

#include "d3dUtil.h"

enum class RigidbodyType : uint8_t
{
    Static = 0,
    Dynamic,
    Kinematic,
    Count,
};

struct Rigidbody
{
    Rigidbody() = default;
    Rigidbody(uint8_t type,
        XMFLOAT3 scale, XMFLOAT4 rotateQuat, XMFLOAT3 position,
        BoundingSphere boundingSphere = BoundingSphere(),
        BoundingOrientedBox boundingBox = BoundingOrientedBox()) :
        type_(type), scale_(scale), position_(position), orientation_(rotateQuat),
        localBoundingSphere_(boundingSphere), localBoundingBox_(boundingBox)
    {
        localBoundingSphere_.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
        localBoundingSphere_.Radius = 1.414f;
        localBoundingBox_.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
        localBoundingBox_.Extents = XMFLOAT3(1.0f, 1.0f, 1.0f);

        UpdateBounds();
    }

    void UpdateBounds();
     
    uint8_t type_ = (uint8_t)RigidbodyType::Static;

    float mass_ = 1.0f;
    float inverseMass_ = 1.0f; // 1.0f / mass

    XMFLOAT3 scale_ = { 1.0f, 1.0f, 1.0f };

    XMFLOAT3 position_ = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 velocity_ = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 acceleration_ = { 0.0f, 0.0f, 0.0f };

    XMFLOAT3 angularVelocity_ = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 torque_ = { 0.0f, 0.0f, 0.0f };
    XMFLOAT4 orientation_ = { 0.0f, 0.0f, 0.0f, 1.0f };

    XMFLOAT3 force_ = { 0.0f, 0.0f, 0.0f };

    // bounds
    BoundingSphere localBoundingSphere_ = {};
    BoundingSphere boundingSphere_ = {};
    BoundingOrientedBox localBoundingBox_ = {};
    BoundingOrientedBox boundingBox_ = {};

public:
    void ApplyForce(const XMFLOAT3& f)
    {
        force_.x += f.x;
        force_.y += f.y;
        force_.z += f.z;
    }

    void Integrate(float dt)
    {
        if (inverseMass_ <= 0.0f) return; // 고정체는 무시

        velocity_.x += (force_.x * inverseMass_) * dt;
        velocity_.y += (force_.y * inverseMass_) * dt;
        velocity_.z += (force_.z * inverseMass_) * dt;

        position_.x += velocity_.x * dt;
        position_.y += velocity_.y * dt;
        position_.z += velocity_.z * dt;

        force_ = { 0.0f, 0.0f, 0.0f };
    }
};
