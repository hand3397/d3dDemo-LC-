#pragma once
#include "Fixture.h"
#include "Shape.h"

class GameObject;

namespace spe {;

enum class RigidbodyType
{
    STATIC = 0,
    DYNAMIC,
    KINEMATIC,
    COUNT,
};

class Rigidbody
{
public:
    Rigidbody();
    Rigidbody(RigidbodyType type, const XMFLOAT4& rotateQuat, const XMFLOAT3& position);
    ~Rigidbody();

    void AddForce(const XMFLOAT3& force);
    void AddTorque(const XMFLOAT3& torque);
    void AddLinearAcc(const XMFLOAT3& linearAcc);
    void ClearForces();
    void ClearAcclerations();
    void CreateFixture(Shape* shape);
    void CalculateMatrix();

    void Integrate(float dt);

    void AddFixture(Fixture* fixture);

    GameObject* GetGameObject()const;
    RigidbodyType GetType()const;
    float GetMass()const;
    float GetInvMass()const;
    XMFLOAT3 GetPosition()const;
    XMFLOAT3 GetLinearVelocity()const;
    XMFLOAT3 GetLinearAcceleration()const;
    XMFLOAT4 GetOrientation()const;
    XMFLOAT3 GetAngularVelocity()const;
    XMFLOAT3 GetAngularAcceleration()const;
    XMFLOAT3X3 GetInverseInertiaTensorWorld()const;
    XMFLOAT3X3 GetInverseInertiaTensor()const;
    XMMATRIX GetTransformMatrix()const;
    XMFLOAT4X4 GetTransformMatrixf()const;
    float GetLinearDamping()const;
    float GetAngularDamping()const;
    Fixture* GetFixture();
    bool isGrounded()const;
    bool isAwake()const;
    int32_t GetIslandId()const;

    void SetGameObject(GameObject* gameObject);
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
    void SetIslandId(int32_t id);

    void ComputeInertiaTensor();

public:

    RigidbodyType type_ = RigidbodyType::STATIC;

    GameObject* gameObject_ = nullptr;

    float mass_ = 1.0f;
    float inverseMass_ = 1.0f; // 1.0f / mass

    XMFLOAT3 position_              = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 linearVelocity_        = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 linearAcceleration_    = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 force_                 = { 0.0f, 0.0f, 0.0f };

    XMFLOAT4 orientation_           = { 0.0f, 0.0f, 0.0f, 1.0f };
    XMFLOAT3 angularVelocity_       = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 angularAcceleration_   = { 0.0f,0.0f, 0.0f };
    XMFLOAT3 torque_                = { 0.0f, 0.0f, 0.0f };

    XMFLOAT3X3 inverseInertiaTensorWorld_   = XMFLOAT3X3(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    XMFLOAT3X3 inverseInertiaTensor_        = XMFLOAT3X3(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    XMFLOAT4X4 transformMatrix_;

    // drag [0.f, 1.0f]
    float linearDamping_ = 0.001f;
    float angularDamping_ = 0.001f;

    // flag
    bool isGrounded_ = false;
    bool isAwake_ = false;

    // fixture
    Fixture* fixture_ = nullptr;
    uint32_t numFixtures_ = 0;

    // id
    int32_t islandId_ = -1;
};

}



