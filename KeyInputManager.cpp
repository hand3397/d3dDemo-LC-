#include "KeyInputManager.h"

KeyInputManager::KeyInputManager()
    : mouseX_(0), mouseY_(0), prevMouseX_(0), prevMouseY_(0)
{
    prevKeyState_.fill(false);
    currKeyState_.fill(false);
    keyPressed_.fill(false);
    keyReleased_.fill(false);

    prevMouseState_.fill(false);
    currMouseState_.fill(false);
    mousePressed_.fill(false);
    mouseReleased_.fill(false);
}

void KeyInputManager::OnKeyDown(WPARAM wparam)
{
    if (wparam < NUM_KEYS)
        currKeyState_[wparam] = true;
}

void KeyInputManager::OnKeyUp(WPARAM wparam)
{
    if (wparam < NUM_KEYS)
        currKeyState_[wparam] = false;
}

void KeyInputManager::OnMouseDown(MouseButton button)
{
    currMouseState_[button] = true;
}

void KeyInputManager::OnMouseUp(MouseButton button)
{
    currMouseState_[button] = false;
}

void KeyInputManager::SetMousePos(int x, int y)
{
    mouseX_ = x;
    mouseY_ = y;
}

void KeyInputManager::Update()
{
    // 키보드 상태
    for (int i = 0; i < NUM_KEYS; i++) {
        keyPressed_[i] = currKeyState_[i] && !prevKeyState_[i];
        keyReleased_[i] = !currKeyState_[i] && prevKeyState_[i];
    }
    prevKeyState_ = currKeyState_;

    // 마우스 상태
    for (int i = 0; i < NUM_MOUSE_BUTTONS; i++) {
        mousePressed_[i] = currMouseState_[i] && !prevMouseState_[i];
        mouseReleased_[i] = !currMouseState_[i] && prevMouseState_[i];
    }
    prevMouseState_ = currMouseState_;

    prevMouseX_ = mouseX_;
    prevMouseY_ = mouseY_;
}

// -------------------- 상태 쿼리 --------------------
bool KeyInputManager::IsKeyDown(UINT key) const
{
    return key < NUM_KEYS ? currKeyState_[key] : false;
}

bool KeyInputManager::WasKeyPressed(UINT key) const
{
    return key < NUM_KEYS ? keyPressed_[key] : false;
}

bool KeyInputManager::WasKeyReleased(UINT key) const
{
    return key < NUM_KEYS ? keyReleased_[key] : false;
}

bool KeyInputManager::IsMouseDown(UINT button) const
{
    return button < NUM_MOUSE_BUTTONS ? currMouseState_[button] : false;
}

bool KeyInputManager::WasMousePressed(UINT button) const
{
    return button < NUM_MOUSE_BUTTONS ? mousePressed_[button] : false;
}

bool KeyInputManager::WasMouseReleased(UINT button) const
{
    return button < NUM_MOUSE_BUTTONS ? mouseReleased_[button] : false;
}

void KeyInputManager::GetMousePos(int& x, int& y) const
{
    x = mouseX_;
    y = mouseY_;
}

void KeyInputManager::GetMouseDelta(int& dx, int& dy) const
{
    dx = mouseX_ - prevMouseX_;
    dy = mouseY_ - prevMouseY_;
}