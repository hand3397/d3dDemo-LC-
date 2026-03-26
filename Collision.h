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

struct Ray
{
    XMFLOAT3 origin = XMFLOAT3(0.f, 0.f, 0.f);
    XMFLOAT3 dir = XMFLOAT3(0.f, 0.f, 0.f);
    // dir 값이 바뀌면 invDir을 다시 계산해야함
    XMFLOAT3 invDir = XMFLOAT3(0.f, 0.f, 0.f);

    Ray(XMFLOAT3 o, XMFLOAT3 d) : origin(o), dir(d)
    {
        const float epsilon = 1e-6f;

        XMVECTOR absDir = XMVectorAbs(XMLoadFloat3(&d));
        XMVECTOR isZero = XMVectorLess(absDir, XMVectorReplicate(epsilon));

        // 0인 성분만 epsilon으로 교체 (부호 유지)
        XMVECTOR safeDir = XMVectorSelect(XMLoadFloat3(&d), XMVectorReplicate(epsilon), isZero);
        XMStoreFloat3(&dir, safeDir);
        XMStoreFloat3(&invDir, XMVectorReciprocal(safeDir));
    }
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

    bool Intersects(const Ray& ray) const
    {
        float tMin, tMax;
        return RayCast(ray, tMin, tMax);
    }

    bool Intersects(const Ray& ray, float& tMin, float& tMax) const
    {
        return RayCast(ray, tMin, tMax);
    }

    bool RayCast(const Ray& ray, float& tmin, float& tmax) const
    {
        const XMVECTOR origin = XMLoadFloat3(&ray.origin);
        const XMVECTOR dir = XMLoadFloat3(&ray.dir);
        const XMVECTOR invDir = XMLoadFloat3(&ray.invDir);

        XMVECTOR bMin = XMLoadFloat3(&lowerBound);
        XMVECTOR bMax = XMLoadFloat3(&upperBound);

        // (bMin - origin) * invDir
        XMVECTOR t1 = (bMin - origin) * invDir;
        // (bMax - origin) * invDir
        XMVECTOR t2 = (bMax - origin) * invDir;

        // 각 축별로 진입 시간(min)과 탈출 시간(max)을 정렬
        XMVECTOR vMin = XMVectorMin(t1, t2);
        XMVECTOR vMax = XMVectorMax(t1, t2);

        // 모든 축 중 가장 늦게 들어오는 시간 (X, Y, Z 중 max)
        XMVECTOR maxNear = XMVectorMax(vMin, XMVectorSwizzle<1, 2, 0, 3>(vMin)); // x,y 비교
        maxNear = XMVectorMax(maxNear, XMVectorSwizzle<2, 0, 1, 3>(vMin));      // z 비교

        XMVECTOR minFar = XMVectorMin(vMax, XMVectorSwizzle<1, 2, 0, 3>(vMax));
        minFar = XMVectorMin(minFar, XMVectorSwizzle<2, 0, 1, 3>(vMax));

        float tNear = XMVectorGetX(maxNear);
        float tFar = XMVectorGetX(minFar);

        // 교차 조건: 
        // 1. 들어오는 시간이 나가는 시간보다 빨라야 함
        // 2. 나가는 시간이 0보다 커야 함 (Ray 뒤쪽에 있는 AABB 제외)
        if (tNear <= tFar && tFar > 0.0f) {
            tmin = tNear;
            tmax = tFar;
            return true;
        }

        return false;
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

