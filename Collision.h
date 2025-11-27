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

	void combine(const AABB& aabb)
	{
		XMVECTOR minv = XMVectorMin(XMLoadFloat3(&lowerBound), XMLoadFloat3(&aabb.lowerBound));
		XMVECTOR maxv = XMVectorMax(XMLoadFloat3(&upperBound), XMLoadFloat3(&aabb.upperBound));
		XMStoreFloat3(&lowerBound, minv);
		XMStoreFloat3(&upperBound, maxv);
	}

	void combine(const AABB& aabb1, const AABB& aabb2)
	{
		XMVECTOR minv = XMVectorMin(XMLoadFloat3(&aabb1.lowerBound), XMLoadFloat3(&aabb2.lowerBound));
		XMVECTOR maxv = XMVectorMax(XMLoadFloat3(&aabb1.upperBound), XMLoadFloat3(&aabb2.upperBound));
		XMStoreFloat3(&lowerBound, minv);
		XMStoreFloat3(&upperBound, maxv);
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
	float normalImpulse;  // 법선 방향 충격량
	float tangentImpulse; // 접촉면 충격량
	float seperation;	  // 관통 깊이
	XMFLOAT3 pointA;	  // 충돌 지점의 위치
	XMFLOAT3 pointB;	  // 충돌 지점의 위치
	XMFLOAT3 normal;	  // 법선 벡터
};

const int32_t MAX_MANIFOLD_COUNT = 40;

struct Manifold
{
	ManifoldPoint points[MAX_MANIFOLD_COUNT];
	int32_t pointsCount;
};

}

