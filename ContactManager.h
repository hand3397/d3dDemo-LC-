#pragma once
#include "Contact.h"
#include "BroadPhase.h"
#include "Rigidbody.h"

namespace spe {;

// broadPhase에서 넘어온 contact의 휴효성을 판단한다.
// 이후 contact를 업데이트 하고 충돌을 해결함.
class ContactManager
{
public:
    ContactManager();
    ~ContactManager();

    void AddPair(void* proxyUserDataA, void* proxyUserDataB);
    void FindNewContacts();
    bool IsSameContact(ContactLink* link, Fixture* fixtureA, Fixture* fixtureB, int32_t indexA, int32_t indexB);
    void Collide();

private:
    BroadPhase broadPhase;

    Contact* contacts;
    uint32_t numContacts;
};

}



