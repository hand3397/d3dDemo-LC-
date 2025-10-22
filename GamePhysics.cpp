#include "GamePhysics.h"

void GamePhysics::Update(float dt, vector<unique_ptr<GameObject>>& allGameObjects, vector<GameObject*> gameObjectLayer[], Player* player)
{
    auto& dynamicGameObjects = gameObjectLayer[(uint8_t)RigidbodyType::Dynamic];
    for (auto& go : dynamicGameObjects) {
        go->GetRigidbody().Integrate(dt);
        go->Update(dt);
    }

    auto& kinematicGameObjects = gameObjectLayer[(uint8_t)RigidbodyType::Kinematic];
    for (auto& go : kinematicGameObjects) {
        go->GetRigidbody().Integrate(dt);
        go->Update(dt);
    }

    auto& playerRigidbody = player->GetRigidbody();

    if (IsBelowWorldBounds(playerRigidbody)) {
        float dPosY = minFloorY_ - (playerRigidbody.boundingBox_.Center.y - playerRigidbody.boundingBox_.Extents.y);
        XMFLOAT3 pos = player->GetPosition();
        pos.y += dPosY;
        player->SetPosition(pos);

        player->Update(dt);
    }
}

bool GamePhysics::IsBelowWorldBounds(const Rigidbody& rigidbody)
{
    float minY = rigidbody.boundingBox_.Center.y - rigidbody.boundingBox_.Extents.y;
    return (minY < minFloorY_);
}

