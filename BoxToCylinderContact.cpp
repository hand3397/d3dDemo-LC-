#include "BoxToCylinderContact.h"

namespace spe {;

BoxToCylinderContact::BoxToCylinderContact(Fixture* fixtureA, Fixture* fixtureB) :
    Contact(fixtureA, fixtureB)
{
}

Contact* BoxToCylinderContact::Create(Fixture* fixtureA, Fixture* fixtureB)
{
    return new BoxToCylinderContact(fixtureA, fixtureB);
}

void spe::BoxToCylinderContact::FindCollisionPoints(const ConvexInfo& boxA, const ConvexInfo& cylinderB, 
    CollisionInfo& collisionInfo, ResultEPA& resultEPA, Polytope* simplexArray)
{
    Face refFace, incFace;
    SetBoxFace(refFace, boxA, resultEPA.normal);
    SetCylinderFace(incFace, cylinderB, -resultEPA.normal);

    cout << cylinderB.axes[0].x << ", " << cylinderB.axes[0].y << ", " << cylinderB.axes[0].z << '\n';
    
    ContactFace contactFace;
    ComputeContactPolygon(contactFace, refFace, incFace);

    BuildManifoldFromPolygon(collisionInfo, refFace, incFace, contactFace, resultEPA);
}

}
