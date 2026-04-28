#pragma once
#include "GameObject.h"

class Character : public GameObject
{
    public:
    Character(const XMFLOAT3& position);
    virtual ~Character() {}

    virtual void Update(float dt) override;

    void MoveTargetPosXZ(float dt);

    void SetTargetPos(const XMFLOAT3& targetPos);
private:
    XMFLOAT3 targetPos_ = { 0.0f, 0.0f, 0.0f };
    float moveSpeed_ = 2.0f;
};

