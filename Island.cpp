#include "Island.h"
#include "ContactSolver.h"
#include "PhysicsWorld.h"

namespace spe {;

    const uint32_t Island::VELOCITY_ITERATION = 10;
    const uint32_t Island::POSITION_ITERATION = 10;
    const float Island::STOP_LINEAR_VELOCITY_SQ = 1.00f;
    const float Island::STOP_ANGULAR_VELOCITY_SQ = 1.00f;
    const float Island::SLEEP_START_TIME = 0.2f;

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
            contactSolver.SolveVelocityConstraints();
        }

        // 위치 제약 처리 반복
        for (uint32_t i = 0; i < POSITION_ITERATION; ++i) {
            contactSolver.SolvePositionConstraints();
        }

        contactSolver.CheckSleepContact();

        // 위치, 회전, 속도 업데이트
        for (int32_t i = 0; i < numBodies_; ++i) {

            Rigidbody* body = bodies_[i];
            if (body->GetType() == RigidbodyType::STATIC) {
                continue;
            }

            // [수정] 속도 계산 시 Buffer(충격량)를 포함하도록 수정
            XMVECTOR finallinearVel = XMLoadFloat3(&velocities_[i].linearVelocity) + XMLoadFloat3(&velocities_[i].linearVelocityBuffer);
            XMVECTOR finalAngularVel = XMLoadFloat3(&velocities_[i].angularVelocity) + XMLoadFloat3(&velocities_[i].angularVelocityBuffer);

            if (positions_[i].isNormalStop && positions_[i].isTangentStop && positions_[i].isSupported &&
                Vec3LengthSq(finallinearVel) < STOP_LINEAR_VELOCITY_SQ &&
                Vec3LengthSq(finalAngularVel) < STOP_ANGULAR_VELOCITY_SQ) {
                
                if (body->GetSleepTime() < SLEEP_START_TIME) {
                    body->AddSleepTime(duration);
                }
                else {
                    finallinearVel = XMVectorZero();
                    finalAngularVel = XMVectorZero();
                    body->SetAwake(false);
                }
            }
            else {
                body->SetAwake(true);
            }

            body->UpdateSweep();
            XMFLOAT3 newPostion;
            XMStoreFloat3(&newPostion,
                XMLoadFloat3(&positions_[i].position) + XMLoadFloat3(&positions_[i].positionBuffer));
            body->SetPosition(newPostion);

            XMFLOAT3 finalLinVelF3, finalAngVelF3;
            XMStoreFloat3(&finalLinVelF3, finallinearVel);
            XMStoreFloat3(&finalAngVelF3, finalAngularVel);

            body->SetLinearVelocity(finalLinVelF3);
            body->SetAngularVelocity(finalAngVelF3);
            //body->synchronizeFixtures();
        }

        // 위치, 회전, 속도 업데이트
        for (int32_t i = 0; i < numBodies_; ++i) {

            Rigidbody* body = bodies_[i];
            if (body->GetType() == RigidbodyType::STATIC) {
                continue;
            }

        }

        contactSolver.Destroy();

        delete[] positions_;
        delete[] velocities_;
    }

    void Island::Destroy()
    {
        delete[] bodies_;
        bodies_ = nullptr;
        delete[] contacts_;
        contacts_ = nullptr;
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
        numBodies_ = 0;
        numContacts_ = 0;
    }

    uint32_t Island::numBodies() const
    {
        return numBodies_;
    }

    Rigidbody** Island::GetBodies()
    {
        return bodies_;
    }

}
