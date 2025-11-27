#pragma once
#include "d3dUtil.h"
#include "Collision.h"

namespace spe {; 

enum class ShapeType
{
	SPHERE = (1 << 0),
	BOX = (1 << 1),
	GROUND = (1 << 2),
	CYLINDER = (1 << 3),
	CAPSULE = (1 << 4),
};

int32_t operator|(ShapeType type1, ShapeType type2);

struct ConvexInfo
{
	XMFLOAT3* points = nullptr;
	XMFLOAT3* axes = nullptr;

	int32_t numPoints;
	int32_t numAxes;

	XMFLOAT3 center;
	XMFLOAT3 halfSize;

	float radius;
	float height;

	ShapeType type;

	XMFLOAT3 GetFarthestPoint(const XMVECTOR& dir) const;
};

class Shape
{
public:
	virtual ~Shape() = default;

	virtual void GetConvexInfo(const XMMATRIX& transform, ConvexInfo& out) const = 0;
	virtual AABB GetAABB(const XMMATRIX& transform) const = 0;
	ShapeType GetType() const;
	virtual XMMATRIX ComputeLocalInertia(float mass) const = 0;
	XMFLOAT3 GetSenter()const;

	void SetType(ShapeType type);
	void SetCenter(const XMFLOAT3& center);
protected:
	XMFLOAT3 center_;
	ShapeType type_;
};

class SphereShape : public Shape
{
public:
	SphereShape(const XMFLOAT3& center, float radius);

	virtual void GetConvexInfo(const XMMATRIX& transform, ConvexInfo& out) const;
	virtual AABB GetAABB(const XMMATRIX& transform) const override;
	virtual XMMATRIX ComputeLocalInertia(float mass) const override;

	void SetRadius(const float& radius);
protected:
	float radius_;
};

class BoxShape : public Shape
{
public:
	BoxShape(const XMFLOAT3& center, const XMFLOAT3& halfSize);

	virtual void GetConvexInfo(const XMMATRIX& transform, ConvexInfo& out) const;
	virtual AABB GetAABB(const XMMATRIX& transform) const override;
	virtual XMMATRIX ComputeLocalInertia(float mass) const override;

	void SetHalfSize(const XMFLOAT3& halfSize);
protected:
	XMFLOAT3 halfSize_;
};

}

