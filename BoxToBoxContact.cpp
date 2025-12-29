#include "BoxToBoxContact.h"

namespace spe {;

BoxToBoxContact::BoxToBoxContact(Fixture* fixtureA, Fixture* fixtureB) :
	Contact(fixtureA, fixtureB)
{
}

Contact* BoxToBoxContact::Create(Fixture* fixtureA, Fixture* fixtureB)
{
	return new BoxToBoxContact(fixtureA, fixtureB);
}

void BoxToBoxContact::FindCollisionPoints(const ConvexInfo& boxA, const ConvexInfo& boxB,
	CollisionInfo& collisionInfo, ResultEPA& resultEPA, Polytope* simplexArray)
{
	// 1. 기준면(Reference Face) 선정을 위한 정렬도 검사
	// 충돌 법선(resultEPA.normal)과 각 상자의 축들이 얼마나 평행한지(내적 절대값) 확인합니다.
	// 내적값이 1에 가까울수록 해당 상자의 면이 충돌 평면과 평행하다는 뜻입니다.
	auto GetMaxAbsDot = [](const ConvexInfo& box, const XMVECTOR& normal) -> float {
		float maxDot = 0.0f;
		for (int i = 0; i < 3; ++i) {
			float dot = fabsf(VecDot(XMLoadFloat3(&box.axes[i]), normal));
			if (dot > maxDot) maxDot = dot;
		}
		return maxDot;
		};

	float dotA = GetMaxAbsDot(boxA, resultEPA.normal);
	float dotB = GetMaxAbsDot(boxB, resultEPA.normal);

	// boxB가 충돌 법선과 더 평행하다면(바닥 등), boxB를 Reference로 삼아야 더 안정적인 접촉점을 얻을 수 있습니다.
	bool swapRef = dotB > dotA;

	Face refFace, incFace; 
	if (swapRef) {
		// B가 Reference가 되므로, 법선 방향을 고려하여 설정
		SetBoxFace(refFace, boxB, -resultEPA.normal); // Normal B->A
		SetBoxFace(incFace, boxA, resultEPA.normal);  // Normal A->B
	}
	else {
		// 기본 상태 (A가 Reference)
		SetBoxFace(refFace, boxA, resultEPA.normal);
		SetBoxFace(incFace, boxB, -resultEPA.normal);
	}

	ContactFace contactFace;
	ComputeContactPolygon(contactFace, refFace, incFace);
	BuildManifoldFromPolygon(collisionInfo, refFace, incFace, contactFace, resultEPA);

	// 2. 만약 Reference/Incident를 뒤집었다면, 결과 데이터도 다시 원래 순서(A, B)로 맞춰줘야 합니다.
	if (swapRef) {
		for (int i = 0; i < collisionInfo.size; ++i) {
			// PointA와 PointB를 교체 (BuildManifold가 뒤집힌 기준으로 생성했으므로)
			std::swap(collisionInfo.pointA[i], collisionInfo.pointB[i]);

			// Reference 법선도 뒤집혔으므로 다시 반전 (B->A 벡터를 A->B로)
			collisionInfo.normal[i].x *= -1.0f;
			collisionInfo.normal[i].y *= -1.0f;
			collisionInfo.normal[i].z *= -1.0f;
		}
	}
}

}