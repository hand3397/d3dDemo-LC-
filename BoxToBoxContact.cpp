#include "BoxToBoxContact.h"

namespace spe {;

BoxToBoxContact::BoxToBoxContact(Fixture* fixtureA, Fixture* fixtureB) :
	Contact(fixtureA, fixtureB) { }

Contact* BoxToBoxContact::Create(Fixture* fixtureA, Fixture* fixtureB)
{
    return new BoxToBoxContact(fixtureA, fixtureB);
}

void BoxToBoxContact::FindCollisionPoints(const ConvexInfo& boxA, const ConvexInfo& boxB,
	CollisionInfo& collisionInfo, ResultEPA& resultEPA, Polytope* simplexArray)
{
	Face refFace, incFace;
	SetBoxFace(refFace, boxA, resultEPA.normal);
	SetBoxFace(incFace, boxB, -resultEPA.normal);

	ContactFace contactFace;
	ComputeContactPolygon(contactFace, refFace, incFace);
	BuildManifoldFromPolygon(collisionInfo, refFace, incFace, contactFace, resultEPA);
}

}