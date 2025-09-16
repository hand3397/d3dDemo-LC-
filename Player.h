#pragma once
#include <GameObject.h>
#include <KeyInputManager.h> 
#include <Camera.h>

class Player : public MovingObject 
{
public:
    Player(const string& name);

    void KeyInput(const KeyInputManager& keyInput, float dt);
    virtual void Update(float dt) override;

    Camera* GetCamera();
    void SetCameraOffset(const XMFLOAT3& offset);

private:
    Camera camera_;
    XMFLOAT4X4 cameraOffset_ = MathHelper::Identity4x4();
};

