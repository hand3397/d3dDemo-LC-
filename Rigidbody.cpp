#include "RigidBody.h"

Rigidbody::Rigidbody()
{
    CalculateMatrix();
}

Rigidbody::Rigidbody(RigidbodyType type, const XMFLOAT4& rotateQuat, const XMFLOAT3& position) :
    type_(type), orientation_(rotateQuat), position_(position)
{
    CalculateMatrix();
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
    fixtures_ = fixture;
}

void Rigidbody::CalculateMatrix()
{
    XMVECTOR quat = XMVector4Normalize(XMLoadFloat4(&orientation_));

    XMMATRIX r = XMMatrixRotationQuaternion(quat);
    XMMATRIX t = XMMatrixTranslation(position_.x, position_.y, position_.z);

    XMStoreFloat4x4(&transformMatrix_, r * t);

    XMFLOAT3X3 rot3x3;
    XMStoreFloat3x3(&rot3x3, r);
    XMMATRIX R = XMLoadFloat3x3(&rot3x3);

    // inverseInertiaTensorWorld_ = R * inverseInertiaTensor_ * R^T
    XMStoreFloat3x3(&inverseInertiaTensorWorld_, (R * XMLoadFloat3x3(&inverseInertiaTensor_)) * XMMatrixTranspose(R));
}

void Rigidbody::Integrate(float dt)
{
    // Static인 경우 물체는 움직이지 않는다.
    if (type_ == RigidbodyType::Static) {
        return;
    }

    // Set acceleration by F = ma
    XMVECTOR linearAccVec = XMLoadFloat3(&force_) * inverseMass_;

    // gravity
    //addGravity();

    // set angular acceleration
    XMVECTOR angularAccVec = XMVector3TransformNormal(XMLoadFloat3(&torque_), XMLoadFloat3x3(&inverseInertiaTensorWorld_));

    // set velocity by accerleration
    XMVECTOR linearVelocityVec = XMLoadFloat3(&linearVelocity_);
    XMVECTOR angularVelocityVec = XMLoadFloat3(&angularVelocity_);
    linearVelocityVec += (XMLoadFloat3(&linearAcceleration_) * dt);
    angularVelocityVec += (XMLoadFloat3(&angularAcceleration_) * dt);

    // impose drag
    linearVelocityVec *= (1.0f - linearDamping_);
    angularVelocityVec *= (1.0f - angularDamping_);

    // set sweep (previous Transform)
    //m_sweep.p = m_xf.position;
    //m_sweep.q = m_xf.orientation;

    // set position
    XMVECTOR positionVec = XMLoadFloat3(&position_);
    positionVec += (linearVelocityVec * dt);
    XMStoreFloat3(&position_, positionVec);

    // set orientation
    XMVECTOR angularVelocityQuat = XMVectorSetW(angularVelocityVec, 0.0f);                      // 각속도를 쿼터니언으로 변환
    XMVECTOR orientationVec = XMLoadFloat4(&orientation_);
    orientationVec = XMVectorAdd(orientationVec,
        XMVectorScale(XMQuaternionMultiply(angularVelocityQuat, orientationVec), 0.5f * dt));   // 쿼터니언 미분 공식
    orientationVec = XMQuaternionNormalizeEst(orientationVec);                                  // 정규화하여 안정성 유지
    XMStoreFloat4(&orientation_, orientationVec);

    // Synchronize the transform matrix with the current position and orientation
    CalculateMatrix();

    ClearForces(); 
    ClearAcclerations();
}

void Rigidbody::AddFixture(Fixture* fixture)
{
    // 우선 fixture의 수를 rigidbody당 1개로 제한
    numFixtures_ = 1;
    fixtures_ = fixture;
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

float Rigidbody::GetLinearDamping() const
{
    return linearDamping_;
}

float Rigidbody::GetAngularDamping() const
{
    return angularDamping_;
}

bool Rigidbody::isGrounded()const
{
    return isGrounded_;
}

bool Rigidbody::isAwake()const
{
    return isAwake_;
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

void Rigidbody::SetSleep()
{
    isAwake_ = false;
}

void Rigidbody::SetAwake()
{
    isAwake_ = true;
}

