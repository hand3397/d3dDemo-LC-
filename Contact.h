#pragma once
#include "Rigidbody.h"
#include "Fixture.h"


//  두 물체의 충돌을 처리함.


class Contact
{
public:
    Contact(Fixture* A, Fixture* B);

private:

    Fixture* A = nullptr;
    Fixture* B = nullptr;
};

