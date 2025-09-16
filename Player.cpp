#include "Player.h"

Player::Player(const string& name) : MovingObject(name)
{
    speed_ = 500.0f;
}

void Player::KeyInput(const KeyInputManager& keyInput, float dt)
{
    //mouse input
    if (keyInput.IsMouseDown(MouseButton::LMB)) {
        int dx, dy;
        keyInput.GetMouseDelta(dx, dy);

        camera_.RotatePitch(static_cast<float>(dy) * 0.01f);
        camera_.RotateYaw(static_cast<float>(dx) * 0.01f);
        camera_.UpdateViewMatrix();
    }

    //keyboard input
    XMVECTOR force = XMVectorZero();
    XMVECTOR lookXZDir = XMVector3NormalizeEst(XMVectorSet(camera_.GetLook3f().x, 0.0f, camera_.GetLook3f().z, 0.0f));
    XMVECTOR rightXZDir = XMVector3NormalizeEst(XMVectorSet(camera_.GetRight3f().x, 0.0f, camera_.GetRight3f().z, 0.0f));
    if (keyInput.IsKeyDown('W')) {
        force += lookXZDir * speed_ * dt;
    }
    if (keyInput.IsKeyDown('A')) {
        force += -rightXZDir * speed_ * dt;
    }
    if (keyInput.IsKeyDown('S')) {
        force += -lookXZDir * speed_ * dt;
    }
    if (keyInput.IsKeyDown('D')) {
        force += rightXZDir * speed_ * dt;
    }

    XMFLOAT3 force3f;
    XMStoreFloat3(&force3f, force);
    ApplyForce(force3f);
}

void Player::Update(float dt)
{
    //pos Update
    MovingObject::Update(dt);

    camera_.SetTarget(position_);
    camera_.UpdateViewMatrix();
}

Camera* Player::GetCamera()
{
    return &camera_ ;
}

void Player::SetCameraOffset(const XMFLOAT3& offset)
{
    camera_.SetOffset(offset);
}
