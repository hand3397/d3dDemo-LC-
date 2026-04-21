#include "SphereToCylinderContact.h"

namespace spe {;

SphereToCylinderContact::SphereToCylinderContact(Fixture* fixtureA, Fixture* fixtureB) :
    Contact(fixtureA, fixtureB)
{
}

Contact* SphereToCylinderContact::Create(Fixture* fixtureA, Fixture* fixtureB)
{
    return new SphereToCylinderContact(fixtureA, fixtureB);
}

void SphereToCylinderContact::FindCollisionPoints(const ConvexInfo& sphereA, const ConvexInfo& cylinderB,
    CollisionInfo& collisionInfo, ResultEPA& resultEPA, Polytope* simplexArray)
{
    XMStoreFloat3(&collisionInfo.normal[0], resultEPA.normal);
    collisionInfo.separation[0] = resultEPA.distance;
    XMStoreFloat3(&collisionInfo.pointA[0], 
        XMLoadFloat3(&sphereA.center) + resultEPA.normal * sphereA.radius);
    XMStoreFloat3(&collisionInfo.pointB[0], 
        XMLoadFloat3(&collisionInfo.pointA[0]) - resultEPA.normal * collisionInfo.separation[0]);
    ++collisionInfo.size;
}

}
