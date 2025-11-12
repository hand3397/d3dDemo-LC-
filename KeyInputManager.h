#pragma once
#include <array>
#include <Windows.h>

// 최대 키 수 정의 (0~255)
constexpr int NUM_KEYS = 256;
constexpr int NUM_MOUSE_BUTTONS = 5; // LMB, RMB, MMB, X1, X2

enum MouseButton : uint8_t
{
    LMB = 0,  // Left Mouse Button
    RMB = 1,  // Right Mouse Button
    MMB = 2,  // Middle Mouse Button
    X1 = 3,
    X2 = 4
};

class KeyInputManager
{
public:
    KeyInputManager();

    // Win32 이벤트 처리
    void OnKeyDown(WPARAM wparam, LPARAM lParam);
    void OnKeyUp(WPARAM wparam, LPARAM lParam);
    void OnMouseDown(MouseButton button);
    void OnMouseUp(MouseButton button);
    void SetMousePos(int x, int y);

    // 매 프레임 호출, 상태 확정
    void Update();

    // 키 상태 쿼리
    bool IsKeyDown(UINT key) const;
    bool WasKeyPressed(UINT key) const;
    bool WasKeyReleased(UINT key) const;

    // 마우스 상태 쿼리
    bool IsMouseDown(UINT button) const;
    bool WasMousePressed(UINT button) const;
    bool WasMouseReleased(UINT button) const;
    void GetMousePos(int& x, int& y) const;
    void GetMouseDelta(int& dx, int& dy) const;

private:
    WPARAM MapLeftRightKeys(WPARAM vk, LPARAM lParam);

    std::array<bool, NUM_KEYS> prevKeyState_;
    std::array<bool, NUM_KEYS> currKeyState_;
    std::array<bool, NUM_KEYS> keyPressed_;
    std::array<bool, NUM_KEYS> keyReleased_;

    std::array<bool, NUM_MOUSE_BUTTONS> prevMouseState_;
    std::array<bool, NUM_MOUSE_BUTTONS> currMouseState_;
    std::array<bool, NUM_MOUSE_BUTTONS> mousePressed_;
    std::array<bool, NUM_MOUSE_BUTTONS> mouseReleased_;

    int mouseX_;
    int mouseY_;
    int prevMouseX_;
    int prevMouseY_;
};
