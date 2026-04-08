#include "RigidBody.h"
#include "GameObject.h"
#include "Contact.h"

namespace spe { ; 

Rigidbody::Rigidbody()
{
    CalculateMatrix();
}

Rigidbody::Rigidbody(RigidbodyType type, const XMFLOAT4& rotateQuat, const XMFLOAT3& position) :
    type_(type), orientation_(rotateQuat), position_(position)
{
    if (type == RigidbodyType::STATIC) {
        // invMass = 0 -> 질량을 무한대로 설정
        SetMass(0.0f);
    }

    CalculateMatrix();

    if (type != RigidbodyType::STATIC) {
        SetFlag(RigidbodyFlag::AWAKE);
    }
}

Rigidbody::~Rigidbody()
{
    if (fixture_ != nullptr) {
        delete fixture_;
        fixture_ = nullptr;
    }
}

void Rigidbody::AddForce(const XMFLOAT3& force)
{
    force_.x += force.x;
    force_.y += force.y;
    force_.z += force.z;
}

void Rigidbody::AddTorque(const XMFLOAT3& torque)
{
    torque_.x += torque.x;
    torque_.y += torque.y;
    torque_.z += torque.z;
}

void Rigidbody::AddLinearAcc(const XMFLOAT3& linearAcc)
{
    linearAcceleration_.x += linearAcc.x;
    linearAcceleration_.y += linearAcc.y;
    linearAcceleration_.z += linearAcc.z;
}

void Rigidbody::AddLinearVelocity(const XMFLOAT3& linearVelocity)
{
    linearVelocity_.x += linearVelocity.x;
    linearVelocity_.y += linearVelocity.y;
    linearVelocity_.z += linearVelocity.z;
}

void Rigidbody::ClearForces()
{
    force_ = { 0.f, 0.f, 0.f };
    torque_ = { 0.f, 0.f, 0.f };
}

void Rigidbody::ClearAcclerations()
{
    linearAcceleration_ = { 0.f, 0.f, 0.f };
    angularAcceleration_ = { 0.f, 0.f, 0.f };
}

void Rigidbody::CreateFixture(Shape* shape)
{
    Fixture* fixture = new Fixture(shape);

    numFixtures_ = 1;
    fixture_ = fixture;
}

void Rigidbody::CalculateMatrix()
{
    XMVECTOR q = XMVector4Normalize(XMLoadFloat4(&orientation_));

    XMMATRIX R = XMMatrixRotationQuaternion(q);
    XMMATRIX T = XMMatrixTranslation(position_.x, position_.y, position_.z);

    XMStoreFloat4x4(&transformMatrix_, R * T);

    XMMATRIX invILocal = XMLoadFloat3x3(&inverseInertiaTensor_);
    XMMATRIX invIWorld = R * invILocal * XMMatrixTranspose(R);

    XMStoreFloat3x3(&inverseInertiaTensorWorld_, invIWorld);
}

void Rigidbody::Integrate(float dt)
{
    // Static인 경우 물체는 움직이지 않는다.
    if (type_ == RigidbodyType::STATIC) {
        return;
    }

    // Set acceleration by F = ma
    XMVECTOR linearAccVec = XMLoadFloat3(&linearAcceleration_);
    linearAccVec += XMLoadFloat3(&force_) * inverseMass_;

    // set angular acceleration
    XMVECTOR angularAccVec = XMLoadFloat3(&angularAcceleration_);
    angularAccVec += XMVector3TransformNormal(XMLoadFloat3(&torque_), XMLoadFloat3x3(&inverseInertiaTensorWorld_));

    // set velocity by accerleration
    XMVECTOR linearVelocityVec = XMLoadFloat3(&linearVelocity_);
    XMVECTOR angularVelocityVec = XMLoadFloat3(&angularVelocity_);
    linearVelocityVec += (linearAccVec * dt);
    angularVelocityVec += (angularAccVec * dt);

    // impose drag
    linearVelocityVec *= std::max(0.f, (1.0f - linearDamping_ * dt));
    angularVelocityVec *= std::max(0.f, (1.0f - angularDamping_ * dt));

    // store velocity
    XMStoreFloat3(&linearVelocity_, linearVelocityVec);
    XMStoreFloat3(&angularVelocity_, angularVelocityVec);

    // set sweep (previous Transform)
    sweep_.position = position_;
    sweep_.orientation = orientation_;

    // set position
    XMVECTOR positionVec = XMLoadFloat3(&position_);
    positionVec += (linearVelocityVec * dt);
    XMStoreFloat3(&position_, positionVec);

    XMVECTOR orientationVec = XMLoadFloat4(&orientation_);

    float omegaMag = Vec3Length(angularVelocityVec);
    if (omegaMag > EPS_FLOAT_SQ) // 회전이 있을 때만 계산
    {
        // 각속도 벡터 방향을 회전축, 크기*dt를 회전각으로 하는 쿼터니언 생성
        // 이 쿼터니언(deltaQ)은 월드 좌표계 기준의 회전량이다
        XMVECTOR deltaQ = XMQuaternionRotationAxis(angularVelocityVec, omegaMag * dt);

        // 회전 적용: Q_new = deltaQ * Q_old
        // DirectXMath의 곱셈 순서는 (Q2 * Q1)이 "Q1 회전 후 Q2 회전"을 의미하므로,
        // 현재 방향(Q_old)에 월드 회전(deltaQ)을 추가하려면 왼쪽에 곱해야 합니다.
        orientationVec = XMQuaternionMultiply(orientationVec, deltaQ);
        orientationVec = XMQuaternionNormalize(orientationVec);
    }

    XMStoreFloat4(&orientation_, orientationVec);

    // Synchronize the transform matrix with the current position and orientation
    CalculateMatrix();

    ClearForces();
    ClearAcclerations();
}

void Rigidbody::UpdateSweep()
{
    sweep_.position = position_;
    sweep_.orientation = orientation_;
}

void Rigidbody::SynchronizeFixture(BroadPhase* broadPhase)
{
    if (fixture_ == nullptr)
        return;

    XMVECTOR quat = XMVector4Normalize(XMLoadFloat4(&sweep_.orientation));

    XMMATRIX r = XMMatrixRotationQuaternion(quat);
    XMMATRIX t = XMMatrixTranslationFromVector(XMLoadFloat3(&sweep_.position));
    XMMATRIX xf1 = r * t;

    // fixture의 이동을 broadPhase 와 동기화 -> dynamicTree의 node 이동
    fixture_->Synchronize(broadPhase, xf1, XMLoadFloat4x4(&transformMatrix_));
}

void Rigidbody::AddFixture(Fixture* fixture)
{
    fixture->SetRigidbody(this);
    // 우선 fixture의 수를 rigidbody당 1개로 제한
    numFixtures_ = 1;
    fixture_ = fixture;
}

GameObject* Rigidbody::GetGameObject() const
{
    return gameObject_;
}

RigidbodyType Rigidbody::GetType()const
{
    return type_;
}

float Rigidbody::GetMass()const
{
    return mass_;
}

float Rigidbody::GetInvMass()const
{
    return inverseMass_;
}

XMFLOAT3 Rigidbody::GetPosition()const
{
    return position_;
}

XMFLOAT3 Rigidbody::GetLinearVelocity()const
{
    return linearVelocity_;
}

XMFLOAT3 Rigidbody::GetLinearAcceleration()const
{
    return linearAcceleration_;
}

XMFLOAT4 Rigidbody::GetOrientation()const
{
    return orientation_;
}

XMFLOAT3 Rigidbody::GetAngularVelocity()const
{
    return angularVelocity_;
}

XMFLOAT3 Rigidbody::GetAngularAcceleration()const
{
    return angularAcceleration_;
}

XMFLOAT3X3 Rigidbody::GetInverseInertiaTensorWorld() const
{
    return inverseInertiaTensorWorld_;
}

XMFLOAT3X3 Rigidbody::GetInverseInertiaTensor() const
{
    return inverseInertiaTensor_;
}

XMMATRIX Rigidbody::GetTransformMatrix() const
{
    return XMLoadFloat4x4(&transformMatrix_);
}

XMFLOAT4X4 Rigidbody::GetTransformMatrixf() const
{
    return transformMatrix_;
}

float Rigidbody::GetLinearDamping() const
{
    return linearDamping_;
}

float Rigidbody::GetAngularDamping() const
{
    return angularDamping_;
}

Fixture* Rigidbody::GetFixture()
{
    return fixture_;
}

int32_t Rigidbody::GetIslandId() const
{
    return islandId_;
}

ContactLink* Rigidbody::GetContactLink()
{
    return contactLink_;
}

bool Rigidbody::HasFlag(RigidbodyFlag flag) const
{
    return (flags_ & static_cast<uint32_t>(flag)) != 0;
}

Rigidbody* Rigidbody::GetNext() const
{
    return next_;
}

Rigidbody* Rigidbody::GetPrev() const
{
    return prev_;
}

void Rigidbody::SetGameObject(GameObject* gameObject)
{
    gameObject_ = gameObject;
}

void Rigidbody::SetMass(const float mass)
{
    mass_ = mass;

    if (mass == 0.f)
        inverseMass_ = 0.f;
    else
        inverseMass_ = 1.0f / mass;
}

void Rigidbody::SetPosition(const XMFLOAT3& position)
{
    position_ = position;
}

void Rigidbody::SetLinearVelocity(const XMFLOAT3& linearVelocity)
{
    linearVelocity_ = linearVelocity;
}

void Rigidbody::SetLinearAccelration(const XMFLOAT3& linearAccelration)
{
    linearAcceleration_ = linearAccelration;
}

void Rigidbody::SetOrientation(const XMFLOAT4& orientation)
{
    orientation_ = orientation;
}

void Rigidbody::SetAngularVelocity(const XMFLOAT3& angularVelocity)
{
    angularVelocity_ = angularVelocity;
}

void Rigidbody::SetAngularAccelration(const XMFLOAT3& angularAcceleration)
{
    angularAcceleration_ = angularAcceleration;
}

void Rigidbody::SetLinearDamping(const float linearDamping)
{
    linearDamping_ = linearDamping;
}

void Rigidbody::SetAngularDamping(const float angularDamping)
{
    angularDamping_ = angularDamping;
}

void Rigidbody::SetAwake(bool awake)
{
    if (awake) {
        SetFlag(RigidbodyFlag::AWAKE);
        sleepTime_ = 0.f;
    }
    else {
        ClearFlag(RigidbodyFlag::AWAKE);
        ClearAcclerations();
    }
}

void Rigidbody::AddSleepTime(float dt)
{
    sleepTime_ += dt;
}

float Rigidbody::GetSleepTime() const
{
    return sleepTime_;
}

void Rigidbody::SetFlag(RigidbodyFlag flag)
{
    flags_ |= static_cast<uint32_t>(flag);
}

void Rigidbody::ClearFlag(RigidbodyFlag flag)
{
    flags_ &= ~static_cast<uint32_t>(flag);
}

void Rigidbody::SetIslandId(int32_t id)
{
    islandId_ = id;
}

void Rigidbody::SetContactLink(ContactLink* contactLink)
{
    contactLink_ = contactLink;
}

void Rigidbody::SetNext(Rigidbody* next)
{
    next_ = next;
}

void Rigidbody::SetPrev(Rigidbody* prev)
{
    prev_ = prev;
}

void Rigidbody::ComputeInertiaTensor()
{
    if (type_ == RigidbodyType::STATIC) {
        inverseInertiaTensor_ = XMFLOAT3X3(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        inverseInertiaTensorWorld_ = XMFLOAT3X3(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }

    // 첫 fixture만 inertiaTensor를 구하는데 사용됨.
    if (fixture_ != nullptr) {
        XMStoreFloat3x3(&inverseInertiaTensor_, fixture_->GetShape()->ComputeLocalInvInertia(mass_));
    }
    CalculateMatrix();
}

} // namespace spe



