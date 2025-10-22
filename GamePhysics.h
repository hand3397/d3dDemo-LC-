#pragma once
#include "d3dUtil.h"
#include "GameObject.h"
#include "Player.h"

class GamePhysics
{
public:
    GamePhysics() {}
    ~GamePhysics() {}
    
    void Update(float dt, vector<unique_ptr<GameObject>>& allGameObjects, vector<GameObject*> gameObjectLayer[], Player* player);

private:
    bool IsBelowWorldBounds(const Rigidbody& rigidbody);

    const float minFloorY_ = 0.0f;
};

