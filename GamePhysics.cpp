#include "GamePhysics.h"
#include "Island.h"

namespace spe {;

GamePhysics::GamePhysics()
{
}

GamePhysics::~GamePhysics()
{
}

void GamePhysics::Update(float dt,
    vector<GameObject*>& allGameObjects,
    vector<GameObject*>* allLayeredGameObjects,
    Player* player)
{
    auto& dynamicGameObjects = allLayeredGameObjects[(uint32_t)RigidbodyType::DYNAMIC];
    auto& kinematicGameObjects = allLayeredGameObjects[(uint32_t)RigidbodyType::KINEMATIC];
    auto& staticGameObjects = allLayeredGameObjects[(uint32_t)RigidbodyType::STATIC];

    // gravity falling
    OnGravity(dynamicGameObjects, kinematicGameObjects, player);

    // Intergraer rigidbody
    for (auto& go : dynamicGameObjects) {
        go->GetRigidbody().Integrate(dt);
        go->Update(dt);
    }
    for (auto& go : kinematicGameObjects) {
        go->GetRigidbody().Integrate(dt);
        go->Update(dt);
    }

    // Contact DynamicToStatic
    for (auto& dynamicGo : dynamicGameObjects) {
        auto& dynamicRigidbody = dynamicGo->GetRigidbody();
        XMMATRIX xf1 = dynamicRigidbody.GetTransformMatrix();
        AABB aabb1 = dynamicRigidbody.GetFixture()->GetShape()->GetAABB(xf1);
        
        for (auto& staticGo : staticGameObjects) {
            auto& staticRigidbody = staticGo->GetRigidbody();
            XMMATRIX xf2 = staticRigidbody.GetTransformMatrix();
            AABB aabb2 = staticRigidbody.GetFixture()->GetShape()->GetAABB(xf2);
            if (aabb1.Intersects(aabb2)) {
                contact = Contact::Create(dynamicRigidbody.GetFixture(), staticRigidbody.GetFixture());
            }
        }
    }

    if (contact != nullptr) {
        contact->Update();
        if (contact->IsTouching()) {
            Island island(2, 1);
            Rigidbody* bodyA = contact->GetFixtureA()->GetRigidbody();
            Rigidbody* bodyB = contact->GetFixtureB()->GetRigidbody();
            island.Add(contact);
            island.Add(bodyA);
            island.Add(bodyB);
            island.Solve(dt);
            island.Clear();

            bodyA->GetGameObject()->Update(dt);
            bodyB->GetGameObject()->Update(dt);
        }
    }

    if (contact != nullptr) {
        delete contact;
        contact = nullptr;
    }

    auto& playerRigidbody = player->GetRigidbody();
    playerRigidbody.Integrate(dt);
}

void GamePhysics::OnGravity(vector<GameObject*>& dynamicGameObjects,
    vector<GameObject*>& kinematicGameObjects, Player* player)
{
    for (auto& go : dynamicGameObjects)
        if (!go->GetRigidbody().isAwake())
            go->GetRigidbody().AddLinearAcc(XMFLOAT3(0.f, gravity_, 0.f));
    for (auto& go : kinematicGameObjects)
        if (!go->GetRigidbody().isAwake())
            go->GetRigidbody().AddLinearAcc(XMFLOAT3(0.f, gravity_, 0.f));
}

}


