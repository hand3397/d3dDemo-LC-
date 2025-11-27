#pragma once
#include "Contact.h"

namespace spe {; 

class SphereToBoxContact : public Contact
{
public:
    SphereToBoxContact(Fixture* fixtureA, Fixture* fixtureB);
    static Contact* Create(Fixture* fixtureA, Fixture* fixtureB);

protected:
    virtual void FindCollisionPoints(const ConvexInfo& sphereA, const ConvexInfo& boxB,
        CollisionInfo& collisionInfo, ResultEPA& resultEPA, Polytope& simplexArray) override;
};

}


