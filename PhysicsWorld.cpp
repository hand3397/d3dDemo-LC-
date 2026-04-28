#include "PhysicsWorld.h"
#include "Island.h"
#include "Scene.h"

namespace spe {;

PhysicsWorld::PhysicsWorld(Scene* scene) :
    scene_(scene), rigidbodies_(nullptr), numRigidbodies_(0)
{
	broadPhase_ = contactManager_.GetBroadPhase();
}

PhysicsWorld::~PhysicsWorld()
{
    numRigidbodies_ = 0;
    rigidbodies_ = nullptr;
}

void PhysicsWorld::Update(float dt)
{
	OnGravity(dt);

    Rigidbody* body = rigidbodies_;
    while (body != nullptr) {
        if (body->HasFlag(RigidbodyFlag::AWAKE) && body->GetType() != RigidbodyType::STATIC) {
            // 이동 계산
            body->Integrate(dt);
            // body값을 fixture에 동기화
            body->SynchronizeFixture(broadPhase_);
        }
        body = body->GetNext();
    }

    // update Possible Contact Pairs - BroadPhase
    contactManager_.FindNewContacts();

    // Process Contacts - NarrowPhase
    contactManager_.Collide();

    // Solve contact - velocity & position
    Solve(dt);

    body = rigidbodies_;
	
    while (body != nullptr) {
        // Synchronize GameObejct
		body->CalculateMatrix();
        body->GetGameObject()->Update(dt);
        body = body->GetNext();
    }
}

void PhysicsWorld::Solve(float dt)
{
	Island island(numRigidbodies_, contactManager_.numContacts());

	// 모든 body들의 플래그에 islandFlag 제거
	for (Rigidbody* body = rigidbodies_; body != nullptr; body = body->GetNext())
	{
		body->ClearFlag(RigidbodyFlag::ISLAND);
	}

	// 모든 contact들의 플래그에 islandFlag 제거
	for (Contact* contact = contactManager_.GetContacts(); contact != nullptr; contact = contact->GetNext()) 
	{
		contact->ClearFlag(ContactFlag::ISLAND);
	}

	// Body를 순회하며 island를 생성후 solve 처리
	Rigidbody** stack = new Rigidbody*[numRigidbodies_];
	int32_t stackPtr = 0;

	// rigidbodies_를 순회하며 아직 island에 포함되지 않는 targetBody를 찾는다.
	// 해당 targetBody를 찾을 경우 targetBody의 contactLink를 불러와 충돌한 상대otherbody를 island에 추가하고
	// 이후 다른 연결된 충돌을 찾는다.
	// 충돌에 연결된 다른 body가 있다면 전부 island에 넣고 충돌을 해결한다. 
	for (Rigidbody* body = rigidbodies_; body != nullptr; body = body->GetNext()) 
	{
		// 이미 island에 포함된 경우 continue
		if (body->HasFlag(RigidbodyFlag::ISLAND)) {
			continue;
		}

		// staticBody인 경우 continue
		if (body->GetType() == RigidbodyType::STATIC) {
			continue;
		}

		// island clear를 통해 새로운 island 생성
		island.Clear();
		stack[stackPtr++] = body;	// push body
		body->SetFlag(RigidbodyFlag::ISLAND); // body island 처리

		// DFS로 island 생성
		while (stackPtr > 0) {
			// 스택 가장 마지막에 있는 body island에 추가
			Rigidbody* targetBody = stack[--stackPtr];	// pop body
			island.Add(targetBody);

			// body가 staticBody면 뒤에 과정 pass
			if (targetBody->GetType() == RigidbodyType::STATIC) {
				continue;
			}

			// body contactList의 contact들을 island에 추가
			for (ContactLink* link = targetBody->GetContactLink(); link != nullptr; link = link->next) {
				Contact* contact = link->contact;

				// 이미 island에 포함된 경우 continue
				if (contact->HasFlag(ContactFlag::ISLAND)) {
					continue;
				}

				// contact가 touching 상태가 아니면 continue
				if (contact->HasFlag(ContactFlag::TOUCHING) == false) {
					continue;
				}

				// 위 조건을 다 충족하는 경우 island에 추가 후 island 플래그 on
				island.Add(contact);
				contact->SetFlag(ContactFlag::ISLAND);

				Rigidbody* other = link->other;

				// 충돌 상대 body가 이미 island에 속한 상태면 continue
				if (other->HasFlag(RigidbodyFlag::ISLAND)) {
					continue;
				}

				// 충돌 상대 body가 island에 속한게 아니었으면 stack에 추가 후 island 플래그 on
				stack[stackPtr++] = other; // push body
				other->SetFlag(RigidbodyFlag::ISLAND);
			}
		}

		// 생성한 island 충돌 처리
		island.Solve(dt);

		// island의 staticBody들의 island 플래그 off
		// static물체가 다른 충돌에 다시 사용될 수 있도록한다.
		uint32_t numBodies = island.numBodies();
		Rigidbody** islandBodies = island.GetBodies();
		for (uint32_t i = 0; i < numBodies; ++i) {

			if (islandBodies[i]->GetType() == RigidbodyType::STATIC) {
				islandBodies[i]->ClearFlag(RigidbodyFlag::ISLAND);
			}
		}
	}

	delete[] stack;
	island.Destroy();
}

void PhysicsWorld::OnGravity(float dt)
{
	Rigidbody* body = rigidbodies_;
	float gravityVel = gravity_ * dt;
	while (body != nullptr) {
		if (body->HasFlag(RigidbodyFlag::AWAKE) && body->GetType() == RigidbodyType::DYNAMIC) {
			body->AddLinearVelocity(XMFLOAT3(0.0f, gravityVel, 0.0f));
		}

		body = body->GetNext();
	}
}

void PhysicsWorld::InitSceneObjects()
{
    Clear();
    if (scene_ == nullptr)
        return;

    const vector<GameObject*>& GameObjects = scene_->GetAllGameObjects();
    for (auto& go : GameObjects) 
    {
        if (go == nullptr)
            continue;

        AddRigidbody(go->GetRigidbody());
    }

	for (int i = 0; i < scene_->NumCharacters(); ++i) {
		AddRigidbody(scene_->GetCharacters(i)->GetRigidbody());
	}

}

void PhysicsWorld::AddRigidbody(Rigidbody* rigidbody)
{
    rigidbody->SetPrev(nullptr);
    rigidbody->SetNext(rigidbodies_);
    if (rigidbodies_ != nullptr) {
        rigidbodies_->SetPrev(rigidbody);
    }
    rigidbodies_ = rigidbody;
    
	rigidbody->GetFixture()->CreateProxy(broadPhase_);

    ++numRigidbodies_;
}

void PhysicsWorld::RemoveRigidbody(Rigidbody* rigidbody)
{
	if (rigidbody == nullptr)
		return;

	// rigidbodies_에서 제거
	Rigidbody* prev = rigidbody->GetPrev();
	Rigidbody* next = rigidbody->GetNext();

	if (rigidbody == rigidbodies_) {
		rigidbodies_ = next;
	}

	if (prev != nullptr) prev->SetNext(next);
	if (next != nullptr) next->SetPrev(prev);

	rigidbody->SetPrev(nullptr);
	rigidbody->SetNext(nullptr);

	// Proxy 제거 (BroadPhase에서 proxySet_ 정리)
	Fixture* fixture = rigidbody->GetFixture();
	if (fixture != nullptr) {
		fixture->DestroyProxy(broadPhase_);
	}

    // 해당 Rigidbody와 연결된 모든 Rigidbody를 깨운다.
    vector<Rigidbody*> stack;
	unordered_set<Rigidbody*> visited;
	stack.push_back(rigidbody);
    visited.insert(rigidbody);

	while (!stack.empty()) {
		Rigidbody* current = stack.back();
		stack.pop_back();
		
		current->SetAwake(true);
		ContactLink* contactLink = current->GetContactLink();
		while (contactLink != nullptr) {
			if (contactLink->other != nullptr && visited.find(contactLink->other) == visited.end()) {
				stack.push_back(contactLink->other);
                visited.insert(contactLink->other);
			}
		}
	}

	// 해당 Rigidbody의 모든 Contact 정리
	ContactLink* contactLink = rigidbody->GetContactLink();
	while (contactLink != nullptr) {
		ContactLink* next = contactLink->next;
		contactManager_.RemoveContact(contactLink->contact);
		contactLink = next;
	}

	--numRigidbodies_;
}

void PhysicsWorld::Clear()
{
	contactManager_.ClearContact();
    numRigidbodies_ = 0;
    rigidbodies_ = nullptr;
}

Rigidbody* PhysicsWorld::GetRigidbodies() const
{
	return rigidbodies_;
}

uint32_t PhysicsWorld::GetNumRigidbodies() const
{
	return numRigidbodies_;
}

Rigidbody* PhysicsWorld::RayCast(const Ray& ray)
{
	Rigidbody* closestBody = nullptr;
	float minT = FLT_MAX;

	// 1. DynamicTree를 통해 Ray와 AABB가 겹치는 후보들을 찾음
	FixtureProxy* proxyData = static_cast<FixtureProxy*>(broadPhase_->RayCast(ray));
	if (proxyData == nullptr) // ray와 겹치는 물체가 없음
		return nullptr;

	Rigidbody* rigidBody = proxyData->fixture->GetRigidbody();

	// 2. 각 후보 body에 대해 세부 충돌 판정 (Narrow-phase)


	// Shape의 유형에 따라 Ray-Shape 충돌 계산 (Collision.h 등에 정의 필요)
	return rigidBody;
}

}


