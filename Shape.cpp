#include "Shape.h"

namespace spe {; 

int32_t operator|(ShapeType type1, ShapeType type2)
{
    return (int32_t)type1 | (int32_t)type2;
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
    SetCenter(center);
    SetType(ShapeType::SPHERE);
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

XMMATRIX SphereShape::ComputeLocalInertia(float mass) const
{
    float v = (2.0f / 5.0f) * mass * radius_ * radius_;
    return XMMatrixSet(
        v, 0, 0, 0,
        0, v, 0, 0,
        0, 0, v, 0,
        0, 0, 0, 1);
}

XMFLOAT3 Shape::GetSenter() const
{
    return center_;
}

void SphereShape::SetRadius(const float& radius)
{
    radius_ = radius;
}

BoxShape::BoxShape(const XMFLOAT3& center, const XMFLOAT3& halfSize) :
    halfSize_(halfSize)
{
    SetCenter(center);
    SetType(ShapeType::BOX);
}

void BoxShape::GetConvexInfo(const XMMATRIX& transform, ConvexInfo& out) const
{
    out.type = ShapeType::BOX;

    XMStoreFloat3(&out.center,
        XMVector3TransformCoord(XMLoadFloat3(&center_), transform));
    out.halfSize = halfSize_;

    out.numPoints = 8;
    out.points = new XMFLOAT3[8];
    out.points[0] = XMFLOAT3(center_.x - halfSize_.x, center_.y - halfSize_.y, center_.z - halfSize_.z);
    out.points[1] = XMFLOAT3(center_.x + halfSize_.x, center_.y - halfSize_.y, center_.z - halfSize_.z);
    out.points[2] = XMFLOAT3(center_.x - halfSize_.x, center_.y + halfSize_.y, center_.z - halfSize_.z);
    out.points[3] = XMFLOAT3(center_.x + halfSize_.x, center_.y + halfSize_.y, center_.z - halfSize_.z);
    out.points[4] = XMFLOAT3(center_.x - halfSize_.x, center_.y - halfSize_.y, center_.z + halfSize_.z);
    out.points[5] = XMFLOAT3(center_.x + halfSize_.x, center_.y - halfSize_.y, center_.z + halfSize_.z);
    out.points[6] = XMFLOAT3(center_.x - halfSize_.x, center_.y + halfSize_.y, center_.z + halfSize_.z);
    out.points[7] = XMFLOAT3(center_.x + halfSize_.x, center_.y + halfSize_.y, center_.z + halfSize_.z);

    for (int i = 0; i < 8; i++) {
        XMStoreFloat3(&out.points[i],
            XMVector3TransformCoord(XMLoadFloat3(&out.points[i]), transform));
    }

    out.axes = new XMFLOAT3[3];
    XMVECTOR axesPoints[4] = {
        XMLoadFloat3(&out.points[0]),
        XMLoadFloat3(&out.points[1]),
        XMLoadFloat3(&out.points[2]),
        XMLoadFloat3(&out.points[4])
    };
    out.numAxes = 3;
    XMStoreFloat3(&out.axes[0], XMVector3NormalizeEst(axesPoints[1] - axesPoints[0]));
    XMStoreFloat3(&out.axes[1], XMVector3NormalizeEst(axesPoints[2] - axesPoints[0]));
    XMStoreFloat3(&out.axes[2], XMVector3NormalizeEst(axesPoints[3] - axesPoints[0]));
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

XMMATRIX BoxShape::ComputeLocalInertia(float mass) const
{
    float hx = halfSize_.x, hy = halfSize_.y, hz = halfSize_.z;
    float Ixx = (1.0f / 12.0f) * mass * ((2 * hy) * (2 * hy) + (2 * hz) * (2 * hz));
    float Iyy = (1.0f / 12.0f) * mass * ((2 * hx) * (2 * hx) + (2 * hz) * (2 * hz));
    float Izz = (1.0f / 12.0f) * mass * ((2 * hx) * (2 * hx) + (2 * hy) * (2 * hy));

    // DirectXMath matrix (3×3 부분만 사용)
    return XMMatrixSet(
        Ixx, 0, 0, 0,
        0, Iyy, 0, 0,
        0, 0, Izz, 0,
        0, 0, 0, 1);
}

void BoxShape::SetHalfSize(const XMFLOAT3& halfSize)
{
    halfSize_ = halfSize;
}

XMFLOAT3 ConvexInfo::GetFarthestPoint(const XMVECTOR& dir) const
{
    XMFLOAT3 out;
    XMVECTOR center = XMLoadFloat3(&this->center);

    switch (type) {
    case ShapeType::SPHERE: {
        XMStoreFloat3(&out, center + radius * dir);
    }
        break;
    case ShapeType::BOX: {
        XMVECTOR axesVec[3] = { XMLoadFloat3(&axes[0]), XMLoadFloat3(&axes[1]), XMLoadFloat3(&axes[2]) };
        center += axesVec[0] * ((VecDot(axesVec[0], dir) > 0 ? 1.0f : -1.0f) * halfSize.x);
        center += axesVec[1] * ((VecDot(axesVec[1], dir) > 0 ? 1.0f : -1.0f) * halfSize.y);
        center += axesVec[2] * ((VecDot(axesVec[2], dir) > 0 ? 1.0f : -1.0f) * halfSize.z);
        XMStoreFloat3(&out, center);
    }
        break;
    case ShapeType::GROUND:
        break;
    case ShapeType::CYLINDER:
        break;
    case ShapeType::CAPSULE:
        break;
    }

    return out;
}

}

