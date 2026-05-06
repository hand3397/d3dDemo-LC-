#include "ContactSolver.h"


namespace spe {;

const float ContactSolver::NORMAL_STOP_VELOCITY = 0.01f;
const float ContactSolver::TANGENT_STOP_VELOCITY = 0.01f;
const float ContactSolver::NORMAL_SLEEP_VELOCITY_SQ = 1.0f;
const float ContactSolver::TANGENT_SLEEP_VELOCITY_SQ = 1.0f;
const float ContactSolver::POSITION_SOLVE_ALPHA = 0.25f;
const float ContactSolver::CONTACT_SLOP = 0.01f;

ContactSolver::ContactSolver(float duration, Contact** contacts,
	PositionBuffer* positions, VelocityBuffer* velocities,
	uint32_t numBodies, uint32_t numContacts) :
	duration_(duration), contacts_(contacts),
	positions_(positions), velocities_(velocities),
	numBodies_(numBodies), numContacts_(numContacts)
{
	contactConstraints_ = new ContactConstraint[numContacts_];

	for (uint32_t i = 0; i < numContacts_; ++i) {
		Contact* contact = contacts_[i];

		Fixture* fixtureA = contact->GetFixtureA();
		Fixture* fixtureB = contact->GetFixtureB();
		Shape* shapeA = fixtureA->GetShape();
		Shape* shapeB = fixtureB->GetShape();
		Rigidbody* bodyA = fixtureA->GetRigidbody();
		Rigidbody* bodyB = fixtureB->GetRigidbody();
		Manifold& manifold = contact->GetManifold();

        // Character의 충돌은 따로 처리
		if (contact->HasFlag(ContactFlag::CHARACTER)) {
			contactConstraints_[i].isKinematicContact_ = true;
		}

		contactConstraints_[i].points = manifold.points;
		contactConstraints_[i].numPoints = manifold.numPoints;
		XMStoreFloat3(&contactConstraints_[i].worldCenterA,
			XMVector3TransformCoord(XMLoadFloat3(&shapeA->GetCenter()), bodyA->GetTransformMatrix()));
		XMStoreFloat3(&contactConstraints_[i].worldCenterB,
			XMVector3TransformCoord(XMLoadFloat3(&shapeB->GetCenter()), bodyB->GetTransformMatrix()));
		contactConstraints_[i].invInertiaA = bodyA->GetInverseInertiaTensorWorld();
		contactConstraints_[i].invInertiaB = bodyB->GetInverseInertiaTensorWorld();
		contactConstraints_[i].bodyIdA = bodyA->GetIslandId();
		contactConstraints_[i].bodyIdB = bodyB->GetIslandId();
		contactConstraints_[i].invMassA = bodyA->GetInvMass();
		contactConstraints_[i].invMassB = bodyB->GetInvMass();
		contactConstraints_[i].friction = contact->GetFriction();
		contactConstraints_[i].restitution = contact->GetRestitution();
	}
}

void ContactSolver::Destroy()
{
	for (uint32_t i = 0; i < numContacts_; i++) {
		contactConstraints_[i].~ContactConstraint();
	}
	delete[] contactConstraints_;
	contactConstraints_ = nullptr;
}

void ContactSolver::SolveVelocityConstraints()
{
	for (uint32_t i = 0; i < numContacts_; ++i) {
		Contact* contact = contacts_[i];
		ContactConstraint& contactConstraint = contactConstraints_[i];
        // kinematic과 kinematic의 contact는 노말방향 속도 처리, 노말방향 위치 보간만
		if (contactConstraint.isKinematicContact_) {
			SolveKinematicVelocityConstraints(i);
            continue;
        }
		int32_t numPoints = contactConstraint.numPoints;
		int32_t idxA = contactConstraint.bodyIdA;
		int32_t idxB = contactConstraint.bodyIdB;

		// Load XMVECTOR
		XMVECTOR linearVelA = XMLoadFloat3(&velocities_[idxA].linearVelocity);
		XMVECTOR linearVelB = XMLoadFloat3(&velocities_[idxB].linearVelocity);
		XMVECTOR angularVelA = XMLoadFloat3(&velocities_[idxA].angularVelocity);
		XMVECTOR angularVelB = XMLoadFloat3(&velocities_[idxB].angularVelocity);

		XMVECTOR linearVelBufferA = XMLoadFloat3(&velocities_[idxA].linearVelocityBuffer);
		XMVECTOR linearVelBufferB = XMLoadFloat3(&velocities_[idxB].linearVelocityBuffer);
		XMVECTOR angularVelBufferA = XMLoadFloat3(&velocities_[idxA].angularVelocityBuffer);
		XMVECTOR angularVelBufferB = XMLoadFloat3(&velocities_[idxB].angularVelocityBuffer);

		bool isStop = true;

		float seperationSum = 0.0f;

		for (int32_t j = 0; j < numPoints; ++j) {
			seperationSum += contactConstraint.points[j].separation;
		}

		// 두 물체가 멀어지고 있음 -> 충돌 처리 x
		if (seperationSum <= 0.0f) {
			continue;
		}

		for (int32_t j = 0; j < numPoints; ++j) {
			ManifoldPoint& manifoldPoint = contactConstraint.points[j];
			const float dSeparation = manifoldPoint.separation / seperationSum;

			// 물체의 중심에서 충돌지점 까지의 벡터
			const XMVECTOR rA = XMLoadFloat3(&manifoldPoint.pointA) - XMLoadFloat3(&contactConstraint.worldCenterA);
			const XMVECTOR rB = XMLoadFloat3(&manifoldPoint.pointB) - XMLoadFloat3(&contactConstraint.worldCenterB);

			const XMMATRIX invInertiaA = XMLoadFloat3x3(&contactConstraint.invInertiaA);
			const XMMATRIX invInertiaB = XMLoadFloat3x3(&contactConstraint.invInertiaB);

			// 상대 속도 계산
			const XMVECTOR velocityA = linearVelA + XMVector3Cross(angularVelA, rA);
			const XMVECTOR velocityB = linearVelB + XMVector3Cross(angularVelB, rB);
			const XMVECTOR relativeVel = velocityB - velocityA;

			// 법선 방향 속도
			const XMVECTOR contactNormal = XMLoadFloat3(&manifoldPoint.normal);
			const float normalSpeed = VecDot(relativeVel, contactNormal);

			if (normalSpeed < -NORMAL_STOP_VELOCITY) {
				// 충돌 처리를 위한 법선방향 충격량 구하기
				// 충격량 = 속도 변화량 (반발 계수 포함) / 유효질량
				const float oldNormalImpulse = manifoldPoint.normalImpulse;

				float appliedNormalImpulse = -(1.0f + contactConstraint.restitution) * normalSpeed * dSeparation;
				const float inverseMasses = (contactConstraint.invMassA + contactConstraint.invMassB);

				// 노말 방향 유효질량 구하기
				const XMVECTOR crossA = XMVector3Cross(contactNormal, rA);
				float normalEffectiveMassA = VecDot(crossA, XMVector3TransformNormal(crossA, invInertiaA));
				const XMVECTOR crossB = XMVector3Cross(contactNormal, rB);
				float normalEffectiveMassB = VecDot(crossB, XMVector3TransformNormal(crossB, invInertiaB));

				appliedNormalImpulse =
					appliedNormalImpulse / (inverseMasses + normalEffectiveMassA + normalEffectiveMassB); // 유효질량의 합을 나눔

				// 이전 프레임의 충격량과 이번 프레임의 충격량을 더해 새로운 충격량 만들기
				const float newNormalImpulse = appliedNormalImpulse + oldNormalImpulse;

				manifoldPoint.normalImpulse = newNormalImpulse;
				 
				// 충격량이 음수라면 충돌이 멀어지고 있음.
				if (manifoldPoint.normalImpulse <= 0.0f) {
					manifoldPoint.normalImpulse = 0.0f;
				}

				const XMVECTOR normalImpulse = appliedNormalImpulse * contactNormal;

				linearVelBufferA -= contactConstraint.invMassA * normalImpulse;
				linearVelBufferB += contactConstraint.invMassB * normalImpulse;

				if (Vec3LengthSq(rA) == 0.0f) {
					throw std::runtime_error("normal rA is zero!!");
				}

				if (Vec3LengthSq(rB) == 0.0f) {
					throw std::runtime_error("normal rB is zero!!");
				}

				angularVelBufferA -= XMVector3TransformNormal(XMVector3Cross(rA, normalImpulse), invInertiaA);
				angularVelBufferB += XMVector3TransformNormal(XMVector3Cross(rB, normalImpulse), invInertiaB);
			}

			// 접선 방향 상대속도 계산
			const XMVECTOR tangentVel = relativeVel - (normalSpeed * contactNormal);
			const XMVECTOR tangent = XMVector3Normalize(tangentVel);
			float tangentSpeed = VecDot(tangent, tangentVel);

			if (tangentSpeed > TANGENT_STOP_VELOCITY) {
				// 충격량 = 속도 변화량 (반발 계수 포함) / 유효질량
				const float oldTangentImpulse = manifoldPoint.tangentImpulse;
				float newTangentImpulse = tangentSpeed * dSeparation;

				const float inverseMasses = (contactConstraint.invMassA + contactConstraint.invMassB);

				// 접선 방향 유효질량 구하기
				const XMVECTOR crossA = XMVector3Cross(tangent, rA);
				const float tangentEffectiveMassA = VecDot(crossA, XMVector3TransformNormal(crossA, invInertiaA));
				const XMVECTOR crossB = XMVector3Cross(tangent, rB);
				const float tangentEffectiveMassB = VecDot(crossB, XMVector3TransformNormal(crossB, invInertiaB));

				// 상대속도에 유효질량을 나눠 충격량 구하기
				newTangentImpulse = newTangentImpulse / (inverseMasses + tangentEffectiveMassA + tangentEffectiveMassB);
				newTangentImpulse += oldTangentImpulse;

				const float maxFriction = contactConstraint.friction * manifoldPoint.normalImpulse;
				// std::cout << "maxFriction: " << maxFriction << "\n";
				// std::cout << "newTangentImpulse: " << newTangentImpulse << "\n";

				newTangentImpulse = clamp(newTangentImpulse, -maxFriction, maxFriction);

				manifoldPoint.tangentImpulse = newTangentImpulse;

				const float appliedTangentImpulse = newTangentImpulse - oldTangentImpulse;

				const XMVECTOR tangentImpulse = appliedTangentImpulse * tangent;
				linearVelBufferA += contactConstraint.invMassA * tangentImpulse;
				linearVelBufferB -= contactConstraint.invMassB * tangentImpulse;

				if (Vec3LengthSq(rA) == 0.0f) {
					throw std::runtime_error("tangent rA is zero!!");
				}

				if (Vec3LengthSq(rB) == 0.0f) {
					throw std::runtime_error("tangent rB is zero!!");
				}

				if (Vec3LengthSq(tangent) == 0.0f) {
					throw std::runtime_error("tangent vector is zero!!");
				}

				angularVelBufferA += XMVector3TransformNormal(XMVector3Cross(rA, tangentImpulse), invInertiaA);
				angularVelBufferB -= XMVector3TransformNormal(XMVector3Cross(rB, tangentImpulse), invInertiaB);
			}
		}

		// store XMVECTOR
		linearVelA += linearVelBufferA;
		linearVelB += linearVelBufferB;
		angularVelA += angularVelBufferA;
		angularVelB += angularVelBufferB;

		XMStoreFloat3(&velocities_[idxA].linearVelocity, linearVelA);
		XMStoreFloat3(&velocities_[idxB].linearVelocity, linearVelB);
		XMStoreFloat3(&velocities_[idxA].angularVelocity, angularVelA);
		XMStoreFloat3(&velocities_[idxB].angularVelocity, angularVelB);
		// buffer 초기화
		velocities_[idxA].linearVelocityBuffer = XMFLOAT3(0.f, 0.f, 0.f);
		velocities_[idxB].linearVelocityBuffer = XMFLOAT3(0.f, 0.f, 0.f);
		velocities_[idxA].angularVelocityBuffer = XMFLOAT3(0.f, 0.f, 0.f);
		velocities_[idxB].angularVelocityBuffer = XMFLOAT3(0.f, 0.f, 0.f);
	}

}

void ContactSolver::SolvePositionConstraints()
{
	for (uint32_t i = 0; i < numContacts_; ++i) {
		Contact* contact = contacts_[i];
		ContactConstraint& contactConstraint = contactConstraints_[i];

        // kinematic과 kinematic의 contact는 따로 처리
		if (contactConstraint.isKinematicContact_) {
			SolveKinematicPositionConstraints(i);
			continue;
        }

		int32_t numPoints = contactConstraint.numPoints;
		int32_t indexA = contactConstraint.bodyIdA;
		int32_t indexB = contactConstraint.bodyIdB;

		float sumMass = contactConstraint.invMassA + contactConstraint.invMassB;
		float ratioA = contactConstraint.invMassA / sumMass;
		float ratioB = contactConstraint.invMassB / sumMass;

		// load XMVECTOR
		XMVECTOR positionBufferA = XMLoadFloat3(&positions_[indexA].positionBuffer);
		XMVECTOR positionBufferB = XMLoadFloat3(&positions_[indexB].positionBuffer);

		for (int32_t j = 0; j < numPoints; j++) {

			ManifoldPoint& manifoldPoint = contactConstraint.points[j];

			const XMVECTOR contactNormal = XMLoadFloat3(&manifoldPoint.normal);
			XMVECTOR movedPointA = XMLoadFloat3(&manifoldPoint.pointA) + positionBufferA;
			XMVECTOR movedPointB = XMLoadFloat3(&manifoldPoint.pointB) + positionBufferB;

			float separation = VecDot(contactNormal, (movedPointA - movedPointB));

			// 관통 해소된상태면 무시
			if (separation < CONTACT_SLOP) {
				continue;
			}

			// 관통 깊이에 따른 보정량 계산
			float correction = separation * POSITION_SOLVE_ALPHA / numPoints;
			XMVECTOR correctionVector = correction * contactNormal;

			positionBufferA -= correctionVector * ratioA;
			positionBufferB += correctionVector * ratioB;
		}

		// store XMVECTOR
		XMStoreFloat3(&positions_[indexA].positionBuffer, positionBufferA);
		XMStoreFloat3(&positions_[indexB].positionBuffer, positionBufferB);
	}
}

void ContactSolver::SolveKinematicVelocityConstraints(uint32_t i)
{
	Contact* contact = contacts_[i];
	ContactConstraint& contactConstraint = contactConstraints_[i];

	int32_t indexA = contactConstraint.bodyIdA;
	int32_t indexB = contactConstraint.bodyIdB;

	float sumMass = contactConstraint.invMassA + contactConstraint.invMassB;
	if (sumMass <= 0.0f) {
		return; // 둘 다 정적(Static) 물체라면 무시
	}

	float ratioA = contactConstraint.invMassA / sumMass;
	float ratioB = contactConstraint.invMassB / sumMass;

	// 현재 속도와 버퍼에 쌓인 속도를 모두 고려
	XMVECTOR linearVelA = XMLoadFloat3(&velocities_[indexA].linearVelocity);
	XMVECTOR linearVelB = XMLoadFloat3(&velocities_[indexB].linearVelocity);
	XMVECTOR linearVelBufferA = XMLoadFloat3(&velocities_[indexA].linearVelocityBuffer);
	XMVECTOR linearVelBufferB = XMLoadFloat3(&velocities_[indexB].linearVelocityBuffer);

	XMVECTOR totalVelA = linearVelA + linearVelBufferA;
	XMVECTOR totalVelB = linearVelB + linearVelBufferB;

	// PositionSolver와 동일하게 첫 번째 접점의 노말만 사용하여 단순화
	ManifoldPoint& manifoldPoint = contactConstraint.points[0];
	XMVECTOR contactNormal = XMLoadFloat3(&manifoldPoint.normal);

	// Position 제약과 동일하게 Y축 방향 충돌은 무시하고 XZ 평면에서만 속도를 제어
	contactNormal = XMVectorSetY(contactNormal, 0.0f);

	// 노말 벡터가 0이 아니라면 다시 정규화
	if (XMVectorGetX(XMVector3LengthSq(contactNormal)) > 0.0001f) {
		contactNormal = XMVector3Normalize(contactNormal);
	}
	else {
		return;
	}

	// 상대 속도 계산 (B - A)
	XMVECTOR relativeVel = totalVelB - totalVelA;

	// 법선(Normal) 방향의 투영 속도
	float normalSpeed = VecDot(contactNormal, relativeVel);

	// normalSpeed가 음수이면 두 물체가 가까워지고 있다는 뜻
	if (normalSpeed < 0.0f) {
		// 캐릭터는 통통 튀지 않으므로(Restitution = 0), 파고드는 속도만큼 완벽하게 상쇄하는 충격량 벡터 계산
		XMVECTOR impulseVector = normalSpeed * contactNormal;

		// A는 충격량 방향으로 이동하고(밀리고), B는 반대 방향으로 이동하여 상쇄
		// mass ratio를 곱하여 무거운 유닛이 덜 밀리도록 처리
		linearVelBufferA += impulseVector * ratioA;
		linearVelBufferB -= impulseVector * ratioB;

		// 결과 저장 (회전 속도는 캐릭터이므로 변경하지 않음)
		XMStoreFloat3(&velocities_[indexA].linearVelocityBuffer, linearVelBufferA);
		XMStoreFloat3(&velocities_[indexB].linearVelocityBuffer, linearVelBufferB);
	}
}

void ContactSolver::SolveKinematicPositionConstraints(uint32_t i)
{
	Contact* contact = contacts_[i];
	ContactConstraint& contactConstraint = contactConstraints_[i];

	float sumMass = contactConstraint.invMassA + contactConstraint.invMassB;
	float ratioA = contactConstraint.invMassA / sumMass;
	float ratioB = contactConstraint.invMassB / sumMass;

	int32_t indexA = contactConstraint.bodyIdA;
	int32_t indexB = contactConstraint.bodyIdB;

	// 현재까지 누적된 위치 보정량(Buffer) 로드
	XMVECTOR positionBufferA = XMLoadFloat3(&positions_[indexA].positionBuffer);
	XMVECTOR positionBufferB = XMLoadFloat3(&positions_[indexB].positionBuffer);

	ManifoldPoint& manifoldPoint = contactConstraint.points[0];
	const XMVECTOR contactNormal = XMLoadFloat3(&manifoldPoint.normal);

	// 최신 충돌 지점 및 분리 거리 계산
	// pointA, pointB는 충돌 지점이며, 여기에 현재 보정량을 더해 실시간 위치를 시뮬레이션합니다.
	XMVECTOR movedPointA = XMLoadFloat3(&manifoldPoint.pointA) + positionBufferA;
	XMVECTOR movedPointB = XMLoadFloat3(&manifoldPoint.pointB) + positionBufferB;

	// (A - B)를 노멀에 투영. 충돌 시 separation은 음수입니다.
	float currentSeparation = VecDot(contactNormal, movedPointA - movedPointB);

	// 관통되지 않았거나(양수), 무시할 수준(Slop)이면 종료
	if (currentSeparation > -CONTACT_SLOP) {
		return;
	}

	// 보정량 계산 및 XZ 평면 제한
	// 보정량은 음수인 currentSeparation을 0으로 만들기 위한 값입니다.
	float correction = (currentSeparation + CONTACT_SLOP) * POSITION_SOLVE_ALPHA;
	XMVECTOR correctionVector = correction * contactNormal;

	// 밀려나는 것은 XZ 방향으로 한정 (Y축 보정 제거)
	correctionVector = XMVectorSetY(correctionVector, 0.0f);

	// --- 4. 최종 위치 보정 적용 ---
	// correction이 음수이므로 -= 를 통해 A를 밀어내고 += 를 통해 B를 밀어냅니다.
	positionBufferA -= correctionVector * ratioA;
	positionBufferB += correctionVector * ratioB;

	// 결과 저장
	XMStoreFloat3(&positions_[indexA].positionBuffer, positionBufferA);
	XMStoreFloat3(&positions_[indexB].positionBuffer, positionBufferB);
}

void ContactSolver::CheckSleepContact()
{
	for (uint32_t i = 0; i < numContacts_; i++) {
		Contact* contact = contacts_[i];
		const ContactConstraint& contactConstraint = contactConstraints_[i];
		const uint32_t numPoints = contactConstraint.numPoints;
		const int32_t indexA = contactConstraint.bodyIdA;
		const int32_t indexB = contactConstraint.bodyIdB;

		const XMVECTOR relativeVelocity = XMLoadFloat3(&velocities_[indexA].linearVelocity) - XMLoadFloat3(&velocities_[indexB].linearVelocity);
		const XMVECTOR relativeAngularVelocity = XMLoadFloat3(&velocities_[indexA].angularVelocity) - XMLoadFloat3(&velocities_[indexB].angularVelocity);
		const XMVECTOR upVector = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

		for (int32_t j = 0; j < numPoints; ++j) {
			const ManifoldPoint& manifoldPoint = contactConstraint.points[j];

			if (Vec3LengthSq(relativeVelocity) > NORMAL_SLEEP_VELOCITY_SQ) {
				positions_[indexA].isNormalStop = false;
				positions_[indexB].isNormalStop = false;
			}

			if (Vec3LengthSq(relativeAngularVelocity) > TANGENT_SLEEP_VELOCITY_SQ) {
				positions_[indexA].isTangentStop = false;
				positions_[indexB].isTangentStop = false;
			}

			float normalDotUpVector = VecDot(XMLoadFloat3(&manifoldPoint.normal), upVector);
			if (normalDotUpVector < -0.3f) {
				positions_[indexA].isSupported = true;
			}
			if (normalDotUpVector > 0.3f) {
				positions_[indexB].isSupported = true;
			}
		}
	}
}

}
