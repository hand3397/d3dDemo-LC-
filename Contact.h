#pragma once
#include "Rigidbody.h"
#include "Fixture.h"
#include "Collision.h"

//  두 Fixture의 충돌을 처리함.

namespace spe {;

class Contact;

extern const float EPS_FLOAT;

using ContactMemberFunction = Contact * (*)(Fixture*, Fixture*);

struct SupportPoint
{
	XMFLOAT3 a;
	XMFLOAT3 b;
	XMVECTOR diff;
};

struct Simplex
{
public:
	Simplex() {}

	Simplex& operator=(std::initializer_list<SupportPoint> list)
	{
		size_ = 0;
		for (const SupportPoint& point : list)
			points_[size_++] = point;

		return *this;
	}

	void push_front(const SupportPoint& point)
	{
		points_ = { point, points_[0], points_[1], points_[2] };
		size_ = (size_ + 1 < 4 ? size_ + 1 : 4);
	}

	SupportPoint& operator[](int i) { return points_[i]; }
	size_t size() const { return size_; }

	auto begin() const { return points_.begin(); }
	auto end() const { return points_.end() - (4 - size_); }

private:
	std::array<SupportPoint, 4> points_;
	int size_ = 0;
};

struct ResultEPA
{
	XMVECTOR normal;
	float dist;
};

struct CollisionInfo
{
	XMFLOAT3 normal[MAX_MANIFOLD_COUNT];
	XMFLOAT3 pointA[MAX_MANIFOLD_COUNT];
	XMFLOAT3 pointB[MAX_MANIFOLD_COUNT];
	float seperation[MAX_MANIFOLD_COUNT];
	int32_t size;
};

class Contact
{
public:
	static Contact* Create(Fixture* fixtureA, Fixture* fixtureB);

	Contact(Fixture* fixtureA, Fixture* fixtureB);
	void Update();
	void Evaluate(Manifold& manifold, const XMMATRIX& transformA, const XMMATRIX& transformB);

	void generateManifolds(CollisionInfo& collisionInfo, Manifold& manifold, Fixture* fixtureA, Fixture* fixtureB);

	bool IsTouching()const;
	float GetFriction() const;
	float GetRestitution() const;
	std::tuple<std::vector<XMVECTOR>, std::vector<float>, uint32_t> GetFaceNormals(
		const std::vector<SupportPoint>& polytope,
		const std::vector<uint32_t>& faces);
	SupportPoint GetSupportPoint(const ConvexInfo& convexA, const ConvexInfo& convexB, XMVECTOR& dir);
	Fixture* GetFixtureA() const;
	Fixture* GetFixtureB() const;
	Manifold& GetManifold();

	ResultEPA GetEPA(Simplex& simplex, const ConvexInfo& convexA, const ConvexInfo& convexB);

protected:
	// contact를 이루는 두 fixture A, B의 shape type에 따라 다른 contact를 생성한다.
	// ex) A.type = sphere, B.type = box -> Create SphereToBoxContact : Contact
	const static ContactMemberFunction createContactFunctions_[32];

	bool LineSimplex(Simplex& simplex, XMVECTOR& dir);
	bool TriangleSimplex(Simplex& simplex, XMVECTOR& dir);
	bool TetrahedronSimplex(Simplex& simplex, XMVECTOR& dir);
	bool NextSimplex(Simplex& simplex, XMVECTOR& dir);

	bool GetGJK(Simplex& simplex, const ConvexInfo& convexA, const ConvexInfo& convexB);

	bool SameDirection(const XMVECTOR& direction, const XMVECTOR& ao) const;
	void AddIfUniqueEdge(std::vector<std::pair<uint32_t, uint32_t>>& edges,
		const std::vector<uint32_t>& faces, uint32_t a, uint32_t b);

	virtual void findCollisionPoints(const ConvexInfo& convexA, const ConvexInfo& convexB, CollisionInfo& collisionInfo,
		ResultEPA& resultEPA, Simplex& simplexArray) = 0;
	bool IsDuplicatedPoint(const vector<SupportPoint>& polytope, const XMVECTOR& supportPoint);

	/*
	void computeContactPolygon(ContactPolygon& contactPolygon, Face& refFace, Face& incFace);
	void clipPolygonAgainstPlane(ContactPolygon& contactPolygon, const XMFLOAT3& planeNormal, float planeDist);

	void buildManifoldFromPolygon(CollisionInfo& collisionInfo, const Face& refFace, const Face& incFace,
		ContactPolygon& contactPolygon, EpaInfo& epaInfo);
	void sortVerticesClockwise(XMFLOAT3* vertices, const XMFLOAT3& center, const XMFLOAT3& normal,
		int32_t verticesSize);

	void setBoxFace(Face& face, const ConvexInfo& box, const XMFLOAT3& normal);
	void setCylinderFace(Face& face, const ConvexInfo& cylinder, const XMFLOAT3& normal);
	void setCapsuleFace(Face& face, const ConvexInfo& capsule, const XMFLOAT3& normal);

	bool isCollideToHemisphere(const ConvexInfo& capsule, const XMFLOAT3& dir);

	void addFaceInFaceArray(FaceArray& faceArray, int32_t idx1, int32_t idx2, int32_t idx3);
	void mergeFaceArray(FaceArray& faceArray, FaceArray& newFaceArray);
	void sizeUpFaceArray(FaceArray& faceArray, int32_t newMaxCount);
	*/
	void freeConvexInfo(ConvexInfo& convexA, ConvexInfo& convexB);
	
	float friction_;
	float restitution_;

	Fixture* fixtureA_ = nullptr;
	Fixture* fixtureB_ = nullptr;

	Manifold manifold_;

	bool isTouching_ = false;
};

}	// namespace spe

