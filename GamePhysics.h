#pragma once
#include "d3dUtil.h"
#include "GameObject.h"
#include "Player.h"
#include "Contact.h"

namespace spe {;

class GamePhysics
{
public:
    GamePhysics();
    ~GamePhysics();

    void Update(float dt,
        vector<GameObject*>& allGameObjects,
        vector<GameObject*>* gameObjectLayers,
        Player* player);

private:
    void OnGravity(vector<GameObject*>& dynamicGameObjects,
        vector<GameObject*>& kinematicGameObjects, Player* player);

    Contact* contact = nullptr;
    uint32_t numContacts;

    float gravity_ = -9.8f;
    float minFloorY_ = 0.0f;
};

}


