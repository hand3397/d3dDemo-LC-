#include "ContactSolver.h"

namespace spe {; 

const float ContactSolver::NORMAL_STOP_VELOCITY = 0.0001f;
const float ContactSolver::TANGENT_STOP_VELOCITY = 0.0001f;
const float ContactSolver::NORMAL_SLEEP_VELOCITY = 1.0f;
const float ContactSolver::TANGENT_SLEEP_VELOCITY = 1.0f;
const float ContactSolver::POSITION_SOLVE_ALPHA = 0.3f;

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

		// contact data
		Fixture* fixtureA = contact->GetFixtureA();
		Fixture* fixtureB = contact->GetFixtureB();
		Shape* shapeA = fixtureA->GetShape();
		Shape* shapeB = fixtureB->GetShape();
		Rigidbody* bodyA = fixtureA->GetRigidbody();
		Rigidbody* bodyB = fixtureB->GetRigidbody();
		Manifold& manifold = contact->GetManifold();

		// contact data -> contact constraints
		contactConstraints_[i].points = manifold.points;
		contactConstraints_[i].numPoints = manifold.pointsCount;
		XMStoreFloat3(&contactConstraints_[i].worldCenterA,
			XMVector3TransformCoord(XMLoadFloat3(&shapeA->GetSenter()), bodyA->GetTransformMatrix()));
		XMStoreFloat3(&contactConstraints_[i].worldCenterB,
			XMVector3TransformCoord(XMLoadFloat3(&shapeB->GetSenter()), bodyB->GetTransformMatrix()));
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

void ContactSolver::destroy()
{
	for (uint32_t i = 0; i < numContacts_; i++) {
		contactConstraints_[i].~ContactConstraint();
	}
	delete[] contactConstraints_;
	contactConstraints_ = nullptr;
}

void ContactSolver::solveVelocityConstraints()
{
	for (uint32_t i = 0; i < numContacts_; i++) {
		const ContactConstraint& contactConstraint = contactConstraints_[i];
		uint32_t numPoints = contactConstraint.numPoints;
		int32_t idxA = contactConstraint.bodyIdA;
		int32_t idxB = contactConstraint.bodyIdB;

		XMVECTOR positionA = XMLoadFloat3(&positions_[idxA].position);
		XMVECTOR positionB = XMLoadFloat3(&positions_[idxB].position);
		XMVECTOR positionBufferA = XMLoadFloat3(&positions_[idxA].positionBuffer);
		XMVECTOR positionBufferB = XMLoadFloat3(&positions_[idxB].positionBuffer);

		XMVECTOR linearVelocityA = XMLoadFloat3(&velocities_[idxA].linearVelocity);
		XMVECTOR linearVelocityB = XMLoadFloat3(&velocities_[idxB].linearVelocity);
		XMVECTOR linearVelocityBufferA = XMLoadFloat3(&velocities_[idxA].linearVelocityBuffer);
		XMVECTOR linearVelocityBufferB = XMLoadFloat3(&velocities_[idxB].linearVelocityBuffer);

		XMVECTOR angularVelocityA = XMLoadFloat3(&velocities_[idxA].angularVelocity);
		XMVECTOR angularVelocityB = XMLoadFloat3(&velocities_[idxB].angularVelocity);
		XMVECTOR angularVelocityBufferA = XMLoadFloat3(&velocities_[idxA].angularVelocityBuffer);
		XMVECTOR angularVelocityBufferB = XMLoadFloat3(&velocities_[idxB].angularVelocityBuffer);

		// 침투 깊이 합
		float seperationSum = 0.0f;

		for (uint32_t j = 0; j < numPoints; ++j) {
			seperationSum += contactConstraint.points[j].seperation;
		}

		if (seperationSum <= 0.0f) {
			continue;
		}

		for (uint32_t j = 0; j < numPoints; ++j) {
			ManifoldPoint& manifoldPoint = contactConstraint.points[j];

			XMVECTOR rA = XMLoadFloat3(&manifoldPoint.pointA) - XMLoadFloat3(&contactConstraint.worldCenterA); // bodyA의 충돌 지점까지의 벡터
			XMVECTOR rB = XMLoadFloat3(&manifoldPoint.pointB) - XMLoadFloat3(&contactConstraint.worldCenterB); // bodyB의 충돌 지점까지의 벡터

			// 상대 속도 계산
			XMVECTOR velocityA = linearVelocityA + XMVector3Cross(angularVelocityA, rA);
			XMVECTOR velocityB = linearVelocityB + XMVector3Cross(angularVelocityB, rB);
			XMVECTOR relativeVelocity = velocityB - velocityA;

			// 법선 방향 속도
			XMVECTOR normalVec = XMLoadFloat3(&manifoldPoint.normal);
			float normalSpeed = VecDot(relativeVelocity, normalVec);
			float appliedNormalImpulse;

			// normalSpeed < 0 두물체가 가까워지고 있음
			// else 멀어지고 있음0
			if (normalSpeed < -NORMAL_STOP_VELOCITY) {
				// 충격량 = 속도 변화량 (반발 계수 포함) / 유효질량
				float oldNormalImpulse = manifoldPoint.normalImpulse;

				// normal방향 속도 변화량 계산
				appliedNormalImpulse =
					-(1.0f + contactConstraint.restitution) * normalSpeed * (manifoldPoint.seperation / seperationSum);

				// 유효질량 계산 effective
				float inverseMasses = (contactConstraint.invMassA + contactConstraint.invMassB);

				XMMATRIX invInertiaMatA = XMLoadFloat3x3(&contactConstraint.invInertiaA);
				XMMATRIX invInertiaMatB = XMLoadFloat3x3(&contactConstraint.invInertiaB);
				XMVECTOR torqueArmA = XMVector3Cross(normalVec, rA);
				XMVECTOR torqueArmB = XMVector3Cross(normalVec, rB);
				// invInertia가 대칭이라고 가정해서 transpose를 하지 않는다
				float normalEffectiveMassA = VecDot(torqueArmA, XMVector3Transform(torqueArmA, invInertiaMatA));
				float normalEffectiveMassB = VecDot(torqueArmB, XMVector3Transform(torqueArmB, invInertiaMatB));

				appliedNormalImpulse /= (inverseMasses + normalEffectiveMassA + normalEffectiveMassB);
				appliedNormalImpulse += oldNormalImpulse;

				manifoldPoint.normalImpulse = appliedNormalImpulse;

				// 두 물체는 서로 당기지 않는다.
				if (manifoldPoint.normalImpulse <= 0.0f) {
					manifoldPoint.normalImpulse = 0.0f;
				}

				linearVelocityBufferA -= contactConstraint.invMassA * appliedNormalImpulse * normalVec;
				linearVelocityBufferB += contactConstraint.invMassB * appliedNormalImpulse * normalVec;

				angularVelocityBufferA -= XMVector3Transform(
					XMVector3Cross(rA, appliedNormalImpulse * normalVec), invInertiaMatA);
				angularVelocityBufferB += XMVector3Transform(
					XMVector3Cross(rB, appliedNormalImpulse * normalVec), invInertiaMatB);
			}

			// 접선 방향 충격량 계산
			XMVECTOR tangentVelocity = relativeVelocity - (normalSpeed * normalVec);
			XMVECTOR tangentVec = XMVector3Normalize(tangentVelocity);
			float tangentSpeed = VecDot(tangentVec, tangentVelocity);

			if (tangentSpeed > TANGENT_STOP_VELOCITY) {
				float oldTangentImpulse = manifoldPoint.tangentImpulse;

				float newTangentImpulse = tangentSpeed * (manifoldPoint.seperation / seperationSum);

				float inverseMasses = (contactConstraint.invMassA + contactConstraint.invMassB);

				XMMATRIX invInertiaMatA = XMLoadFloat3x3(&contactConstraint.invInertiaA);
				XMMATRIX invInertiaMatB = XMLoadFloat3x3(&contactConstraint.invInertiaB);
				XMVECTOR torqueArmA = XMVector3Cross(tangentVec, rA);
				XMVECTOR torqueArmB = XMVector3Cross(tangentVec, rB);
				float tangentEffectiveMassA =
					VecDot(torqueArmA, XMVector3Transform(torqueArmA, invInertiaMatA));
				float tangentEffectiveMassB =
					VecDot(torqueArmB, XMVector3Transform(torqueArmB, invInertiaMatB));
				newTangentImpulse /= (inverseMasses + tangentEffectiveMassA + tangentEffectiveMassB);
				newTangentImpulse += oldTangentImpulse;

				float maxFriction = contactConstraint.friction * manifoldPoint.normalImpulse;
				newTangentImpulse = std::clamp(newTangentImpulse, -maxFriction, maxFriction);

				manifoldPoint.tangentImpulse = newTangentImpulse;

				float appliedTangentImpulse = newTangentImpulse - oldTangentImpulse;

				linearVelocityBufferA += contactConstraint.invMassA * appliedTangentImpulse * tangentVec;
				linearVelocityBufferB -= contactConstraint.invMassB * appliedTangentImpulse * tangentVec;

				angularVelocityBufferA +=
					XMVector3Transform(XMVector3Cross(rA, appliedTangentImpulse * tangentVec), invInertiaMatA);

				angularVelocityBufferB -=
					XMVector3Transform(XMVector3Cross(rB, appliedTangentImpulse * tangentVec), invInertiaMatB);
			}
		}

		linearVelocityA += linearVelocityBufferA;
		linearVelocityB += linearVelocityBufferB;
		angularVelocityA += angularVelocityBufferA;
		angularVelocityB += angularVelocityBufferB;

		XMStoreFloat3(&velocities_[idxA].linearVelocity, linearVelocityA);
		XMStoreFloat3(&velocities_[idxB].linearVelocity, linearVelocityB);
		XMStoreFloat3(&velocities_[idxA].angularVelocity, angularVelocityA);
		XMStoreFloat3(&velocities_[idxB].angularVelocity, angularVelocityB);

		velocities_[idxA].linearVelocityBuffer = XMFLOAT3(0.0f, 0.0f, 0.0f);
		velocities_[idxB].linearVelocityBuffer = XMFLOAT3(0.0f, 0.0f, 0.0f);
		velocities_[idxA].angularVelocityBuffer = XMFLOAT3(0.0f, 0.0f, 0.0f);
		velocities_[idxB].angularVelocityBuffer = XMFLOAT3(0.0f, 0.0f, 0.0f);
	}
}

void ContactSolver::solvePositionConstraints()
{
	const float kSlop = 0.001f; // 허용 관통 오차
	const float alpha = POSITION_SOLVE_ALPHA;

	for (int i = 0; i < numContacts_; ++i) {
		Contact* contact = contacts_[i];
		const ContactConstraint& contactConstraint = contactConstraints_[i];

		int32_t numPoints = contactConstraint.numPoints;
		int32_t idxA = contactConstraint.bodyIdA;
		int32_t idxB = contactConstraint.bodyIdB;
		
		float sumMass = contactConstraint.invMassA + contactConstraint.invMassB;
		float ratioA = contactConstraint.invMassA / sumMass;
		float ratioB = contactConstraint.invMassB / sumMass;

		XMVECTOR positionBufferA = XMLoadFloat3(&positions_[idxA].positionBuffer);
		XMVECTOR positionBufferB = XMLoadFloat3(&positions_[idxB].positionBuffer);
		for (uint32_t j = 0; j < numPoints; j++) {
			const ManifoldPoint& manifoldPoint = contactConstraint.points[j];

			XMVECTOR movedPointA = XMLoadFloat3(&manifoldPoint.pointA) + positionBufferA;
			XMVECTOR movedPointB = XMLoadFloat3(&manifoldPoint.pointB) + positionBufferB;
			
			XMVECTOR normalVec = XMLoadFloat3(&manifoldPoint.normal);
			float seperation = VecDot(normalVec, movedPointA - movedPointB);

			// 관통 해소된상태면 무시
			if (seperation < kSlop) {
				continue;
			}

			// 관통 깊이에 따른 보정량 계산 -> 겹침을 해소하는 방향으로 위치 이동
			float correction = seperation * (alpha / numPoints);
			XMVECTOR correctionVector = correction * normalVec;

			positionBufferA -= correctionVector * ratioA;
			positionBufferB += correctionVector * ratioB;
		}
		XMStoreFloat3(&positions_[idxA].positionBuffer, positionBufferA);
		XMStoreFloat3(&positions_[idxB].positionBuffer, positionBufferB);
	}
}

void ContactSolver::checkSleepContact()
{
	return;
}


}

