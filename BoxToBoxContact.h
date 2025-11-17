#pragma once
#include "Contact.h"

namespace spe {;

class BoxToBoxContact : public Contact
{
public:
    BoxToBoxContact(Fixture* fixtureA, Fixture* fixtureB);
    static Contact* Create(Fixture* fixtureA, Fixture* fixtureB);
protected:
    virtual void findCollisionPoints(const ConvexInfo& boxA, const ConvexInfo& boxB, CollisionInfo& collisionInfo,
        ResultEPA& resultEPA, Simplex& simplexArray) override;
};

}


