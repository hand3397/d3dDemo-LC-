#pragma once
#include "Fixture.h"
#include "Shape.h"

enum class RigidbodyType : uint8_t
{
    Static = 0,
    Dynamic,
    Kinematic,
    Count,
};

class Rigidbody
{
public:
    Rigidbody();
    Rigidbody(RigidbodyType type, const XMFLOAT4& rotateQuat, const XMFLOAT3& position);

    void AddForce(const XMFLOAT3& force);
    void AddTorque(const XMFLOAT3& torque);
    void ClearForces();
    void ClearAcclerations();
    void CreateFixture(Shape* shape);
    void CalculateMatrix();

    void Integrate(float dt);

    void AddFixture(Fixture* fixture);

    RigidbodyType GetType()const;
    float GetMass()const;
    float GetInvMass()const;
    XMFLOAT3 GetPosition()const;
    XMFLOAT3 GetLinearVelocity()const;
    XMFLOAT3 GetLinearAcceleration()const;
    XMFLOAT4 GetOrientation()const;
    XMFLOAT3 GetAngularVelocity()const;
    XMFLOAT3 GetAngularAcceleration()const;
    float GetLinearDamping()const;
    float GetAngularDamping()const;
    bool isGrounded()const;
    bool isAwake()const;

    void SetMass(const float mass);
    void SetPosition(const XMFLOAT3& position);
    void SetLinearVelocity(const XMFLOAT3& linearVelocity);
    void SetLinearAccelration(const XMFLOAT3& linearAccelration);
    void SetOrientation(const XMFLOAT4& orientation);
    void SetAngularVelocity(const XMFLOAT3& angularVelocity);
    void SetAngularAccelration(const XMFLOAT3& angularAcceleration);
    void SetLinearDamping(const float linearDamping);
    void SetAngularDamping(const float angularDamping);
    void SetSleep();
    void SetAwake();

public:

    RigidbodyType type_ = RigidbodyType::Static;

    float mass_ = 1.0f;
    float inverseMass_ = 1.0f; // 1.0f / mass

    XMFLOAT3 position_ = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 linearVelocity_ = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 linearAcceleration_ = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 force_ = { 0.0f, 0.0f, 0.0f };

    XMFLOAT4 orientation_ = { 0.0f, 0.0f, 0.0f, 1.0f };
    XMFLOAT3 angularVelocity_ = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 angularAcceleration_ = { 0.0f,0.0f, 0.0f };
    XMFLOAT3 torque_ = { 0.0f, 0.0f, 0.0f };

    XMFLOAT3X3 inverseInertiaTensorWorld_;
    XMFLOAT3X3 inverseInertiaTensor_;
    XMFLOAT4X4 transformMatrix_;

    // drag [0.f, 1.0f]
    float linearDamping_ = 0.1f;
    float angularDamping_ = 0.1f;

    // flag
    bool isGrounded_ = true;
    bool isAwake_ = false;

    // fixture
    Fixture* fixtures_ = nullptr;
    uint32_t numFixtures_ = 0;
};
