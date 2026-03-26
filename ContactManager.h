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
    void RemoveContact(Contact* target);
    void ClearContact();
    void FindNewContacts();
    bool IsSameContact(ContactLink* link, Fixture* fixtureA, Fixture* fixtureB);
    void Collide();

    Contact* GetContacts();
    uint32_t numContacts()const;

    BroadPhase* GetBroadPhase();

private:
    BroadPhase broadPhase_;

    Contact* contacts_;
    uint32_t numContacts_;
};

}



