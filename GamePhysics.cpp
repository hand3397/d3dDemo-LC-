#include "GamePhysics.h"

GamePhysics::GamePhysics()
{
}

GamePhysics::~GamePhysics()
{
}

void GamePhysics::Update(float dt, 
    vector<GameObject*>& allGameObjects, 
    vector<GameObject*>& dynamicGameObjects, 
    vector<GameObject*>& kinematicGameObjects,
    Player* player)
{
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

    
    auto& playerRigidbody = player->GetRigidbody();
    playerRigidbody.Integrate(dt);
}

void GamePhysics::OnGravity(vector<GameObject*>& dynamicGameObjects,
    vector<GameObject*>& kinematicGameObjects, Player* player)
{
    for (auto& go : dynamicGameObjects)
        if (!go->GetRigidbody().isGrounded_)
            go->GetRigidbody().AddForce(XMFLOAT3(0.f, gravity_ * 10.f, 0.f));
    for (auto& go : kinematicGameObjects)
        if (!go->GetRigidbody().isGrounded_)
            go->GetRigidbody().AddForce(XMFLOAT3(0.f, gravity_ * 10.f, 0.f));
}

