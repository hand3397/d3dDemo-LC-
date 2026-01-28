#include "CylinderToCylinderContact.h"

namespace spe {;

CylinderToCylinderContact::CylinderToCylinderContact(Fixture* fixtureA, Fixture* fixtureB) :
	Contact(fixtureA, fixtureB)
{
}

Contact* CylinderToCylinderContact::Create(Fixture* fixtureA, Fixture* fixtureB)
{
	return new CylinderToCylinderContact(fixtureA, fixtureB);
}

void CylinderToCylinderContact::FindCollisionPoints(const ConvexInfo& cylinderA, const ConvexInfo& cylinderB, 
	CollisionInfo& collisionInfo, ResultEPA& resultEPA, Polytope* simplexArray)
{
	Face refFace, incFace;
	SetCylinderFace(refFace, cylinderA, resultEPA.normal);
	SetCylinderFace(incFace, cylinderB, -resultEPA.normal);

	ContactFace contactFace;
	ComputeContactPolygon(contactFace, refFace, incFace);

	BuildManifoldFromPolygon(collisionInfo, refFace, incFace, contactFace, resultEPA);
}

}

