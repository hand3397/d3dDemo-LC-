#pragma once
#include "Shape.h"


namespace spe {; 

class BroadPhase;
class Rigidbody;
class Fixture;

struct FixtureProxy
{
    AABB aabb;
    Fixture* fixture = nullptr;
    int32_t proxyId = -1;    // dynamicTree에서 사용할 node접근용 id
};

class Fixture
{
public:
    Fixture(Shape* shape);
    ~Fixture();

    void CreateProxy(BroadPhase* broadPhase);
    void DestroyProxy(BroadPhase* broadPhase);

    void Synchronize(BroadPhase* broadPhase, 
        const XMMATRIX& xf1, const XMMATRIX& xf2);

    Rigidbody* GetRigidbody()const;
    Shape* GetShape();
    ShapeType GetShapeType()const;
    float GetFriction()const;
    float GetRestitution()const;
    const FixtureProxy* GetFixtureProxy() const;

    void SetRigidbody(Rigidbody* rigidbody);
    void SetFriction(float friction);
    void SetRestitution(float restitution);

protected:
    Rigidbody* rigidbody_ = nullptr;

    ShapeType shapeType_ = ShapeType::BOX;
    Shape* shape_ = nullptr;

    //float density_ = 1.0f;
    float friction_ = 0.f;
    float restitution_ = 0.f;

    FixtureProxy* proxy_ = nullptr;
};

}


