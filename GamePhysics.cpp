#include "GamePhysics.h"

GamePhysics::GamePhysics()
{
}

GamePhysics::~GamePhysics()
{
}

void GamePhysics::Update(float dt, vector<unique_ptr<GameObject>>& allGameObjects, vector<GameObject*> gameObjectLayer[], Player* player)
{
    auto& dynamicGameObjects = gameObjectLayer[(uint8_t)RigidbodyType::Dynamic];
    auto& kinematicGameObjects = gameObjectLayer[(uint8_t)RigidbodyType::Kinematic];
    
    // gravity falling
    /*
    for (auto& go : dynamicGameObjects)
        if (!go->GetRigidbody().isGrounded_)
            go->GetRigidbody().AddForce(XMFLOAT3(0.f, gravity_, 0.f));
    for (auto& go : kinematicGameObjects) 
        if (!go->GetRigidbody().isGrounded_)
            go->GetRigidbody().AddForce(XMFLOAT3(0.f, gravity_, 0.f));
    */

    // Intergraer rigidbody
    for (auto& go : dynamicGameObjects) {
        go->GetRigidbody().Integrate(dt);
        go->Update(dt);
    }
    for (auto& go : kinematicGameObjects) {
        go->GetRigidbody().Integrate(dt);
        go->Update(dt);
    }

    /*
    auto& playerRigidbody = player->GetRigidbody();

    if (IsBelowWorldBounds(playerRigidbody)) {
        playerRigidbody.position_.y = minFloorY_;
        playerRigidbody.isGrounded_ = true;
        player->Update(dt);
    }
    */
}

