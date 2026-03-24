#pragma once
#include "d3dUtil.h"
#include "Player.h"
#include "ContactManager.h"
#include "Island.h"
#include "ContactSolver.h"

class Scene;

namespace spe {;

class PhysicsWorld
{
public:
    PhysicsWorld(Scene* scene);
    ~PhysicsWorld();

    void Update(float dt);
    void Solve(float dt);
    void OnGravity(float dt);

    // set Rigidbodies
    void InitSceneObjects();
    void AddRigidbody(Rigidbody* rigidbody);
    void DeleteRigidbody(Rigidbody* rigidbody);
    void Clear();

    Ray pickingRay;
private:
    Scene* scene_;

    ContactManager contactManager_;
    BroadPhase* broadPhase_;

	Rigidbody *rigidbodies_;
	int32_t numRigidbodies_;
    
    float gravity_ = -9.8f;
    float minFloorY_ = 0.0f;
};

}


