#pragma once
#include "d3dUtil.h"
#include "Collision.h"

namespace spe {; 

enum class ShapeType
{
	SPHERE = (1 << 0),
	BOX = (1 << 1),
	GROUND = (1 << 2),	// height map
	CYLINDER = (1 << 3),
	CAPSULE = (1 << 4),
};

int32_t operator|(ShapeType type1, ShapeType type2);

struct ConvexInfo
{
	~ConvexInfo() {
		if (points) {
			delete[] points;
			points = nullptr;
		}
    }

	XMFLOAT3* points = nullptr;	// box, cylinder, capsule
	XMFLOAT3 axes[3];	// box, cylinder, capsule

	int32_t numPoints = 0;
	int32_t numAxes = 0; // numAxes <= 3

	// center & halfsize is local value
	XMFLOAT3 center = XMFLOAT3(0.f, 0.f, 0.f);
	XMFLOAT3 halfSize = XMFLOAT3(0.f, 0.f, 0.f);; // box 

	float radius = 0.f;	//sphere, cylinder, capsule
	float height = 0.f; //cylinder, capsule

	ShapeType type;

	// 도형의 점중 dir방향의 가장 먼 점을 반환하는 함수
	XMVECTOR GetFarthestPoint(const XMVECTOR& dir) const;
};

class Shape
{
public:
	Shape() = default;
	virtual ~Shape() = default;

	virtual void GetConvexInfo(const XMMATRIX& transform, ConvexInfo& out) const = 0;
	virtual AABB GetAABB(const XMMATRIX& transform) const = 0;
	ShapeType GetType() const;
	virtual XMMATRIX ComputeLocalInvInertia(float mass) const = 0;
	XMFLOAT3 GetCenter()const;

	void SetType(ShapeType type);
	void SetCenter(const XMFLOAT3& center);
protected:
	XMFLOAT3 center_ = XMFLOAT3(0.0f, 0.0f, 0.0f);
	ShapeType type_ = ShapeType::BOX;
};

class SphereShape : public Shape
{
public:
	SphereShape(const XMFLOAT3& center, float radius);

	virtual void GetConvexInfo(const XMMATRIX& transform, ConvexInfo& out) const override;
	virtual AABB GetAABB(const XMMATRIX& transform) const override;
	virtual XMMATRIX ComputeLocalInvInertia(float mass) const override;

	void SetRadius(float radius);
protected:
	float radius_;
};

class BoxShape : public Shape
{
public:
	BoxShape(const XMFLOAT3& center, const XMFLOAT3& halfSize);

	virtual void GetConvexInfo(const XMMATRIX& transform, ConvexInfo& out) const override;
	virtual AABB GetAABB(const XMMATRIX& transform) const override;
	virtual XMMATRIX ComputeLocalInvInertia(float mass) const override;

	void SetHalfSize(const XMFLOAT3& halfSize);
protected:
	XMFLOAT3 halfSize_;
};

class CylinderShape : public Shape
{
public:
	CylinderShape(const XMFLOAT3& center, float radius, float height);

	virtual void GetConvexInfo(const XMMATRIX& transform, ConvexInfo& out) const override;
	virtual AABB GetAABB(const XMMATRIX& transform) const override;
	virtual XMMATRIX ComputeLocalInvInertia(float mass) const override;

	void SetRadius(float radius);
	void SetHeight(float height);
protected:
	float radius_;
	float height_;
};

class CapsuleShape : public Shape
{
public:
	CapsuleShape(const XMFLOAT3& center, float radius, float height);

	virtual void GetConvexInfo(const XMMATRIX& transform, ConvexInfo& out) const override;
	virtual AABB GetAABB(const XMMATRIX& transform) const override;
	virtual XMMATRIX ComputeLocalInvInertia(float mass) const override;

	void SetRadius(float radius);
	void SetHeight(float height);
protected:
	float radius_;
	float height_; // cylinder height (capsule height = cylinder + radius * 2)
};

}

