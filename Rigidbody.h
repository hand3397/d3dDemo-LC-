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
public:

    Rigidbody() = default;
    Rigidbody(uint8_t type,
        XMFLOAT3 scale, XMFLOAT4 rotateQuat, XMFLOAT3 position);
    Rigidbody(uint8_t type,
        XMFLOAT3 scale, XMFLOAT4 rotateQuat, XMFLOAT3 position,
        BoundingSphere boundingSphere, BoundingOrientedBox boundingBox);

    void UpdateBounds();
    void ApplyForce(const XMFLOAT3& f);
    void Integrate(float dt);

public:

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
};
