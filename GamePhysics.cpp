#include "GamePhysics.h"

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
            Manifold manifold = contact->GetManifold();
            XMVECTOR pos = XMLoadFloat3(&contact->GetFixtureA()->GetRigidbody()->GetPosition());
            pos += (-XMLoadFloat3(&manifold.points[0].normal) * (manifold.points[0].seperation + 0.001f));
            XMFLOAT3 pos3f;
            XMStoreFloat3(&pos3f, pos);
            contact->GetFixtureA()->GetRigidbody()->SetPosition(pos3f);
            XMFLOAT3 vel = contact->GetFixtureA()->GetRigidbody()->GetLinearVelocity();
            if (vel.y < 0.0f)
                vel.y = -vel.y;
            XMStoreFloat3(&vel, XMLoadFloat3(&vel) * 0.8f);
            contact->GetFixtureA()->GetRigidbody()->SetLinearVelocity(vel);
            contact->GetFixtureA()->GetRigidbody()->CalculateMatrix();
            contact->GetFixtureA()->GetRigidbody()->GetGameObject()->Update(dt);
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
        if (!go->GetRigidbody().isGrounded_)
            go->GetRigidbody().AddForce(XMFLOAT3(0.f, gravity_ * 5.f, 0.f));
    for (auto& go : kinematicGameObjects)
        if (!go->GetRigidbody().isGrounded_)
            go->GetRigidbody().AddForce(XMFLOAT3(0.f, gravity_ * 5.f, 0.f));
}

}


