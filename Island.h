#pragma once

#include "Contact.h"

namespace spe {;

// contact solve중 변하는 position값을 buffer에 저장한 뒤 sovle가 모두 완료된 뒤 적용한다.
struct PositionBuffer
{
	XMFLOAT3 position;
	XMFLOAT3 positionBuffer;
	bool isNormalStop = true;
	bool isTangentStop = true;
	bool isSupported = false; // 해당 물체가 바닥으로부터 지지받고 있는가?
};

// contact solve중 변하는 velocity값을 buffer에 저장한 뒤 sovle가 모두 완료된 뒤 적용한다.
struct VelocityBuffer
{
	XMFLOAT3 linearVelocity;
	XMFLOAT3 angularVelocity;
	XMFLOAT3 linearVelocityBuffer;
	XMFLOAT3 angularVelocityBuffer;
};

// 서로 영향을 미칠 수 있는 contact들을 모아 충돌을 해결한다.
class Island
{
public:
	Island(uint32_t bodyCount, uint32_t contactCount);
	void Solve(float duration);
	void Destroy();

	void Add(Rigidbody* body);
	void Add(Contact* contact);
	void Clear();

	uint32_t numBodies()const;
	Rigidbody** GetBodies();
private:
	static const uint32_t VELOCITY_ITERATION;
	static const uint32_t POSITION_ITERATION;
	static const float STOP_LINEAR_VELOCITY;
	static const float STOP_ANGULAR_VELOCITY;
	static const float STOP_LINEAR_VELOCITY_SQ;
	static const float STOP_ANGULAR_VELOCITY_SQ;
	
	Rigidbody** bodies_;
	Contact** contacts_;
	PositionBuffer* positions_;
	VelocityBuffer* velocities_;

	uint32_t numBodies_;
	uint32_t numContacts_;
};

} // namespace spe

