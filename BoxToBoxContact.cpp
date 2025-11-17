#include "BoxToBoxContact.h"

namespace spe {;

BoxToBoxContact::BoxToBoxContact(Fixture* fixtureA, Fixture* fixtureB) :
	Contact(fixtureA, fixtureB) { }

Contact* BoxToBoxContact::Create(Fixture* fixtureA, Fixture* fixtureB)
{
    return new BoxToBoxContact(fixtureA, fixtureB);
}

void BoxToBoxContact::findCollisionPoints(const ConvexInfo& sphereA, const ConvexInfo& boxB, CollisionInfo& collisionInfo, ResultEPA& resultEPA, Simplex& simplexArray)
{

}

}