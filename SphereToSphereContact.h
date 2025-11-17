#pragma once
#include "Contact.h"

namespace spe {;

class SphereToSphereContact : public Contact
{
public:
    SphereToSphereContact(Fixture* fixtureA, Fixture* fixtureB);
    static Contact* Create(Fixture* fixtureA, Fixture* fixtureB);

protected:
    virtual void findCollisionPoints(const ConvexInfo& sphereA, const ConvexInfo& sphereB, CollisionInfo& collisionInfo,
        ResultEPA& resultEPA, Simplex& simplexArray) override;
};

} // namespace spe



