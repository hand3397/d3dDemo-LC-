#include "d3dApp.h"
#include "MathHelper.h"
#include "UploadBuffer.h"
#include "FrameResource.h"
#include "Camera.h"
#include "RenderItem.h"
#include "Scene.h"
#include "Renderer.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

class Direct3DDemo : public D3DApp
{
public:
	Direct3DDemo(HINSTANCE hInstance);
	Direct3DDemo(const Direct3DDemo& rhs) = delete;
	Direct3DDemo& operator=(const Direct3DDemo& rhs) = delete;
	~Direct3DDemo();

	virtual bool Initialize()override;
	 
private:
    virtual void OnResize()override;
    virtual void Update(const GameTimer& gt)override;
    virtual void Draw(const GameTimer& gt)override;

	virtual void KeyInput(const GameTimer& gt);
private:
	unique_ptr<Scene> scene_;
};

void CreateConsole()
{
	AllocConsole();

	FILE* fp;
	freopen_s(&fp, "CONOUT$", "w", stdout);
	freopen_s(&fp, "CONOUT$", "w", stderr);
	freopen_s(&fp, "CONIN$", "r", stdin);

	std::ios::sync_with_stdio(true);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	CreateConsole();
	_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    try
    {
		Direct3DDemo theApp(hInstance);
        if(!theApp.Initialize())
            return 0;

        return theApp.Run();
    }
    catch(DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}

Direct3DDemo::Direct3DDemo(HINSTANCE hInstance)
: D3DApp(hInstance) 
{
}

Direct3DDemo::~Direct3DDemo()
{
}

bool Direct3DDemo::Initialize()
{
	mainWndCaption_ = L"d3d_Demo";
	clientWidth_ = 1280;
	clientHeight_ = 720;

    if(!D3DApp::Initialize())
		return false;
	
	scene_ = make_unique<Scene>();

	renderer_->CommandListReset();
	scene_->InitScene(renderer_->GetDevice(), renderer_->GetCommandList(), clientWidth_, clientHeight_);
	scene_->GetCamera()->SetLens(0.25f * MathHelper::Pi, AspectRatio(), 0.1f, 1000.0f);
	renderer_->InitScene(scene_.get());
	renderer_->CommandListClose();

	return true;
}

void Direct3DDemo::OnResize()
{
	// 창의 크기가 바뀌면 종횡비를 다시 갱신한다.
	// 투영 행렬을 다시 계산한다.
	Camera* camera = scene_->GetCamera();
	if (camera)
		camera->SetLens(0.25f * MathHelper::Pi, AspectRatio(), 0.1f, 1000.0f);

	renderer_->OnResize(clientWidth_, clientHeight_);
	scene_->OnResize(clientWidth_, clientHeight_);
}

void Direct3DDemo::Update(const GameTimer& gt)
{
	const float dt = gt.DeltaTime();
	keyInput_.Update();
	scene_->Update(gt);

	// update CB
	renderer_->Update(gt, scene_.get());
}

void Direct3DDemo::Draw(const GameTimer& gt)
{
	renderer_->Draw(scene_.get());
}

void Direct3DDemo::KeyInput(const GameTimer& gt)
{
	float dt = gt.DeltaTime();
	
	// Mouse
	if (keyInput_.WasMousePressed(MouseButton::LMB) || keyInput_.WasMousePressed(MouseButton::RMB))
		SetCapture(hMainWnd_);
	if (keyInput_.WasMouseReleased(MouseButton::LMB) || keyInput_.WasMouseReleased(MouseButton::RMB))
		ReleaseCapture();

	// KeyBoard
	scene_->KeyInput(keyInput_, dt);
}
