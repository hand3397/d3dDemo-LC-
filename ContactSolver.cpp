#include "ContactSolver.h"


namespace spe {;

const float ContactSolver::NORMAL_STOP_VELOCITY = 0.01f;
const float ContactSolver::TANGENT_STOP_VELOCITY = 0.01f;
const float ContactSolver::NORMAL_SLEEP_VELOCITY = 1.0f;
const float ContactSolver::TANGENT_SLEEP_VELOCITY = 1.0f;
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

	// for (int32_t i = 0; i < m_bodyCount; ++i)
	// {

		// std::cout << "\n\nafter velocity!!!\n";
		// std::cout << "linear velocity: " << velocities_[i].linearVelocity.x << " " << velocities_[i].linearVelocity.y << " " << velocities_[i].linearVelocity.z << "\n";
		// std::cout << "angular velocity: " << velocities_[i].angularVelocity.x << " " << velocities_[i].angularVelocity.y << " " << velocities_[i].angularVelocity.z << "\n";
	// }
}

void ContactSolver::SolvePositionConstraints()
{
	for (uint32_t i = 0; i < numContacts_; ++i) {
		Contact* contact = contacts_[i];
		ContactConstraint& contactConstraint = contactConstraints_[i];

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

			if (Vec3LengthSq(relativeVelocity) > NORMAL_SLEEP_VELOCITY) {
				positions_[indexA].isNormalStop = false;
				positions_[indexB].isNormalStop = false;
			}

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
