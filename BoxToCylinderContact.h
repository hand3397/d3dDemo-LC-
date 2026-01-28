#pragma once
#include "Contact.h"

namespace spe {;

class BoxToCylinderContact : public Contact
{
public:
    BoxToCylinderContact(Fixture* fixtureA, Fixture* fixtureB);
    static Contact* Create(Fixture* fixtureA, Fixture* fixtureB);
protected:
    virtual void FindCollisionPoints(const ConvexInfo& boxA, const ConvexInfo& cylinderB,
        CollisionInfo& collisionInfo, ResultEPA& resultEPA, Polytope* simplexArray) override;
};

}


