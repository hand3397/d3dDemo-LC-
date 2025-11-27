#include "SphereToBoxContact.h"

namespace spe {;

SphereToBoxContact::SphereToBoxContact(Fixture* fixtureA, Fixture* fixtureB) :
    Contact(fixtureA, fixtureB) {}

Contact* SphereToBoxContact::Create(Fixture* fixtureA, Fixture* fixtureB)
{
	return new SphereToBoxContact(fixtureA, fixtureB);
}

void SphereToBoxContact::FindCollisionPoints(const ConvexInfo& sphereA, const ConvexInfo& boxB,
	CollisionInfo& collisionInfo, ResultEPA& resultEPA, Polytope& simplexArray)
{
	XMStoreFloat3(&collisionInfo.normal[0], resultEPA.normal);
	collisionInfo.seperation[0] = resultEPA.dist;
	XMStoreFloat3(&collisionInfo.pointA[0], XMLoadFloat3(&sphereA.center) + (resultEPA.normal * sphereA.radius));
	XMStoreFloat3(&collisionInfo.pointB[0], XMLoadFloat3(&collisionInfo.pointA[0])
		- (XMLoadFloat3(&collisionInfo.normal[0]) * collisionInfo.seperation[0]));
	++collisionInfo.size;
}

}


