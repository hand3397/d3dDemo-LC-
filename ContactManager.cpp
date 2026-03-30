#include "ContactManager.h"

namespace spe {;

ContactManager::ContactManager() :
    contacts_(nullptr), numContacts_(0)
{
}

ContactManager::~ContactManager()
{
    ClearContact();
}

void ContactManager::AddPair(void* proxyUserDataA, void* proxyUserDataB)
{
    FixtureProxy* proxyA = static_cast<FixtureProxy*>(proxyUserDataA);
    FixtureProxy* proxyB = static_cast<FixtureProxy*>(proxyUserDataB);

    Fixture* fixtureA = proxyA->fixture;
    Fixture* fixtureB = proxyB->fixture;

    if (fixtureA == nullptr || fixtureB == nullptr)
        return;

    Rigidbody* bodyA = fixtureA->GetRigidbody();
    Rigidbody* bodyB = fixtureB->GetRigidbody();

    // contact의 유효성 판단
    // 같은 rigidbody인가?
    if (bodyA == bodyB) {
        return;
    }
        
    // 같은 충돌이 이미 있는가?
    ContactLink* link = bodyA->GetContactLink();
    while (link) {
        if (link->other == bodyB && IsSameContact(link, fixtureA, fixtureB)) {
            return;
        }
        link = link->next;
    }

    // contact 생성
    Contact* contact = Contact::Create(fixtureA, fixtureB);

    assert(contact != nullptr);

    // contact 생성시 fixture의 순서가 바뀔 수 있음
    fixtureA = contact->GetFixtureA();
    fixtureB = contact->GetFixtureB();
    bodyA = fixtureA->GetRigidbody();
    bodyB = fixtureB->GetRigidbody();

    // contact를 contacts_의 앞에 넣기
    contact->SetNext(contacts_);
    if (contacts_ != nullptr) {
        contacts_->SetPrev(contact);
    }
    contacts_ = contact;
    ++numContacts_;

    // bodyA의 contactLinks에 새로운 Link 추가
    ContactLink* linkA = contact->GetContactLinkA();
    ContactLink* bodyLinkA = bodyA->GetContactLink();
    linkA->next = bodyLinkA;
    if (bodyLinkA != nullptr) {
        bodyLinkA->prev = linkA;
    }
    bodyA->SetContactLink(linkA);

    // bodyB의 contactLinks에 새로운 Link 추가
    ContactLink* linkB = contact->GetContactLinkB();
    ContactLink* bodyLinkB = bodyB->GetContactLink();
    linkB->next = bodyLinkB;
    if (bodyLinkB != nullptr) {
        bodyLinkB->prev = linkB;
    }
    bodyB->SetContactLink(linkB);
}

void ContactManager::RemoveContact(Contact* target)
{
    if (target == nullptr)
        return;

    numContacts_--;

    Contact* contact = contacts_;
    while (contact != nullptr) {
        if (contact == target) {
            Contact* prev = contact->GetPrev();
            Contact* next = contact->GetNext();

            // prev 노드의 next 포인터 갱신
            if (prev != nullptr)
                prev->SetNext(next);
            // next 노드의 prev 포인터 갱신
            if (next != nullptr)
                next->SetPrev(prev);
            // head가 삭제되는 경우 처리
            if (contacts_ == contact)
                contacts_ = next;

            delete contact;
            break;
        }
        contact = contact->GetNext();
    }
}

void ContactManager::ClearContact()
{
    numContacts_ = 0;

    Contact* contact = contacts_;
    while (contact) {
        Contact* next = contact->GetNext();
        delete contact;
        contact = next;
    }
    contacts_ = nullptr;
}

void ContactManager::FindNewContacts()
{
    broadPhase_.UpdatePairs(this);
}

bool ContactManager::IsSameContact(ContactLink* link, Fixture* fixtureA, Fixture* fixtureB)
{
    Contact* contact = link->contact;
    Fixture* fixtureX = contact->GetFixtureA();
    Fixture* fixtureY = contact->GetFixtureB();

    if (fixtureX == fixtureA && fixtureY == fixtureB) {
        // 이미 해당 충돌이 contacts_에 있기 떄문에 flag만 켜준다.
        contact->SetFlag(ContactFlag::TOUCHING);
        return true;
    }
    if (fixtureX == fixtureB && fixtureY == fixtureA) {
        contact->SetFlag(ContactFlag::TOUCHING);
        return true;
    }

    return false;
}

void ContactManager::Collide()
{
    Contact* contact = contacts_;

    // contactList 순회
    while (contact) {
        // 활성화된 충돌만 처리함.
        if (contact->HasFlag(ContactFlag::TOUCHING)) {
            contact->Update();
        }
        contact = contact->GetNext();
    }
}

Contact* ContactManager::GetContacts()
{
    return contacts_;
}

uint32_t ContactManager::numContacts() const
{
    return numContacts_;
}

BroadPhase* ContactManager::GetBroadPhase()
{
    return &broadPhase_;
}

} // namespace spe;


