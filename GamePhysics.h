#pragma once
#include "d3dUtil.h"
#include "Rigidbody.h"

class GamePhysics
{
public:
    GamePhysics() {}
    ~GamePhysics() {}
    
    void Update(float dt);

private:
    
    vector<Rigidbody*> rigidbodies_;
};

