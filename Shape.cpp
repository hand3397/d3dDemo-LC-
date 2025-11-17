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

ConvexInfo SphereShape::GetConvexInfo(const XMMATRIX& transform) const
{
    ConvexInfo sphere;
    sphere.type = ShapeType::SPHERE;

    XMStoreFloat3(&sphere.center,
        XMVector3Transform(
            XMVectorSetW(XMLoadFloat3(&center_), 1.0f),
            transform));
    sphere.radius = radius_;

    return sphere;
}

void SphereShape::SetRadius(const float& radius)
{
    radius_ = radius;
}

ConvexInfo BoxShape::GetConvexInfo(const XMMATRIX& transform) const
{
    ConvexInfo box;
    box.type = ShapeType::BOX;

    XMStoreFloat3(&box.center,
        XMVector3Transform(
            XMVectorSetW(XMLoadFloat3(&center_), 1.0f),
            transform));
    box.halfSize = halfSize_;

    box.numPoints = 8;
    box.points = new XMFLOAT3[8];
    box.points[0] = XMFLOAT3(center_.x - halfSize_.x, center_.y - halfSize_.y, center_.z - halfSize_.z);
    box.points[1] = XMFLOAT3(center_.x + halfSize_.x, center_.y - halfSize_.y, center_.z - halfSize_.z);
    box.points[2] = XMFLOAT3(center_.x - halfSize_.x, center_.y + halfSize_.y, center_.z - halfSize_.z);
    box.points[3] = XMFLOAT3(center_.x + halfSize_.x, center_.y + halfSize_.y, center_.z - halfSize_.z);
    box.points[4] = XMFLOAT3(center_.x - halfSize_.x, center_.y - halfSize_.y, center_.z + halfSize_.z);
    box.points[5] = XMFLOAT3(center_.x + halfSize_.x, center_.y - halfSize_.y, center_.z + halfSize_.z);
    box.points[6] = XMFLOAT3(center_.x - halfSize_.x, center_.y + halfSize_.y, center_.z + halfSize_.z);
    box.points[7] = XMFLOAT3(center_.x + halfSize_.x, center_.y + halfSize_.y, center_.z + halfSize_.z);

    for (int i = 0; i < 8; i++) {
        XMStoreFloat3(&box.points[i],
            XMVector3Transform(
                XMVectorSetW(XMLoadFloat3(&box.points[i]), 1.0f),
                transform));
    }

    box.axes = new XMFLOAT3[3];
    XMVECTOR axesPoints[4] = {
        XMLoadFloat3(&box.points[0]),
        XMLoadFloat3(&box.points[1]),
        XMLoadFloat3(&box.points[2]),
        XMLoadFloat3(&box.points[3])
    };
    XMStoreFloat3(&box.axes[0], XMVector3NormalizeEst(axesPoints[1] - axesPoints[0]));
    XMStoreFloat3(&box.axes[1], XMVector3NormalizeEst(axesPoints[2] - axesPoints[0]));
    XMStoreFloat3(&box.axes[2], XMVector3NormalizeEst(axesPoints[3] - axesPoints[0]));

    return box;
}

void BoxShape::SetHalfSize(const XMFLOAT3& halfSize)
{
    halfSize_ = halfSize;
}

ConvexInfo::~ConvexInfo()
{
    if (points)
        delete[] points;
    if (axes)
        delete[] axes;
}

XMFLOAT3 ConvexInfo::GetFarthestPoint(const XMVECTOR& dir) const
{
    XMFLOAT3 out;
    XMVECTOR center = XMLoadFloat3(&this->center);

    switch (type) {
    case ShapeType::SPHERE: {
        XMStoreFloat3(&out, center + radius * XMVector3NormalizeEst(dir));
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

