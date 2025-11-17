#include "SphereToSphereContact.h"

namespace spe {; 

SphereToSphereContact::SphereToSphereContact(Fixture* fixtrueA, Fixture* fixtureB) :
    Contact(fixtrueA, fixtureB) { }

Contact* SphereToSphereContact::Create(Fixture* fixtureA, Fixture* fixtureB)
{
	return new SphereToSphereContact(fixtureA, fixtureB);
}

void SphereToSphereContact::findCollisionPoints(const ConvexInfo& sphereA, const ConvexInfo& sphereB, 
	CollisionInfo& collisionInfo, ResultEPA& resultEPA, Simplex& simplexArray)
{
	XMStoreFloat3(&collisionInfo.normal[0], resultEPA.normal);
	collisionInfo.seperation[0] = resultEPA.dist;
	XMStoreFloat3(&collisionInfo.pointA[0], XMLoadFloat3(&sphereA.center) + (resultEPA.normal * sphereA.radius));
	XMStoreFloat3(&collisionInfo.pointB[0], XMLoadFloat3(&collisionInfo.pointA[0])
		- (XMLoadFloat3(&collisionInfo.normal[0]) * collisionInfo.seperation[0]));
	++collisionInfo.size;
}

} // namespace spe


