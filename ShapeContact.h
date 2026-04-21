#pragma once
#include "Contact.h"

namespace spe {

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
            return new ClippingContact<TypeA, TypeB>(a, b);
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

}

