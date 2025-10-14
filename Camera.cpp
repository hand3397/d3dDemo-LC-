//***************************************************************************************
// Camera.h by Frank Luna (C) 2011 All Rights Reserved.
//***************************************************************************************

#include "Camera.h"

using namespace DirectX;

Camera::Camera()
{
	SetLens(0.25f*MathHelper::Pi, 1.0f, 1.0f, 1000.0f);
}

Camera::~Camera()
{
}

void Camera::SetMode(CameraMode cameraMode)
{
	cameraMode_ = cameraMode;
}

CameraMode Camera::GetMode() const
{
	return cameraMode_;
}

XMVECTOR Camera::GetPosition()const
{
	return XMLoadFloat3(&position_);
}

XMFLOAT3 Camera::GetPosition3f()const
{
	return position_;
}

void Camera::SetTarget(const XMFLOAT3& target)
{
	target_ = target;
	viewDirty_ = true;
}

void Camera::SetPosition(float x, float y, float z)
{
	position_ = XMFLOAT3(x, y, z);
	viewDirty_ = true;
}

void Camera::SetPosition(const XMFLOAT3& v)
{
	position_ = v;
	viewDirty_ = true;
}

XMVECTOR Camera::GetRight()const
{
	return XMLoadFloat3(&right_);
}

XMFLOAT3 Camera::GetRight3f()const
{
	return right_;
}

XMVECTOR Camera::GetUp()const
{
	return XMLoadFloat3(&up_);
}

XMFLOAT3 Camera::GetUp3f()const
{
	return up_;
}

XMVECTOR Camera::GetLook()const
{
	return XMLoadFloat3(&look_);
}

XMFLOAT3 Camera::GetLook3f()const
{
	return look_;
}

float Camera::GetNearZ()const
{
	return nearZ_;
}

float Camera::GetFarZ()const
{
	return farZ_;
}

float Camera::GetAspect()const
{
	return aspect_;
}

float Camera::GetFovY()const
{
	return fovY_;
}

float Camera::GetFovX()const
{
	float halfWidth = 0.5f*GetNearWindowWidth();
	return 2.0f*atan(halfWidth / nearZ_);
}

float Camera::GetNearWindowWidth()const
{
	return aspect_ * nearWindowHeight_;
}

float Camera::GetNearWindowHeight()const
{
	return nearWindowHeight_;
}

float Camera::GetFarWindowWidth()const
{
	return aspect_ * farWindowHeight_;
}

float Camera::GetFarWindowHeight()const
{
	return farWindowHeight_;
}

void Camera::SetLens(float fovY, float aspect, float zn, float zf)
{
	// cache properties
	fovY_ = fovY;
	aspect_ = aspect;
	nearZ_ = zn;
	farZ_ = zf;

	nearWindowHeight_ = 2.0f * nearZ_ * tanf( 0.5f * fovY_);
	farWindowHeight_ = 2.0f * farZ_ * tanf( 0.5f * fovY_);

	XMMATRIX P = XMMatrixPerspectiveFovLH(fovY_, aspect_, nearZ_, farZ_);
	XMStoreFloat4x4(&projMat_, P);

	BoundingFrustum::CreateFromMatrix(boundingFrustum_, P);
}

void Camera::LookAt(FXMVECTOR pos, FXMVECTOR target, FXMVECTOR worldUp)
{
	XMVECTOR L = XMVector3Normalize(XMVectorSubtract(target, pos));
	XMVECTOR R = XMVector3Normalize(XMVector3Cross(worldUp, L));
	XMVECTOR U = XMVector3Cross(L, R);

	XMStoreFloat3(&position_, pos);
	XMStoreFloat3(&look_, L);
	XMStoreFloat3(&right_, R);
	XMStoreFloat3(&up_, U);

	viewDirty_ = true;
}

void Camera::LookAt(const XMFLOAT3& pos, const XMFLOAT3& target, const XMFLOAT3& up)
{
	XMVECTOR P = XMLoadFloat3(&pos);
	XMVECTOR T = XMLoadFloat3(&target);
	XMVECTOR U = XMLoadFloat3(&up);

	LookAt(P, T, U);

	viewDirty_ = true;
}

void Camera::SetOffset(const XMFLOAT3& offset)
{
	cameraOffset_ = offset;
}

XMMATRIX Camera::GetView()const
{
	assert(!viewDirty_);
	return XMLoadFloat4x4(&viewMat_);
}

XMMATRIX Camera::GetProj()const
{
	return XMLoadFloat4x4(&projMat_);
}


XMFLOAT4X4 Camera::GetView4x4f()const
{
	assert(!viewDirty_);
	return viewMat_;
}

XMFLOAT4X4 Camera::GetProj4x4f()const
{
	return projMat_;
}

BoundingFrustum Camera::GetBoundingFrustum() const
{
	return boundingFrustum_;
}

void Camera::Strafe(float d)
{
	// position_ += d*right_
	XMVECTOR s = XMVectorReplicate(d);
	XMVECTOR r = XMLoadFloat3(&right_);
	XMVECTOR p = XMLoadFloat3(&position_);
	XMStoreFloat3(&position_, XMVectorMultiplyAdd(s, r, p));

	viewDirty_ = true;
}

void Camera::Walk(float d)
{
	// position_ += d*look_
	XMVECTOR s = XMVectorReplicate(d);
	XMVECTOR l = XMLoadFloat3(&look_);
	XMVECTOR p = XMLoadFloat3(&position_);
	XMStoreFloat3(&position_, XMVectorMultiplyAdd(s, l, p));

	viewDirty_ = true;
}

void Camera::WorldUp(float d)
{
	// position_ += d*up_
	position_.y += d;

	viewDirty_ = true;
}

void Camera::RotatePitch(float radian)
{
	// Rotate up and look vector about the right vector.
	pitch_ += radian;
	pitch_ = std::clamp(pitch_, -maxPitch_, maxPitch_);

	viewDirty_ = true;
}

void Camera::RotateYaw(float radian)
{
	// Rotate the basis vectors about the world y-axis.
	yaw_ += radian;
	yaw_ = fmodf(yaw_, XM_2PI);

	viewDirty_ = true;
}

void Camera::RotateRoll(float radian)
{
	roll_ += radian;
	roll_ = fmodf(roll_, XM_2PI);

	viewDirty_ = true;
}

void Camera::SetPitch(float radian)
{
	pitch_ = radian;
}

void Camera::SetYaw(float radian)
{
	yaw_ = radian;
}

void Camera::SetRoll(float radian)
{
	roll_ = radian;
}

void Camera::UpdateViewMatrix()
{
	if(viewDirty_) {
		switch (cameraMode_) {
		case CameraMode::FPS: UpdateFPS();
			break;
		case CameraMode::TPS: UpdateTPS();
			break;
		case CameraMode::TopDown: UpdateTopDown();
			break;
		}
		viewDirty_ = false;
	}
}

void Camera::UpdateFPS()
{
	// 회전 행렬 생성
	XMMATRIX R = XMMatrixRotationRollPitchYaw(pitch_, yaw_, roll_);

	// 기본 축 벡터를 회전시켜서 look/right/up 계산
	XMVECTOR look = XMVector3TransformNormal(XMVectorSet(0, 0, 1, 0), R);
	XMVECTOR right = XMVector3TransformNormal(XMVectorSet(1, 0, 0, 0), R);
	XMVECTOR up = XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), R);

	XMStoreFloat3(&look_, look);
	XMStoreFloat3(&right_, right);
	XMStoreFloat3(&up_, up);

	position_ = {target_.x + cameraOffset_.x,
		target_.y + cameraOffset_.y, 
		target_.z + cameraOffset_.z };

	// 뷰 행렬 생성
	XMVECTOR pos = XMLoadFloat3(&position_);
	XMVECTOR target = pos + look;
	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&viewMat_, view);
}

void Camera::UpdateTPS()
{
	float x = radius_ * cosf(pitch_) * sinf(yaw_);
	float y = radius_ * sinf(pitch_);
	float z = radius_ * cosf(pitch_) * cosf(yaw_);

	position_ = { target_.x + x + cameraOffset_.x,
		target_.y + y + cameraOffset_.y,
		target_.z + z + cameraOffset_.z };

	XMVECTOR pos = XMLoadFloat3(&position_);
	XMVECTOR target = XMLoadFloat3(&target_) + XMLoadFloat3(&cameraOffset_);
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMVECTOR look = XMVector3Normalize(target - pos);
	XMStoreFloat3(&look_, look);

	XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, look));
	XMStoreFloat3(&right_, right);

	XMStoreFloat3(&up_, up);

	XMStoreFloat4x4(&viewMat_, XMMatrixLookAtLH(pos, target, up));
}

void Camera::UpdateTopDown()
{
	float height = 20.0f;   // 위에서 보는 높이
	float dist = 15.0f;   // 캐릭터에서 떨어진 거리

	XMFLOAT3 offset = { dist, height, dist };

	position_ = { target_.x + cameraOffset_.x + offset.x,
		target_.y + cameraOffset_.y + offset.y,
		target_.z + cameraOffset_.z + offset.z };

	XMVECTOR pos = XMLoadFloat3(&position_);
	XMVECTOR target = XMLoadFloat3(&target_);
	XMVECTOR up = XMVectorSet(0, 1, 0, 0);

	XMStoreFloat4x4(&viewMat_, XMMatrixLookAtLH(pos, target, up));
}


