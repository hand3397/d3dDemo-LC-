#pragma once
#include "Fixture.h"
#include "Shape.h"

class GameObject;

namespace spe {;

struct ContactLink;
class BroadPhase;

enum class RigidbodyType
{
    STATIC = 0,
    DYNAMIC,
    KINEMATIC,
    COUNT,
};

enum class RigidbodyFlag : uint32_t
{
    ISLAND = (1 << 0),
    AWAKE = (1 << 1),
    GROUND = (1 << 2),
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
    void AddLinearVelocity(const XMFLOAT3& linearVelocity);
    void ClearForces();
    void ClearAcclerations();
    void CreateFixture(Shape* shape);
    void CalculateMatrix();

    void Integrate(float dt);
    void UpdateSweep();
    void SynchronizeFixture(BroadPhase* broadPhase);

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
    int32_t GetIslandId()const;
    ContactLink* GetContactLink();
    bool HasFlag(RigidbodyFlag flag)const;
    Rigidbody* GetNext() const;
    Rigidbody* GetPrev() const;

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

    void SetAwake(bool awake);
    void AddSleepTime(float dt);
    float GetSleepTime() const;
    void SetFlag(RigidbodyFlag flag);
    void ClearFlag(RigidbodyFlag flag);

    void SetIslandId(int32_t id);
    void SetContactLink(ContactLink* contactLink);
    void SetNext(Rigidbody* next);
    void SetPrev(Rigidbody* prev);

    void ComputeInertiaTensor();

protected:
    friend class GameObject;
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

    Sweep sweep_; // 이전 프레임 위치 기록용

    XMFLOAT3X3 inverseInertiaTensorWorld_   = XMFLOAT3X3(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    XMFLOAT3X3 inverseInertiaTensor_        = XMFLOAT3X3(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    XMFLOAT4X4 transformMatrix_;

    // drag [0.f, 1.0f]
    float linearDamping_ = 0.01f;
    float angularDamping_ = 0.01f;

    uint32_t flags_ = 0;
    float sleepTime_ = 0.f;

    // fixture
    Fixture* fixture_ = nullptr;
    uint32_t numFixtures_ = 0;

    // id
    int32_t islandId_ = -1;

    // 해당 body의 contact 묶음
    ContactLink* contactLink_ = nullptr;

    // physicsWorld
    Rigidbody* next_ = nullptr;
    Rigidbody* prev_ = nullptr;
};

}



