#include "SphereToSphereContact.h"

namespace spe {; 

SphereToSphereContact::SphereToSphereContact(Fixture* fixtrueA, Fixture* fixtureB) :
    Contact(fixtrueA, fixtureB) { }

Contact* SphereToSphereContact::Create(Fixture* fixtureA, Fixture* fixtureB)
{
	return new SphereToSphereContact(fixtureA, fixtureB);
}

void SphereToSphereContact::Evaluate(Manifold& manifold, const XMMATRIX& transformA, const XMMATRIX& transformB)
{
    Shape* shapeA = fixtureA_->GetShape();
    Shape* shapeB = fixtureB_->GetShape();

    ConvexInfo convexA, convexB;
    shapeA->GetConvexInfo(transformA, convexA);
    shapeB->GetConvexInfo(transformB, convexB);

    CollisionInfo collisionInfo;
    collisionInfo.size = 0;

    // 구vs구 충돌의 경우 GJK -> EPA를 사용하지 않아도 단순계산으로 빠르게 충돌여부를 판단할 수 있기 때문에 예외처리를 한다.
    bool isCollide = IsCollide(convexA, convexB);

    if (!isCollide) {
        FreeConvexInfo(convexA, convexB);
        return;
    }

    ResultEPA resultEPA = GetResultEPA(convexA, convexB);

    if (resultEPA.distance == -1.0f) {
        FreeConvexInfo(convexA, convexB);
        return;
    }

    FindCollisionPoints(convexA, convexB, collisionInfo, resultEPA, nullptr);
    GenerateManifolds(collisionInfo, manifold, fixtureA_, fixtureB_);

    FreeConvexInfo(convexA, convexB);
}

void SphereToSphereContact::FindCollisionPoints(const ConvexInfo& sphereA, const ConvexInfo& sphereB, 
    CollisionInfo& collisionInfo, ResultEPA& resultEPA, Polytope* simplexArray)
{
    // FindCollisionPoints
    XMStoreFloat3(&collisionInfo.normal[0], resultEPA.normal);
    collisionInfo.separation[0] = resultEPA.distance;
    XMStoreFloat3(&collisionInfo.pointA[0], XMLoadFloat3(&sphereA.center) + (resultEPA.normal * sphereA.radius));
    XMStoreFloat3(&collisionInfo.pointB[0], XMLoadFloat3(&collisionInfo.pointA[0])
        - (XMLoadFloat3(&collisionInfo.normal[0]) * collisionInfo.separation[0]));
    ++collisionInfo.size;
}

bool SphereToSphereContact::IsCollide(const ConvexInfo& convexA, const ConvexInfo& convexB)
{
    XMVECTOR centerAB = XMLoadFloat3(&convexB.center) - XMLoadFloat3(&convexA.center);
    float sumRadius = convexA.radius + convexB.radius;
    // 반지름의 합이 구의 중심길이보다 긴가?
    return ((sumRadius * sumRadius) > Vec3LengthSq(centerAB));
}

ResultEPA SphereToSphereContact::GetResultEPA(const ConvexInfo& convexA, const ConvexInfo& convexB)
{
    ResultEPA result;

    XMVECTOR posA = XMLoadFloat3(&convexA.center);
    XMVECTOR posB = XMLoadFloat3(&convexB.center);

    // A에서 B로 향하는 벡터 계산
    XMVECTOR delta = posB - posA;

    // 두 구 중심 사이의 거리 계산
    XMVECTOR lengthVec = XMVector3Length(delta);
    float distance = XMVectorGetX(lengthVec);

    float sumRadius = convexA.radius + convexB.radius;

    // 예외 처리: 두 구의 중심이 완벽하게 겹쳐있을 경우 (Distance가 0에 가까움)
    // 정규화(Normalize) 시 0으로 나누는 오류가 발생하므로 임의의 축(Y축 등)을 노말 잡음
    if (distance <= EPS_FLOAT)
    {
        result.normal = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f); // 임의의 밀어낼 방향
        result.distance = sumRadius; // 완전히 겹쳤으므로 반지름 합만큼 밀어내야 함
    }
    else
    {
        // 충돌 법선(Normal): 중심 차이 벡터를 정규화 (A -> B 방향)
        // 이미 길이를 구했으므로 나눗셈으로 처리하는 것이 빠름
        result.normal = delta / lengthVec;

        // 침투 깊이(Penetration Depth): 반지름의 합 - 현재 거리
        // 양수 값이 나올 것이며, 이 값만큼 노말 방향으로 밀어내야 함
        result.distance = sumRadius - distance;
    }

    return result;
}

} // namespace spe


