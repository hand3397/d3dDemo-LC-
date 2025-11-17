#pragma once
#include "d3dUtil.h"
#include "GameObject.h"
#include "Player.h"


class GamePhysics
{
public:
    GamePhysics();
    ~GamePhysics();
    
    void Update(float dt,
        vector<GameObject*>& allGameObjects,
        vector<GameObject*>& dynamicGameObjects,
        vector<GameObject*>& kinematicGameObjects,
        Player* player);

private:
    void OnGravity(vector<GameObject*>& dynamicGameObjects,
        vector<GameObject*>& kinematicGameObjects, Player* player);

private:

    float gravity_ = -9.8f;
    float minFloorY_ = 0.0f;
};

