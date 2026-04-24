#pragma once
#include "Contact.h"

namespace spe {

    class SphereToConvexContact;
    template <ShapeType TypeA, ShapeType TypeB>
    class ClippingContact;
    class CharcterContact;

    // Sphere vs Any Convex (Box, Cylinder, Capsule 등) 템플릿
    class SphereToConvexContact : public Contact
    {
    public:
        SphereToConvexContact(Fixture* a, Fixture* b) : Contact(a, b) {}
        static Contact* Create(Fixture* a, Fixture* b)
        {
            return new SphereToConvexContact(a, b);
        }

    protected:
        // Contact 생성시 type이 가장 작은 쪽이 A가 되도록 되어있으므로, Sphere는 항상 A다.
        virtual void FindCollisionPoints(const ConvexInfo& sphereA, const ConvexInfo& convexB,
            CollisionInfo& collisionInfo, ResultEPA& resultEPA, Polytope* simplexArray) override
        {
            // 결과 저장
            XMStoreFloat3(&collisionInfo.normal[0], resultEPA.normal);
            collisionInfo.separation[0] = resultEPA.distance;

            // 구의 표면 점 (A)
            XMVECTOR pointA = XMLoadFloat3(&sphereA.center) + (resultEPA.normal * sphereA.radius);
            XMStoreFloat3(&collisionInfo.pointA[0], pointA);

            // 상대 물체 위의 점 (B) = A - (Normal * Distance)
            XMVECTOR pointB = pointA - (resultEPA.normal * collisionInfo.separation[0]);
            XMStoreFloat3(&collisionInfo.pointB[0], pointB);

            collisionInfo.size = 1;
        }
    };

    // Convex vs Convex (Cylinder vs Cylinder 등 Clipping 기반) 템플릿
    // 각 도형의 ShapeType을 받는다.
    template <ShapeType TypeA, ShapeType TypeB>
    class ClippingContact : public Contact
    {
    public:
        ClippingContact(Fixture* a, Fixture* b) : Contact(a, b) {}

        static Contact* Create(Fixture* a, Fixture* b)
        {
            // Character간의 충돌은 따로 설정
            if (a->GetRigidbody()->GetType() == RigidbodyType::KINEMATIC &&
                b->GetRigidbody()->GetType() == RigidbodyType::KINEMATIC)
                if (TypeA == ShapeType::CAPSULE && TypeB == ShapeType::CAPSULE)
                    return new CharcterContact(a, b);

            return new ClippingContact<TypeA, TypeB>(a, b);;
        }

    protected:

        virtual void FindCollisionPoints(const ConvexInfo& infoA, const ConvexInfo& infoB,
            CollisionInfo& collisionInfo, ResultEPA& resultEPA, Polytope* simplexArray) override
        {
            Face refFace, incFace;

            // 템플릿 인자로 받은 TypeA, TypeB에 따라 적절한 SetFace 함수 호출
            InvokeSetFace(TypeA, refFace, infoA, resultEPA.normal);
            InvokeSetFace(TypeB, incFace, infoB, -resultEPA.normal);

            ContactFace contactFace;
            this->ComputeContactPolygon(contactFace, refFace, incFace);
            this->BuildManifoldFromPolygon(collisionInfo, refFace, incFace, contactFace, resultEPA);
        }

    private:
        // Helper: 타입을 보고 실제 함수와 연결해주는 역할
        void InvokeSetFace(ShapeType type, Face& face, const ConvexInfo& info, const XMVECTOR& normal)
        {
            switch (type) {
                //ShapeType::SPHERE는 SphereToConvexContact에서 처리하므로 여기서는 다루지 않음
            case ShapeType::BOX:      this->SetBoxFace(face, info, normal); break;
            case ShapeType::CYLINDER: this->SetCylinderFace(face, info, normal); break;
            case ShapeType::CAPSULE:  this->SetCapsuleFace(face, info, normal); break;
                // 새로운 도형이 추가되면 여기에 한 줄만 추가하면 됨
            }
        }
    };

    class CharcterContact : public Contact
    {
    public:
        // CharacterContact는 rigidbody의 타입과 형체가 Kinematic과 capsule로 고정
        CharcterContact(Fixture* a, Fixture* b) : Contact(a, b) 
        {
            SetFlag(ContactFlag::CHARACTER);
        }

    protected:

        virtual void Evaluate(Manifold& manifold, const XMMATRIX& transformA, const XMMATRIX& transformB) override
        {
            // 두 캡슐은 모두 y축에 정렬되어있다.
            Shape* shapeA = fixtureA_->GetShape();
            Shape* shapeB = fixtureB_->GetShape();

            ConvexInfo convexA, convexB;
            shapeA->GetConvexInfo(transformA, convexA);
            shapeB->GetConvexInfo(transformB, convexB);

            // Y축 실린더 구간 중첩 확인
            float hA = convexA.height * 0.5f + convexA.radius; // 절반 높이
            float hB = convexB.height * 0.5f + convexB.radius;

            XMVECTOR centerA = XMLoadFloat3(&convexA.center);
            XMVECTOR centerB = XMLoadFloat3(&convexB.center);

            float minA = XMVectorGetY(centerA) - hA;
            float maxA = XMVectorGetY(centerA) + hA;
            float minB = XMVectorGetY(centerB) - hB;
            float maxB = XMVectorGetY(centerB) + hB;

            // Y축 구간이 겹치지 않으면 충돌 없음
            if (maxA < minB || maxB < minA) {
                FreeConvexInfo(convexA, convexB);
                return;
            }

            // 4. XZ 평면에서의 원형 충돌 검사
            XMVECTOR diff = centerA - centerB;
            // Y축 차이를 제거하여 수평 벡터 생성
            XMVECTOR diffXZ = XMVectorSet(XMVectorGetX(diff), 0.0f, XMVectorGetZ(diff), 0.0f);

            float distanceXZ = XMVectorGetX(XMVector3Length(diffXZ));
            float combinedRadius = convexA.radius + convexB.radius;

            if (distanceXZ < combinedRadius) {
                manifold.numPoints = 1;
                // 밀어낼 방향 (XZ 평면 노멀)
                // 거리가 0에 가까우면 임의의 방향(전방) 설정
                XMVECTOR normal = (distanceXZ > EPS_FLOAT) ? XMVector3Normalize(diffXZ) : XMVectorSet(0, 0, 1, 0);

                // pointB -> pointA 방향의 normalVector
                // 면 침투시 separation은 침투깊이의 크기만큼의 양수값
                ManifoldPoint& p = manifold.points[0];
                XMStoreFloat3(&p.normal, normal);
                p.separation = combinedRadius - distanceXZ;
                XMStoreFloat3(&p.pointA, centerA - normal * convexA.radius);
                XMStoreFloat3(&p.pointB, centerB + normal * convexB.radius);
                p.normalImpulse = 0.f;
                p.tangentImpulse = 0.f;
            }
            else {
                FreeConvexInfo(convexA, convexB);
                return;
            }

            FreeConvexInfo(convexA, convexB);
        }

        virtual void FindCollisionPoints(const ConvexInfo& convexA, const ConvexInfo& convexB,
            CollisionInfo& collisionInfo, ResultEPA& resultEPA, Polytope* simplexArray) override
        {
            // CharacterContact는 Evaluate에서 모든 처리를 하므로 비워둡니다.
        }
    };

}

