#pragma once

#include "Contact.h"
#include "Island.h"

namespace spe {;

struct ContactConstraint
{
	ManifoldPoint* points;
	int32_t numPoints;
	XMFLOAT3 worldCenterA, worldCenterB;
	XMFLOAT3X3 invInertiaA, invInertiaB;
	int32_t bodyIdA, bodyIdB;	// position buffer, velocity buffer 접근용 islandId
	float invMassA, invMassB;
	float friction;
	float restitution;

	~ContactConstraint() = default;
};

class ContactSolver
{
public:
	// contactSolver의 contacts는 모두 동일한 island에 속한다.
	ContactSolver(float duration, Contact** contacts, PositionBuffer* positions, VelocityBuffer* velocities, uint32_t numBodies, uint32_t numContacts);
	void destroy();

	void solveVelocityConstraints();
	void solvePositionConstraints();
	void checkSleepContact();

	static const float NORMAL_STOP_VELOCITY;
	static const float TANGENT_STOP_VELOCITY;
	static const float NORMAL_SLEEP_VELOCITY;
	static const float TANGENT_SLEEP_VELOCITY;
	static const float POSITION_SOLVE_ALPHA;

	int32_t numBodies_;
	int32_t numContacts_;
	float duration_;
	Contact** contacts_;
	PositionBuffer* positions_;
	VelocityBuffer* velocities_;
	ContactConstraint* contactConstraints_;
};

}



