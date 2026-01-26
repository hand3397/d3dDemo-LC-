#include "Shape.h"

namespace spe {; 

int32_t operator|(ShapeType type1, ShapeType type2)
{
    return (int32_t)type1 | (int32_t)type2;
}

XMVECTOR ConvexInfo::GetFarthestPoint(const XMVECTOR& dir) const
{
    XMVECTOR center = XMLoadFloat3(&this->center);

    switch (type) {
    case ShapeType::SPHERE: {
        return (center + radius * dir);
    }
                          break;
    case ShapeType::BOX: {
        XMVECTOR axesVec[3] = { XMLoadFloat3(&axes[0]), XMLoadFloat3(&axes[1]), XMLoadFloat3(&axes[2]) };
        center += axesVec[0] * ((VecDot(axesVec[0], dir) > 0 ? 1.0f : -1.0f) * halfSize.x);
        center += axesVec[1] * ((VecDot(axesVec[1], dir) > 0 ? 1.0f : -1.0f) * halfSize.y);
        center += axesVec[2] * ((VecDot(axesVec[2], dir) > 0 ? 1.0f : -1.0f) * halfSize.z);
        return center;
    }
                       break;
    case ShapeType::GROUND:
        break;
    case ShapeType::CYLINDER:
        break;
    case ShapeType::CAPSULE:
        break;
    }
}

ShapeType Shape::GetType() const
{
    return type_;
}

void Shape::SetType(ShapeType type)
{
    type_ = type;
}

void Shape::SetCenter(const XMFLOAT3& center)
{
    center_ = center;
}

SphereShape::SphereShape(const XMFLOAT3& center, float radius) :
    radius_(radius)
{
    center_ = center;
    type_ = ShapeType::SPHERE;
}

void SphereShape::GetConvexInfo(const XMMATRIX& transform, ConvexInfo& out) const
{
    out.type = ShapeType::SPHERE;

    XMStoreFloat3(&out.center, XMVector3TransformCoord(
        XMLoadFloat3(&center_), transform));
    out.radius = radius_;

    out.numAxes = 0;
    out.numPoints = 0;
}

AABB SphereShape::GetAABB(const XMMATRIX& transform) const
{
    XMVECTOR center = XMVector3TransformCoord(XMLoadFloat3(&center_), transform);
    XMVECTOR r = XMVectorReplicate(radius_);
    return AABB(center - r, center + r);
}

XMMATRIX SphereShape::ComputeLocalInvInertia(float mass) const
{
    float v = (2.0f / 5.0f) * mass * radius_ * radius_;
    float invV = 1.0f / v;
    return XMMatrixSet(
        invV, 0, 0, 0,
        0, invV, 0, 0,
        0, 0, invV, 0,
        0, 0, 0, 1);
}

XMFLOAT3 Shape::GetCenter() const
{
    return center_;
}

void SphereShape::SetRadius(float radius)
{
    radius_ = radius;
}

BoxShape::BoxShape(const XMFLOAT3& center, const XMFLOAT3& halfSize) :
    halfSize_(halfSize)
{
    center_ = center;
    type_ = ShapeType::BOX;
}

void BoxShape::GetConvexInfo(const XMMATRIX& transform, ConvexInfo& out) const
{
    out.type = ShapeType::BOX;

    // world center 계산
    XMVECTOR worldCenter = XMVector3TransformCoord(XMLoadFloat3(&center_), transform);
    XMStoreFloat3(&out.center, worldCenter);
    
    out.halfSize = halfSize_;
    
    XMVECTOR axisX = transform.r[0];
    XMVECTOR axisY = transform.r[1];
    XMVECTOR axisZ = transform.r[2];
    // world halfsize 계산
    XMVECTOR halfX = axisX * halfSize_.x;
    XMVECTOR halfY = axisY * halfSize_.y;
    XMVECTOR halfZ = axisZ * halfSize_.z;

    // 축 저장
    out.numAxes = 3;
    out.axes = new XMFLOAT3[3];
    XMStoreFloat3(&out.axes[0], XMVector3Normalize(halfX));
    XMStoreFloat3(&out.axes[1], XMVector3Normalize(halfY));
    XMStoreFloat3(&out.axes[2], XMVector3Normalize(halfZ));

    // 점 저장
    out.numPoints = 8;
    out.points = new XMFLOAT3[8];
    XMStoreFloat3(&out.points[0], worldCenter - halfX - halfY - halfZ); // (-X, -Y, -Z)
    XMStoreFloat3(&out.points[1], worldCenter - halfX - halfY + halfZ); // (-X, -Y, +Z)
    XMStoreFloat3(&out.points[2], worldCenter - halfX + halfY - halfZ); // (-X, +Y, -Z)
    XMStoreFloat3(&out.points[3], worldCenter - halfX + halfY + halfZ); // (-X, +Y, +Z)
    XMStoreFloat3(&out.points[4], worldCenter + halfX - halfY - halfZ); // (+X, -Y, -Z)
    XMStoreFloat3(&out.points[5], worldCenter + halfX - halfY + halfZ); // (+X, -Y, +Z)
    XMStoreFloat3(&out.points[6], worldCenter + halfX + halfY - halfZ); // (+X, +Y, -Z)
    XMStoreFloat3(&out.points[7], worldCenter + halfX + halfY + halfZ); // (+X, +Y, +Z)
}

AABB BoxShape::GetAABB(const XMMATRIX& transform) const
{
    XMVECTOR centerVec = XMVector3TransformCoord(XMLoadFloat3(&center_), transform);

    // 회전행렬의 절댓값 추출
    // XMMATRIX의 0,1,2행은 각각 X,Y,Z 축 벡터
    XMMATRIX absRot;
    absRot.r[0] = XMVectorAbs(transform.r[0]);
    absRot.r[1] = XMVectorAbs(transform.r[1]);
    absRot.r[2] = XMVectorAbs(transform.r[2]);
    absRot.r[3] = XMVectorZero();

    // worldHalf = |R| * localHalf
    XMVECTOR halfSizeVec = XMVector3Transform(XMLoadFloat3(&halfSize_), absRot);

    return AABB(centerVec - halfSizeVec, centerVec + halfSizeVec);
}

XMMATRIX BoxShape::ComputeLocalInvInertia(float mass) const
{
    float dx = halfSize_.x * 2.0f, dy = halfSize_.y * 2.0f, dz = halfSize_.z * 2.0f;
    float Ixx = (1.0f / 12.0f) * mass * (dy * dy) + (dz * dz);
    float Iyy = (1.0f / 12.0f) * mass * (dx * dx) + (dz * dz);
    float Izz = (1.0f / 12.0f) * mass * (dx * dx) + (dy * dy);

    // DirectXMath matrix (3×3 부분만 사용)
    return XMMatrixSet(
        (1.0f / Ixx), 0, 0, 0,
        0, (1.0f / Iyy), 0, 0,
        0, 0, (1.0f / Izz), 0,
        0, 0, 0, 1);
}

void BoxShape::SetHalfSize(const XMFLOAT3& halfSize)
{
    halfSize_ = halfSize;
}

CylinderShape::CylinderShape(const XMFLOAT3& center, float radius, float height) :
    radius_(radius), height_(height)
{
    center_ = center;
    type_ = ShapeType::CYLINDER;
}

void CylinderShape::GetConvexInfo(const XMMATRIX& transform, ConvexInfo& out) const
{
    out.type = ShapeType::CYLINDER;

    // world center 계산
    XMVECTOR worldCenter = XMVector3TransformCoord(XMLoadFloat3(&center_), transform);
    XMStoreFloat3(&out.center, worldCenter);

    out.radius = radius_;
    out.height = height_;

    XMVECTOR axisY = transform.r[1];

    // 축 저장
    out.numAxes = 1;
    out.axes = new XMFLOAT3[1];
    out.axes[0] = XMFLOAT3(0.f, 1.f, 0.f);
}

AABB CylinderShape::GetAABB(const XMMATRIX& transform) const
{
    // 1. 월드 중심 계산
    XMVECTOR centerVec = XMVector3TransformCoord(XMLoadFloat3(&center_), transform);

    // 2. 월드 상단 방향 벡터 (Up Vector) - 정규화된 축 가져오기
    XMVECTOR axisVec = XMVector3Normalize(transform.r[1]);
    XMFLOAT3 axis;
    XMStoreFloat3(&axis, axisVec);

    // 3. 각 축(X, Y, Z)에 대한 투영(Projection) 계산
    // 공식: Extent = (Height / 2) * |Axis_i| + Radius * sqrt(1 - Axis_i^2)
    // sqrt(1 - Axis_i^2)는 해당 축에 수직인 평면에 투영된 반지름 성분입니다.

    float h2 = height_ * 0.5f;

    // X축 범위: H/2 * |Ax| + R * sqrt(Ay^2 + Az^2)
    float extentX = h2 * fabsf(axis.x) + radius_ * sqrtf(axis.y * axis.y + axis.z * axis.z);

    // Y축 범위
    float extentY = h2 * fabsf(axis.y) + radius_ * sqrtf(axis.x * axis.x + axis.z * axis.z);

    // Z축 범위
    float extentZ = h2 * fabsf(axis.z) + radius_ * sqrtf(axis.x * axis.x + axis.y * axis.y);

    XMVECTOR extentVec = XMVectorSet(extentX, extentY, extentZ, 0.0f);

    return AABB(centerVec - extentVec, centerVec + extentVec);
}

XMMATRIX CylinderShape::ComputeLocalInvInertia(float mass) const
{
    // 실린더 관성 모멘트 공식 (Y축 기준)
    // I_xx = I_zz = (1/12) * m * (3*r^2 + h^2)
    // I_yy = (1/2) * m * r^2

    float r2 = radius_ * radius_;
    float h2 = height_ * height_;

    float Ixx = (1.0f / 12.0f) * mass * (3.0f * r2 + h2);
    float Iyy = 0.5f * mass * r2;
    float Izz = Ixx;

    return XMMatrixSet(
        (1.0f / Ixx), 0, 0, 0,
        0, (1.0f / Iyy), 0, 0,
        0, 0, (1.0f / Izz), 0,
        0, 0, 0, 1);
}

void CylinderShape::SetRadius(float radius)
{
    radius_ = radius;
}

void CylinderShape::SetHeight(float height)
{
    height_ = height;
}

}

