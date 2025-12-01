#include "Fixture.h"
#include "Rigidbody.h"

namespace spe {;

Fixture::Fixture(Shape* shape)
{
    shape_ = shape;
    shapeType_ = shape->GetType();
}

Fixture::~Fixture()
{
    if (shape_ != nullptr) {
        delete shape_;
        shape_ = nullptr;
    }
}

Rigidbody* Fixture::GetRigidbody() const
{
    return rigidbody_;
}

Shape* Fixture::GetShape()
{
    return shape_;
}

ShapeType Fixture::GetShapeType()const
{
    return shapeType_;
}

float Fixture::GetFriction()const
{
    return friction_;
}

float Fixture::GetRestitution()const
{
    return restitution_;
}

}