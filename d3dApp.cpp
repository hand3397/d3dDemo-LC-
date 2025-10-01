#include "d3dApp.h"
#include <WindowsX.h>

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;

LRESULT CALLBACK
MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// hwnd를 전달합니다. WM_CREATE 등 메시지는 CreateWindow가 반환되기 전,
	// mhMainWnd가 유효하지 않을 때도 받을 수 있습니다.
	return D3DApp::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
}

D3DApp* D3DApp::mApp = nullptr;
D3DApp* D3DApp::GetApp()
{
	return mApp;
}

D3DApp::D3DApp(HINSTANCE hInstance) : mhAppInst(hInstance)
{
	// 한 번에 하나의 D3DApp만 생성될 수 있습니다.
	assert(mApp == nullptr);
	mApp = this;
}

D3DApp::~D3DApp()
{
}

HINSTANCE D3DApp::AppInst()const
{
	return mhAppInst;
}

HWND D3DApp::MainWnd()const
{
	return mhMainWnd;
}

float D3DApp::AspectRatio()const
{
	return static_cast<float>(mClientWidth) / mClientHeight;
}

int D3DApp::Run()
{
	MSG msg = { 0 };

	mTimer.Reset();

	while (msg.message != WM_QUIT) {
		// 윈도우 메시지가 있으면 처리합니다.
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		// 메시지가 없으면 애니메이션/게임 로직을 처리합니다.
		else {
			mTimer.Tick();

			if (!mAppPaused) {
				CalculateFrameStats();
				KeyInput(mTimer);
				Update(mTimer);
				Draw(mTimer);
			}
			else {
				Sleep(100);
			}
		}
	}

	if (renderer)
		renderer->FlushCommandQueue();

	return (int)msg.wParam;
}

bool D3DApp::Initialize()
{
	if (!InitMainWindow())
		return false;

	renderer = make_unique<Renderer>();
	if (!renderer->InitDirect3D(mhMainWnd, mClientWidth, mClientHeight))
		return false;

	return true;
}

void D3DApp::OnResize()
{
	renderer->OnResize(mClientWidth, mClientHeight);
}

LRESULT D3DApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
		// WM_ACTIVATE는 창이 활성화되거나 비활성화될 때 전송됩니다.
		// 창이 비활성화되면 게임을 일시정지하고,
		// 활성화되면 다시 시작합니다.
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE) {
			mAppPaused = true;
			mTimer.Stop();
		}
		else {
			mAppPaused = false;
			mTimer.Start();
		}
		return 0;

		// WM_SIZE는 사용자가 창 크기를 변경할 때 전송됩니다.
	case WM_SIZE:
		// 새 클라이언트 영역 크기를 저장합니다.
		mClientWidth = LOWORD(lParam);
		mClientHeight = HIWORD(lParam);
		if (mHasD3dDevice) {
			if (wParam == SIZE_MINIMIZED) {
				mAppPaused = true;
				mMinimized = true;
				mMaximized = false;
			}
			else if (wParam == SIZE_MAXIMIZED) {
				mAppPaused = false;
				mMinimized = false;
				mMaximized = true;
				OnResize();
			}
			else if (wParam == SIZE_RESTORED) {
				// 최소화 상태에서 복원?
				if (mMinimized) {
					mAppPaused = false;
					mMinimized = false;
					OnResize();
				}
				// 최대화 상태에서 복원?
				else if (mMaximized) {
					mAppPaused = false;
					mMaximized = false;
					OnResize();
				}
				else if (mResizing) {
					// 사용자가 창 크기 조절 막대를 드래그하는 동안에는
					// 여기서 버퍼를 리사이즈하지 않습니다.
					// 사용자가 막대를 계속 드래그하면 WM_SIZE 메시지가
					// 연속적으로 전송되기 때문에, 매 메시지마다 리사이즈하면
					// 의미 없고 느립니다.
					// 대신 사용자가 드래그를 마치고 막대를 놓으면
					// WM_EXITSIZEMOVE 메시지가 전송되고, 그때 리셋합니다.
				}
				else // SetWindowPos
				{
					OnResize();
				}
			}
		}
		return 0;

		// WM_ENTERSIZEMOVE는 사용자가 창 크기 조절 막대를 잡으면 전송됩니다.
	case WM_ENTERSIZEMOVE:
		mAppPaused = true;
		mResizing = true;
		mTimer.Stop();
		return 0;

		// WM_EXITSIZEMOVE는 사용자가 창 크기 조절 막대를 놓으면 전송됩니다.
		// 새 창 크기에 맞춰 모든 것을 재설정합니다.
	case WM_EXITSIZEMOVE:
		mAppPaused = false;
		mResizing = false;
		mTimer.Start();
		OnResize();
		return 0;

		// WM_DESTROY는 창이 파괴될 때 전송됩니다.
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

		// WM_MENUCHAR는 메뉴가 활성화된 상태에서 사용자가
		// 단축키에 해당하지 않는 키를 누를 때 전송됩니다.
	case WM_MENUCHAR:
		// Alt+Enter 시 삐 소리를 내지 않도록 처리
		return MAKELRESULT(0, MNC_CLOSE);

		// 창이 너무 작아지는 것을 방지하기 위해 처리합니다.
	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
		return 0;
	case WM_LBUTTONDOWN: keyInput_.OnMouseDown(MouseButton::LMB); return 0;
	case WM_LBUTTONUP:   keyInput_.OnMouseUp(MouseButton::LMB); return 0;
	case WM_RBUTTONDOWN: keyInput_.OnMouseDown(MouseButton::RMB); return 0;
	case WM_RBUTTONUP:   keyInput_.OnMouseUp(MouseButton::RMB); return 0;
	case WM_MBUTTONDOWN: keyInput_.OnMouseDown(MouseButton::MMB); return 0;
	case WM_MBUTTONUP:   keyInput_.OnMouseUp(MouseButton::MMB); return 0;
		return 0;
	case WM_MOUSEMOVE:
		keyInput_.SetMousePos(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_KEYUP:
		if (wParam == VK_ESCAPE) {
			PostQuitMessage(0);
		}
		if (wParam == VK_SHIFT)
			(lParam & (1 << 24)) ?
			keyInput_.OnKeyDown(VK_RSHIFT) : keyInput_.OnKeyDown(VK_LSHIFT);
		keyInput_.OnKeyUp(wParam);
		return 0;
	case WM_KEYDOWN:
		if (wParam == VK_SHIFT)
			(lParam & (1 << 24)) ?
			keyInput_.OnKeyDown(VK_RSHIFT) : keyInput_.OnKeyDown(VK_LSHIFT);
		keyInput_.OnKeyDown(wParam);
		return 0;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool D3DApp::InitMainWindow()
{
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = mhAppInst;
	wc.hIcon = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wc.lpszMenuName = 0;
	wc.lpszClassName = L"MainWnd";

	if (!RegisterClass(&wc)) {
		MessageBox(0, L"RegisterClass Failed.", 0, 0);
		return false;
	}

	// Compute window rectangle dimensions based on requested client area dimensions.
	RECT R = { 0, 0, mClientWidth, mClientHeight };
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	int width = R.right - R.left;
	int height = R.bottom - R.top;

	mhMainWnd = CreateWindow(L"MainWnd", mMainWndCaption.c_str(),
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, mhAppInst, 0);
	if (!mhMainWnd) {
		MessageBox(0, L"CreateWindow Failed.", 0, 0);
		return false;
	}

	ShowWindow(mhMainWnd, SW_SHOW);
	UpdateWindow(mhMainWnd);

	return true;
}

void D3DApp::CalculateFrameStats()
{
	// 이 코드는 초당 평균 프레임 수(FPS)와
	// 한 프레임을 렌더링하는 평균 시간(ms)을 계산합니다.
	// 계산된 통계는 윈도우 타이틀 바에 표시됩니다.

	static int frameCnt = 0;
	static float timeElapsed = 0.0f;

	frameCnt++;

	// 1초 단위로 평균을 계산합니다.
	if ((mTimer.TotalTime() - timeElapsed) >= 1.0f) {
		float fps = (float)frameCnt; // fps = frameCnt / 1초
		float mspf = 1000.0f / fps;  // 한 프레임당 밀리초

		wstring fpsStr = to_wstring(fps);
		wstring mspfStr = to_wstring(mspf);

		wstring windowText = mMainWndCaption +
			L"    fps: " + fpsStr +
			L"   mspf: " + mspfStr;

		SetWindowText(mhMainWnd, windowText.c_str());

		// 다음 평균 계산을 위해 값 초기화
		frameCnt = 0;
		timeElapsed += 1.0f;
	}
}
