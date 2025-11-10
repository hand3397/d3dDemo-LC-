#pragma once
#include "d3dUtil.h"

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
};

class Shape
{
public:
	virtual ~Shape() = default;
	virtual ConvexInfo GetConvexInfo(const XMMATRIX& transform) const = 0;

	ShapeType GetType() const;
	
	void SetType(ShapeType type);
	void SetCenter(const XMFLOAT3& center);
protected:
	XMFLOAT3 center_;
	ShapeType type_;
};

class SphereShape : public Shape
{
public:
	virtual ConvexInfo GetConvexInfo(const XMMATRIX& transform) const;
	void SetRadius(const float& radius);
protected:
	float radius_;
};

class BoxShape : public Shape
{
public:
	virtual ConvexInfo GetConvexInfo(const XMMATRIX& transform) const;
	void SetHalfSize(const XMFLOAT3& halfSize);
protected:
	XMFLOAT3 halfSize_;
};