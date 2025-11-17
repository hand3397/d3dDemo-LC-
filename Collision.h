#pragma once
#include "d3dUtil.h"

namespace spe {; 

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

