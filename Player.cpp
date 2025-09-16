#include "Player.h"

Player::Player(const string& name) : MovingObject(name)
{
    speed_ = 10.0f;
}

void Player::KeyInput(const KeyInputManager& keyInput, float dt)
{
    //mouse input
    if (keyInput.IsMouseDown(MouseButton::LMB)) {
        int dx, dy;
        keyInput.GetMouseDelta(dx, dy);

        camera_.Pitch(static_cast<float>(dy) * 0.01f);
        camera_.RotateY(static_cast<float>(dx) * 0.01f);
    }

    //keyboard input
    XMVECTOR force = XMVectorZero();
    XMVECTOR lookDir = camera_.GetLook();
    XMVECTOR rightDir = camera_.GetRight();
    if (keyInput.IsKeyDown('W')) {
        force += lookDir * speed_ * dt;
    }
    if (keyInput.IsKeyDown('A')) {
        force += -rightDir * speed_ * dt;
    }
    if (keyInput.IsKeyDown('S')) {
        force += -lookDir * speed_ * dt;
    }
    if (keyInput.IsKeyDown('D')) {
        force += rightDir * speed_ * dt;
    }

    XMFLOAT3 force3f;
    XMStoreFloat3(&force3f, force);
    ApplyForce(force3f);
}

void Player::Update(float dt)
{
    //pos Update
    MovingObject::Update(dt);

    camera_.UpdateViewMatrix();
}

void Player::SetCameraOffset(const XMFLOAT4X4& cameraOffset)
{
    cameraOffset_ = cameraOffset;
}
