#pragma once
#include "d3dUtil.h"

enum class CameraMode
{
	FPS,      // 기존 FPS/Fly 모드 (Walk, Strafe, Pitch, RotateY 등 사용)
	TPS,       // 캐릭터 중심 공전
	TopDown    // 쿼터뷰/탑뷰
};

class Camera
{
public:
	Camera();
	~Camera();

	void SetMode(CameraMode cameraMode);
	CameraMode GetMode() const;
	
	// Get/Set world camera position.
	void SetPosition(float x, float y, float z);
	void SetPosition(const DirectX::XMFLOAT3& v);
	XMVECTOR GetPosition()const;
	XMFLOAT3 GetPosition3f()const;
	
	void SetTarget(const XMFLOAT3& target);
	void SetOffset(const XMFLOAT3& offset);

	// Get camera basis vectors.
	XMVECTOR GetRight()const;
	XMFLOAT3 GetRight3f()const;
	XMVECTOR GetUp()const;
	XMFLOAT3 GetUp3f()const;
	XMVECTOR GetLook()const;
	XMFLOAT3 GetLook3f()const;

	// Get frustum properties.
	float GetNearZ()const;
	float GetFarZ()const;
	float GetAspect()const;
	float GetFovY()const;
	float GetFovX()const;

	// Get near and far plane dimensions in view space coordinates.
	float GetNearWindowWidth()const;
	float GetNearWindowHeight()const;
	float GetFarWindowWidth()const;
	float GetFarWindowHeight()const;
	
	// Set frustum.
	void SetLens(float fovY, float aspect, float zn, float zf);

	// Define camera space via LookAt parameters.
	void LookAt(DirectX::FXMVECTOR pos, DirectX::FXMVECTOR target, DirectX::FXMVECTOR worldUp);
	void LookAt(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& target, const DirectX::XMFLOAT3& up);

	// Get View/Proj matrices.
	XMMATRIX GetView()const;
	XMMATRIX GetProj()const;

	XMFLOAT4X4 GetView4x4f()const;
	XMFLOAT4X4 GetProj4x4f()const;

	// Strafe/Walk the camera a distance d.
	void Strafe(float d);
	void Walk(float d);
	void WorldUp(float d);

	// Rotate the camera.
	void RotatePitch(float radian);
	void RotateYaw(float radian);
	void RotateRoll(float radian);

	void SetPitch(float radian);
	void SetYaw(float radian);
	void SetRoll(float radian);

	// After modifying camera position/orientation, call to rebuild the view matrix.
	void UpdateViewMatrix();
private:
	void UpdateFPS();
	void UpdateTPS();
	void UpdateTopDown();

	CameraMode cameraMode_ = CameraMode::FPS;

	// Camera coordinate system with coordinates relative to world space.
	XMFLOAT3 position_ = { 0.0f, 0.0f, 0.0f };
	XMFLOAT3 target_ = { 0.0f, 0.0f, 0.0f };
	XMFLOAT3 cameraOffset_ = { 0.0f, 0.0f, 0.0f };

	XMFLOAT3 right_ = { 1.0f, 0.0f, 0.0f };
	XMFLOAT3 up_ = { 0.0f, 1.0f, 0.0f };
	XMFLOAT3 look_ = { 0.0f, 0.0f, 1.0f };

	float pitch_ = 0.0f;
	float yaw_ = 0.0f;
	float roll_ = 0.0f;
	// TPSmode targetPos to cameraPos distance
	float radius_ = 10.0f;
	const float maxPitch_ = XMConvertToRadians(89.0f);

	// Cache frustum properties.
	float nearZ_ = 0.0f;
	float farZ_ = 0.0f;
	float aspect_ = 0.0f;
	float fovY_ = 0.0f;
	float nearWindowHeight_ = 0.0f;
	float farWindowHeight_ = 0.0f;

	bool viewDirty_ = true;

	// Cache View/Proj matrices.
	XMFLOAT4X4 viewMat_ = MathHelper::Identity4x4();
	XMFLOAT4X4 projMat_ = MathHelper::Identity4x4();
};
