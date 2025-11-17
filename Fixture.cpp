#include "Fixture.h"

namespace spe {;

Fixture::Fixture(Shape* shape)
{
    shape_ = shape;
    shapeType_ = shape->GetType();
}

Shape* Fixture::GetShape()
{
    return shape_;
}

ShapeType Fixture::GetShapeType()const
{
    return shapeType_;
}

float Fixture::GetFriction()
{
    return friction_;
}

float Fixture::GetRestitution()
{
    return restitution_;
}

}