#include "SphereToBoxContact.h"

namespace spe {;

SphereToBoxContact::SphereToBoxContact(Fixture* fixtureA, Fixture* fixtureB) :
    Contact(fixtureA, fixtureB) {}

Contact* SphereToBoxContact::Create(Fixture* fixtureA, Fixture* fixtureB)
{
	return new SphereToBoxContact(fixtureA, fixtureB);
}

void SphereToBoxContact::FindCollisionPoints(const ConvexInfo& sphereA, const ConvexInfo& boxB,
	CollisionInfo& collisionInfo, ResultEPA& resultEPA, Polytope* simplexArray)
{
	XMStoreFloat3(&collisionInfo.normal[0], resultEPA.normal);
	collisionInfo.separation[0] = resultEPA.distance;
	XMVECTOR pointA = XMLoadFloat3(&sphereA.center) + sphereA.radius * resultEPA.normal;
	XMStoreFloat3(&collisionInfo.pointA[0], pointA);
	XMStoreFloat3(&collisionInfo.pointB[0], pointA - resultEPA.normal * collisionInfo.separation[0]);
	++collisionInfo.size;
}

}


