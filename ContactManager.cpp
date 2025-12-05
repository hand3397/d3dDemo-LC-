#include "ContactManager.h"

namespace spe {;

ContactManager::ContactManager() :
    contacts(nullptr)
{

}

ContactManager::~ContactManager()
{

}

void ContactManager::AddPair(void* proxyUserDataA, void* proxyUserDataB)
{
    FixtureProxy* proxyA = static_cast<FixtureProxy*>(proxyUserDataA);
    FixtureProxy* proxyB = static_cast<FixtureProxy*>(proxyUserDataB);

    Fixture* fixtureA = proxyA->fixture;
    Fixture* fixtureB = proxyB->fixture;

    // contact의 유효성 판단
    if (fixtureA->GetRigidbody() == fixtureA->GetRigidbody())
        return;

    // contact 생성
    Contact* contact = Contact::Create(fixtureA, fixtureB);


}

void ContactManager::FindNewContacts()
{
    broadPhase.UpdatePairs(this);
}

bool ContactManager::IsSameContact(ContactLink* link, Fixture* fixtureA, Fixture* fixtureB, int32_t indexA, int32_t indexB)
{
    return false;
}

void ContactManager::Collide()
{

}

}


