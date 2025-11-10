#pragma once
#include "Shape.h"

class Fixture
{
public:
    Fixture(Shape* shape);

    ShapeType GetShapeType()const;
    float GetFriction();
    float GetRestitution();

protected:
    ShapeType shapeType_;
    Shape* shape_;

    //float density_ = 1.0f;
    float friction_ = 0.f;
    float restitution_ = 0.f;
};

