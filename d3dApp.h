#pragma once

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "d3dUtil.h"
#include "GameTimer.h"
#include "KeyInputManager.h"
#include "Renderer.h"

class D3DApp
{
protected:

    D3DApp(HINSTANCE hInstance);
    D3DApp(const D3DApp& rhs) = delete;
    D3DApp& operator=(const D3DApp& rhs) = delete;
    virtual ~D3DApp();

public:

    static D3DApp* GetApp();

    HINSTANCE AppInst()const;
    HWND      MainWnd()const;
    float     AspectRatio()const;

    int Run();

    virtual bool Initialize();
    virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:
    virtual void OnResize();
    virtual void Update(const GameTimer& gt) = 0;
    virtual void Draw(const GameTimer& gt) = 0;

    virtual void KeyInput(const GameTimer& gt) {}
protected:

    bool InitMainWindow();

    void CalculateFrameStats();

protected:

    static D3DApp* mApp;

    HINSTANCE mhAppInst = nullptr; // 애플리케이션 인스턴스 핸들
    HWND      mhMainWnd = nullptr; // 메인 윈도우 핸들
    bool      mHasD3dDevice = false;
    bool      mAppPaused = false;  // 애플리케이션이 일시정지 상태인지?
    bool      mMinimized = false;  // 애플리케이션이 최소화 되었는지?
    bool      mMaximized = false;  // 애플리케이션이 최대화 되었는지?
    bool      mResizing = false;   // 윈도우 크기 조절 바를 드래그 중인지?
    bool      mFullscreenState = false; // 전체 화면 모드인지?

    // Δt (델타 타임)과 게임 시간을 추적하는 데 사용된다.
    GameTimer mTimer;
    KeyInputManager keyInput_;
    unique_ptr<Renderer> renderer;

    // 파생 클래스가 생성자에서 초기 시작 값을 커스터마이즈하기 위해 이 값들을 설정해야 한다.
    std::wstring mMainWndCaption = L"d3d App";

    int mClientWidth = 800;
    int mClientHeight = 600;
};