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

	bool swapRef = dotB >= dotA;

	// refFace와 incFace의 FaceNormal은 물체의 밖을 바라보는 방향이다.
	Face refFace, incFace;
	if (swapRef) {
		// [수정] B가 Reference(바닥)일 때
		// 바닥의 '윗면'을 찾아야 하므로 EPA 법선(Down)의 반대인 Up 벡터(-resultEPA.normal)로 검색합니다.
		SetBoxFace(refFace, boxB, -resultEPA.normal); // Correct: Finds Top Face (Normal Up)

		// A(상자)는 '아랫면'을 찾아야 하므로 EPA 법선(Down) 그대로 검색합니다.
		SetBoxFace(incFace, boxA, resultEPA.normal);  // Correct: Finds Bottom Face (Normal Down)
	}
	else {
		// A가 Reference일 때 (반대 상황)
		SetBoxFace(refFace, boxA, -resultEPA.normal); // Search with Up -> Top of Box? (or opposite based on situation)
		// A가 Reference라면 보통 A가 더 평행한 상황. 
		// EPA가 A->B(Down)라면, A의 아랫면(Down)이 B의 윗면(Up)과 만남.
		// A의 아랫면을 찾으려면 Down(+Normal)으로 찾아야 함?
		// 기존 코드 로직상 swapRef가 아닐 때는 아래가 맞습니다.
		SetBoxFace(refFace, boxA, resultEPA.normal);
		SetBoxFace(incFace, boxB, -resultEPA.normal);
	}

	ContactFace contactFace;
	ComputeContactPolygon(contactFace, refFace, incFace);

	// Contact.cpp에서 수정한 BuildManifoldFromPolygon 사용
	BuildManifoldFromPolygon(collisionInfo, refFace, incFace, contactFace, resultEPA);

	if (swapRef) {
		for (int i = 0; i < collisionInfo.size; ++i) {
			std::swap(collisionInfo.pointA[i], collisionInfo.pointB[i]);

			// Reference(B)의 법선(Up)을 반전시켜 A->B 방향(Down)으로 만듦
			// 솔버는 A를 밀어내기 위해 -Impulse * Normal을 사용하므로
			// Normal이 Down이어야 A가 Up으로 밀려남. (Correct)
			collisionInfo.normal[i].x *= -1.0f;
			collisionInfo.normal[i].y *= -1.0f;
			collisionInfo.normal[i].z *= -1.0f;
		}
	}
}

}