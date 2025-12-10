#pragma once
#include "Contact.h"

namespace spe {;

class SphereToSphereContact : public Contact
{
public:
    SphereToSphereContact(Fixture* fixtureA, Fixture* fixtureB);
    static Contact* Create(Fixture* fixtureA, Fixture* fixtureB);

    // 구vs구 충돌은 GJK-EPA를 사용하지않고 간단한 연산을 통해 바로 충돌지점을 구할 수 있다.
    virtual void Evaluate(Manifold& manifold, const XMMATRIX& transformA, const XMMATRIX& transformB) override;
protected:
    virtual void FindCollisionPoints(const ConvexInfo& sphereA, const ConvexInfo& sphereB,
        CollisionInfo& collisionInfo, ResultEPA& resultEPA, Polytope& simplexArray) override;

    // 구vs구 충돌 검사용
    bool IsCollide(const ConvexInfo& convexA, const ConvexInfo& convexB);
    ResultEPA GetResultEPA(const ConvexInfo& convexA, const ConvexInfo& convexB);
};

} // namespace spe



