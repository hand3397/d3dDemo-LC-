#pragma once
#include "GameObject.h"
#include "Player.h"

class Scene
{
public:
    void InitScene();
    void AddObject();
    void Update(float dt);

private:
    vector<unique_ptr<GameObject>> objects_;
    unique_ptr<Player> player_;
};

