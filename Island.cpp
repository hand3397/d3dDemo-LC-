#include "Island.h"
#include "ContactSolver.h"

namespace spe {;

const uint32_t Island::VELOCITY_ITERATION = 10;
const uint32_t Island::POSITION_ITERATION = 10;
const float Island::STOP_LINEAR_VELOCITY = 0.1f;
const float Island::STOP_ANGULAR_VELOCITY = 0.1f;

Island::Island(uint32_t bodyCount, uint32_t contactCount) :
    positions_(nullptr), velocities_(nullptr)
{
    bodies_ = new Rigidbody * [bodyCount];
    contacts_ = new Contact * [contactCount];

    numBodies_ = 0;
    numContacts_ = 0;
}

void Island::Solve(float duration)
{
    if (numBodies_ <= 1) {
        return;
    }

    positions_ = new PositionBuffer[numBodies_];
    velocities_ = new VelocityBuffer[numBodies_];

    // rigidbody의 position과 velocity를 buffer에 기록
    for (uint32_t i = 0; i < numBodies_; ++i) {
        positions_[i].position = bodies_[i]->GetPosition();
        positions_[i].positionBuffer = XMFLOAT3(0.0f, 0.0f, 0.0f);
        
        velocities_[i].linearVelocity = bodies_[i]->GetLinearVelocity();
        velocities_[i].angularVelocity = bodies_[i]->GetAngularVelocity();
        velocities_[i].linearVelocityBuffer = XMFLOAT3(0.0f, 0.0f, 0.0f);
        velocities_[i].angularVelocityBuffer = XMFLOAT3(0.0f, 0.0f, 0.0f);
    }

    // contactSolve중 변경된 position과 velocity는 buffer기록된 뒤 나중에 실제 값에 적용한다.
    ContactSolver contactSolver(duration, contacts_, positions_, velocities_, numBodies_, numContacts_);

    // 속도 제약 반복 횟수만큼 반복
    for (uint32_t i = 0; i < VELOCITY_ITERATION; ++i) {
        contactSolver.solveVelocityConstraints();
    }

    // 위치 제약 처리 반복
    for (uint32_t i = 0; i < POSITION_ITERATION; ++i) {
        contactSolver.solvePositionConstraints();
    }

    contactSolver.checkSleepContact();

    // 위치, 회전, 속도 업데이트
    for (int32_t i = 0; i < numBodies_; ++i) {

        Rigidbody* body = bodies_[i];
        if (body->GetType() == RigidbodyType::STATIC) {
            continue;
        }

        if (positions_[i].isNormalStop && positions_[i].isTangentStop && positions_[i].isNormal &&
            XMVectorGetX(XMVector3LengthEst(XMLoadFloat3(&velocities_[i].linearVelocity))) < STOP_LINEAR_VELOCITY &&
            XMVectorGetX(XMVector3LengthEst(XMLoadFloat3(&velocities_[i].angularVelocity))) < STOP_ANGULAR_VELOCITY) {
            velocities_[i].linearVelocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
            velocities_[i].angularVelocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
            body->SetSleep();
        }
        else {
            body->SetAwake();
        }

        //body->updateSweep();
        XMFLOAT3 newPostion;
        XMStoreFloat3(&newPostion, 
            XMLoadFloat3(&positions_[i].position) + XMLoadFloat3(&positions_[i].positionBuffer));
        body->SetPosition(newPostion);
        body->SetLinearVelocity(velocities_[i].linearVelocity);
        body->SetAngularVelocity(velocities_[i].angularVelocity);
        //body->synchronizeFixtures();
    }

    contactSolver.destroy();

    delete[] positions_;
    delete[] velocities_;
}

void Island::Destroy()
{
}

void Island::Add(Rigidbody* body)
{
    body->SetIslandId(numBodies_);
    bodies_[numBodies_++] = body;
}

void Island::Add(Contact* contact)
{
    contacts_[numContacts_++] = contact;
}

void Island::Clear()
{
    delete[] bodies_;
    bodies_ = nullptr;
    delete[] contacts_;
    contacts_ = nullptr;
    numBodies_ = 0;
    numContacts_ = 0;
}

}