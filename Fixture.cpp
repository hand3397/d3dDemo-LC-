#include "Fixture.h"

Fixture::Fixture(Shape* shape)
{
    shape_ = shape;
    shapeType_ = shape->GetType();
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