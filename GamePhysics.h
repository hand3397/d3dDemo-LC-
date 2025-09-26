#pragma once
#include "d3dUtil.h"

#pragma once
#include "stdafx.h"

// 단순 3D RigidBody 구조체
struct RigidBody
{
    // 물리 속성
    float mass = 1.0f;           // 질량
    float inverseMass = 1.0f;    // 질량 역수, 0이면 고정
    bool isStatic = false;        // 고정 객체인지 여부

    // 변환 정보
    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 velocity = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 acceleration = { 0.0f, 0.0f, 0.0f };

    // 회전/각속도
    DirectX::XMFLOAT3 rotation = { 0.0f, 0.0f, 0.0f };       // Euler angles
    DirectX::XMFLOAT3 angularVelocity = { 0.0f, 0.0f, 0.0f };

    // 물리적 마찰
    float linearDamping = 0.98f;   // 속도 감소 계수
    float angularDamping = 0.98f;  // 각속도 감소 계수

    // 충돌/크기 정보 (단순 AABB)
    DirectX::XMFLOAT3 halfSize = { 0.5f, 0.5f, 0.5f }; // 박스의 절반 크기

    // 기본 생성자
    RigidBody() = default;

    // 속도 초기화 함수
    void ApplyForce(const DirectX::XMFLOAT3& force, float dt)
    {
        if (isStatic || inverseMass == 0.0f) return;

        acceleration.x += force.x * inverseMass;
        acceleration.y += force.y * inverseMass;
        acceleration.z += force.z * inverseMass;

        velocity.x += acceleration.x * dt;
        velocity.y += acceleration.y * dt;
        velocity.z += acceleration.z * dt;

        // 가속 초기화
        acceleration = { 0.0f, 0.0f, 0.0f };
    }

    // 위치 업데이트 함수
    void Integrate(float dt)
    {
        if (isStatic || inverseMass == 0.0f) return;

        // 선형 운동
        velocity.x *= linearDamping;
        velocity.y *= linearDamping;
        velocity.z *= linearDamping;

        position.x += velocity.x * dt;
        position.y += velocity.y * dt;
        position.z += velocity.z * dt;

        // 회전 운동 (간단 Euler 통합)
        angularVelocity.x *= angularDamping;
        angularVelocity.y *= angularDamping;
        angularVelocity.z *= angularDamping;

        rotation.x += angularVelocity.x * dt;
        rotation.y += angularVelocity.y * dt;
        rotation.z += angularVelocity.z * dt;
    }
};


class PhysicsManager
{
};

