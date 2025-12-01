#pragma once
#include "Shape.h"

namespace spe {; 

class Rigidbody;

class Fixture
{
public:
    Fixture(Shape* shape);
    ~Fixture();

    Rigidbody* GetRigidbody()const;
    Shape* GetShape();
    ShapeType GetShapeType()const;
    float GetFriction()const;
    float GetRestitution()const;

    void SetRigidbody(Rigidbody* rigidbody);
    void SetFriction(float friction);
    void SetRestitution(float restitution);

protected:
    Rigidbody* rigidbody_ = nullptr;

    ShapeType shapeType_;
    Shape* shape_ = nullptr;

    //float density_ = 1.0f;
    float friction_ = 0.f;
    float restitution_ = 0.f;
};

}


