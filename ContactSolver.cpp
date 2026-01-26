#include "ContactSolver.h"


namespace spe {;

const float ContactSolver::NORMAL_STOP_VELOCITY = 0.01f;
const float ContactSolver::TANGENT_STOP_VELOCITY = 0.01f;
const float ContactSolver::NORMAL_SLEEP_VELOCITY = 0.01f;
const float ContactSolver::TANGENT_SLEEP_VELOCITY = 0.01f;
const float ContactSolver::POSITION_SOLVE_ALPHA = 0.25f;

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
	for (uint32_t i = 0; i < numContacts_; i++) {
		const ContactConstraint& contactConstraint = contactConstraints_[i];
		uint32_t numPoints = contactConstraint.numPoints;
		int32_t idxA = contactConstraint.bodyIdA;
		int32_t idxB = contactConstraint.bodyIdB;

		XMVECTOR positionA = XMLoadFloat3(&positions_[idxA].position);
		XMVECTOR positionB = XMLoadFloat3(&positions_[idxB].position);

		XMVECTOR linearVelocityA = XMLoadFloat3(&velocities_[idxA].linearVelocity) + XMLoadFloat3(&velocities_[idxA].linearVelocityBuffer);
		XMVECTOR linearVelocityB = XMLoadFloat3(&velocities_[idxB].linearVelocity) + XMLoadFloat3(&velocities_[idxB].linearVelocityBuffer);
		XMVECTOR angularVelocityA = XMLoadFloat3(&velocities_[idxA].angularVelocity) + XMLoadFloat3(&velocities_[idxA].angularVelocityBuffer);
		XMVECTOR angularVelocityB = XMLoadFloat3(&velocities_[idxB].angularVelocity) + XMLoadFloat3(&velocities_[idxB].angularVelocityBuffer);

		for (uint32_t j = 0; j < numPoints; ++j) {
			ManifoldPoint& manifoldPoint = contactConstraint.points[j];

			XMVECTOR rA = XMLoadFloat3(&manifoldPoint.pointA) - XMLoadFloat3(&contactConstraint.worldCenterA);
			XMVECTOR rB = XMLoadFloat3(&manifoldPoint.pointB) - XMLoadFloat3(&contactConstraint.worldCenterB);

			XMVECTOR velocityA = linearVelocityA + XMVector3Cross(angularVelocityA, rA);
			XMVECTOR velocityB = linearVelocityB + XMVector3Cross(angularVelocityB, rB);
			XMVECTOR relativeVelocity = velocityB - velocityA;

			XMVECTOR normalVec = XMLoadFloat3(&manifoldPoint.normal);
			float normalSpeed = VecDot(relativeVelocity, normalVec);

			// 1. Normal Impulse
			float inverseMasses = (contactConstraint.invMassA + contactConstraint.invMassB);
			XMMATRIX invInertiaMatA = XMLoadFloat3x3(&contactConstraint.invInertiaA);
			XMMATRIX invInertiaMatB = XMLoadFloat3x3(&contactConstraint.invInertiaB);
			XMVECTOR torqueArmA = XMVector3Cross(rA, normalVec);
			XMVECTOR torqueArmB = XMVector3Cross(rB, normalVec);

			float normalEffectiveMassA = VecDot(torqueArmA, XMVector3Transform(torqueArmA, invInertiaMatA));
			float normalEffectiveMassB = VecDot(torqueArmB, XMVector3Transform(torqueArmB, invInertiaMatB));
			float kNormal = inverseMasses + normalEffectiveMassA + normalEffectiveMassB;

			if (kNormal > 0.0f) {
				float restitution = contactConstraint.restitution;
				if (normalSpeed > 0.0f) restitution = 0.0f;

				float rhs = -(1.0f + restitution) * normalSpeed;
				float lambda = rhs / kNormal;

				float oldNormalImpulse = manifoldPoint.normalImpulse;
				float newNormalImpulse = std::max(0.0f, oldNormalImpulse + lambda);
				lambda = newNormalImpulse - oldNormalImpulse;

				manifoldPoint.normalImpulse = newNormalImpulse;

				XMVECTOR impulseVec = lambda * normalVec;
				linearVelocityA -= contactConstraint.invMassA * impulseVec;
				linearVelocityB += contactConstraint.invMassB * impulseVec;
				angularVelocityA -= XMVector3Transform(XMVector3Cross(rA, impulseVec), invInertiaMatA);
				angularVelocityB += XMVector3Transform(XMVector3Cross(rB, impulseVec), invInertiaMatB);
			}

			// 2. Tangent Impulse
			velocityA = linearVelocityA + XMVector3Cross(angularVelocityA, rA);
			velocityB = linearVelocityB + XMVector3Cross(angularVelocityB, rB);
			relativeVelocity = velocityB - velocityA;

			float currentNormalSpeed = VecDot(relativeVelocity, normalVec);
			XMVECTOR tangentVelocity = relativeVelocity - (currentNormalSpeed * normalVec);
			float tangentSpeed = XMVectorGetX(XMVector3Length(tangentVelocity));

			if (tangentSpeed > TANGENT_STOP_VELOCITY) {
				XMVECTOR tangentVec = tangentVelocity / tangentSpeed;

				XMVECTOR torqueArmAT = XMVector3Cross(rA, tangentVec);
				XMVECTOR torqueArmBT = XMVector3Cross(rB, tangentVec);

				float tangentEffectiveMassA = VecDot(torqueArmAT, XMVector3Transform(torqueArmAT, invInertiaMatA));
				float tangentEffectiveMassB = VecDot(torqueArmBT, XMVector3Transform(torqueArmBT, invInertiaMatB));

				float kTangent = inverseMasses + tangentEffectiveMassA + tangentEffectiveMassB;

				if (kTangent > 0.0f) {
					float lambdaT = -tangentSpeed / kTangent;
					float maxFriction = contactConstraint.friction * manifoldPoint.normalImpulse;
					float oldTangentImpulse = manifoldPoint.tangentImpulse;
					float newTangentImpulse = std::clamp(oldTangentImpulse + lambdaT, -maxFriction, maxFriction);
					lambdaT = newTangentImpulse - oldTangentImpulse;

					manifoldPoint.tangentImpulse = newTangentImpulse;

					XMVECTOR impulseVecT = lambdaT * tangentVec;
					linearVelocityA -= contactConstraint.invMassA * impulseVecT;
					linearVelocityB += contactConstraint.invMassB * impulseVecT;
					angularVelocityA -= XMVector3Transform(XMVector3Cross(rA, impulseVecT), invInertiaMatA);
					angularVelocityB += XMVector3Transform(XMVector3Cross(rB, impulseVecT), invInertiaMatB);
				}
			}
		}

		XMStoreFloat3(&velocities_[idxA].linearVelocityBuffer, linearVelocityA - XMLoadFloat3(&velocities_[idxA].linearVelocity));
		XMStoreFloat3(&velocities_[idxB].linearVelocityBuffer, linearVelocityB - XMLoadFloat3(&velocities_[idxB].linearVelocity));
		XMStoreFloat3(&velocities_[idxA].angularVelocityBuffer, angularVelocityA - XMLoadFloat3(&velocities_[idxA].angularVelocity));
		XMStoreFloat3(&velocities_[idxB].angularVelocityBuffer, angularVelocityB - XMLoadFloat3(&velocities_[idxB].angularVelocity));
	}
}

void ContactSolver::SolvePositionConstraints()
{
	const float kSlop = 0.01f;
	const float alpha = POSITION_SOLVE_ALPHA;

	for (int i = 0; i < numContacts_; ++i) {
		const ContactConstraint& contactConstraint = contactConstraints_[i];

		int32_t numPoints = contactConstraint.numPoints;
		int32_t idxA = contactConstraint.bodyIdA;
		int32_t idxB = contactConstraint.bodyIdB;

		float sumMass = contactConstraint.invMassA + contactConstraint.invMassB;
		if (sumMass == 0.0f) continue;

		float ratioA = contactConstraint.invMassA / sumMass;
		float ratioB = contactConstraint.invMassB / sumMass;

		XMVECTOR positionBufferA = XMLoadFloat3(&positions_[idxA].positionBuffer);
		XMVECTOR positionBufferB = XMLoadFloat3(&positions_[idxB].positionBuffer);

		for (uint32_t j = 0; j < numPoints; j++) {
			const ManifoldPoint& manifoldPoint = contactConstraint.points[j];

			XMVECTOR movedPointA = XMLoadFloat3(&manifoldPoint.pointA) + positionBufferA;
			XMVECTOR movedPointB = XMLoadFloat3(&manifoldPoint.pointB) + positionBufferB;
			XMVECTOR normalVec = XMLoadFloat3(&manifoldPoint.normal); // Normal points A -> B

			// [수정 핵심] B - A 순서로 계산해야 올바른 Signed Distance(침투시 음수)가 나옵니다.
			float separation = VecDot(normalVec, movedPointB - movedPointA);

			// separation이 -kSlop보다 작으면(즉, 깊게 침투했으면) 보정
			if (separation >= -kSlop) {
				continue;
			}

			// 침투 깊이(음수) * alpha = 보정량(양수 스칼라)
			float correction = -separation * alpha;
			XMVECTOR correctionVector = correction * normalVec;

			// A는 Normal 반대 방향(-), B는 Normal 방향(+)으로 밀어냄
			positionBufferA -= correctionVector * ratioA;
			positionBufferB += correctionVector * ratioB;
		}
		XMStoreFloat3(&positions_[idxA].positionBuffer, positionBufferA);
		XMStoreFloat3(&positions_[idxB].positionBuffer, positionBufferB);
	}
}

void ContactSolver::CheckSleepContact()
{
	for (uint32_t i = 0; i < numContacts_; i++) {
		Contact* contact = contacts_[i];
		const ContactConstraint& contactConstraint = contactConstraints_[i];
		uint32_t pointCount = contactConstraint.numPoints;
		int32_t indexA = contactConstraint.bodyIdA;
		int32_t indexB = contactConstraint.bodyIdB;

		XMVECTOR linearVelocityA = XMLoadFloat3(&velocities_[indexA].linearVelocity);
		XMVECTOR linearVelocityB = XMLoadFloat3(&velocities_[indexB].linearVelocity);
		XMVECTOR angularVelocityA = XMLoadFloat3(&velocities_[indexA].angularVelocity);
		XMVECTOR angularVelocityB = XMLoadFloat3(&velocities_[indexB].angularVelocity);
		XMVECTOR upVector = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

		for (int32_t j = 0; j < pointCount; ++j) {
			const ManifoldPoint& manifoldPoint = contactConstraint.points[j];

			XMVECTOR relativeVelocity = linearVelocityA - linearVelocityB;

			if (Vec3LengthSq(relativeVelocity) > NORMAL_SLEEP_VELOCITY) {
				positions_[indexA].isNormalStop = false;
				positions_[indexB].isNormalStop = false;
			}

			XMVECTOR relativeAngularVelocity = angularVelocityA - angularVelocityB;
			if (Vec3LengthSq(relativeAngularVelocity) > TANGENT_SLEEP_VELOCITY) {
				positions_[indexA].isTangentStop = false;
				positions_[indexB].isTangentStop = false;
			}

			float normalDotUpVector = VecDot(XMLoadFloat3(&manifoldPoint.normal), upVector);
			if (normalDotUpVector < -0.3f) {
				positions_[indexA].isNormal = true;
			}
			if (normalDotUpVector > 0.3f) {
				positions_[indexB].isNormal = true;
			}
		}
	}
}

}
