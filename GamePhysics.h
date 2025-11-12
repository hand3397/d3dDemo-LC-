#pragma once
#include "d3dUtil.h"
#include "GameObject.h"
#include "Player.h"


class GamePhysics
{
public:
    GamePhysics();
    ~GamePhysics();
    
    void Update(float dt, vector<unique_ptr<GameObject>>& allGameObjects, vector<GameObject*> gameObjectLayer[], Player* player);

private:
    float gravity_ = -9.8f;
    float minFloorY_ = 0.0f;
};

