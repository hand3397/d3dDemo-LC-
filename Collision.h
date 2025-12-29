#pragma once
#include "d3dUtil.h"

namespace spe {; 

static inline float VecDot(const XMVECTOR& a, const XMVECTOR& b) noexcept
{
    return XMVectorGetX(XMVector3Dot(a, b));
}

static inline float Vec3Length(const XMVECTOR& a) noexcept
{
    return XMVectorGetX(XMVector3Length(a));
}

static inline float Vec3LengthSq(const XMVECTOR& a) noexcept
{
    return XMVectorGetX(XMVector3LengthSq(a));
}

struct Sweep
{
    XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    XMFLOAT4 orientation = { 0.0f, 0.0f, 0.0f, 1.0f };
    float alpha;
};

struct AABB
{
    AABB() = default;
    AABB(const XMFLOAT3& lb, const XMFLOAT3& ub)
    {
        XMStoreFloat3(&lowerBound, XMVectorMin(XMLoadFloat3(&lb), XMLoadFloat3(&ub)));
        XMStoreFloat3(&upperBound, XMVectorMax(XMLoadFloat3(&lb), XMLoadFloat3(&ub)));
    }
    AABB(const XMVECTOR& lb, const XMVECTOR& ub)
    {
        XMStoreFloat3(&lowerBound, XMVectorMin(lb, ub));
        XMStoreFloat3(&upperBound, XMVectorMax(lb, ub));
    }

    float GetSurface() const
    {
        XMFLOAT3 len;
        XMStoreFloat3(&len, XMLoadFloat3(&upperBound) - XMLoadFloat3(&lowerBound));

        return (len.x * len.y + len.y * len.z + len.z * len.x) * 2.0f;
    }

    void Combine(const AABB& aabb)
    {
        XMVECTOR minv = XMVectorMin(XMLoadFloat3(&lowerBound), XMLoadFloat3(&aabb.lowerBound));
        XMVECTOR maxv = XMVectorMax(XMLoadFloat3(&upperBound), XMLoadFloat3(&aabb.upperBound));
        XMStoreFloat3(&lowerBound, minv);
        XMStoreFloat3(&upperBound, maxv);
    }

    void Combine(const AABB& aabb1, const AABB& aabb2)
    {
        XMVECTOR minV = XMVectorMin(XMLoadFloat3(&aabb1.lowerBound), XMLoadFloat3(&aabb2.lowerBound));
        XMVECTOR maxV = XMVectorMax(XMLoadFloat3(&aabb1.upperBound), XMLoadFloat3(&aabb2.upperBound));
        XMStoreFloat3(&lowerBound, minV);
        XMStoreFloat3(&upperBound, maxV);
    }

    bool Contains(const AABB& other) const
    {
        XMVECTOR aMin = XMLoadFloat3(&lowerBound);
        XMVECTOR aMax = XMLoadFloat3(&upperBound);
        XMVECTOR bMin = XMLoadFloat3(&other.lowerBound);
        XMVECTOR bMax = XMLoadFloat3(&other.upperBound);

        // a.min <= b.min
        XMVECTOR cmpMin = XMVectorLessOrEqual(aMin, bMin);
        // a.max >= b.max
        XMVECTOR cmpMax = XMVectorGreaterOrEqual(aMax, bMax);

        // AND all comparisons
        return XMVector3EqualInt(XMVectorAndInt(cmpMin, cmpMax), XMVectorTrueInt());
    }

    bool Intersects(const AABB& other) const
    {
        XMVECTOR aMin = XMLoadFloat3(&lowerBound);
        XMVECTOR aMax = XMLoadFloat3(&upperBound);
        XMVECTOR bMin = XMLoadFloat3(&other.lowerBound);
        XMVECTOR bMax = XMLoadFloat3(&other.upperBound);

        // overlap on all axes:
        // aMax >= bMin && bMax >= aMin
        XMVECTOR cmp1 = XMVectorGreaterOrEqual(aMax, bMin);
        XMVECTOR cmp2 = XMVectorGreaterOrEqual(bMax, aMin);

        return XMVector3EqualInt(XMVectorAndInt(cmp1, cmp2), XMVectorTrueInt());
    }

    XMFLOAT3 lowerBound;
    XMFLOAT3 upperBound;
};

struct ManifoldPoint
{
	float normalImpulse  = 0.0f;    // 법선 방향 충격량
	float tangentImpulse = 0.0f;    // 접촉면 충격량
	float separation = 0.0f;	    // 관통 깊이
	XMFLOAT3 pointA = XMFLOAT3(0.f, 0.f, 0.f);	  // 충돌 지점의 위치
	XMFLOAT3 pointB = XMFLOAT3(0.f, 0.f, 0.f);	  // 충돌 지점의 위치
	XMFLOAT3 normal = XMFLOAT3(0.f, 0.f, 0.f);	  // 법선 벡터
};

const int32_t MAX_MANIFOLD_COUNT = 40;

struct Manifold
{
	ManifoldPoint points[MAX_MANIFOLD_COUNT];
	int32_t numPoints = 0;
};

}

