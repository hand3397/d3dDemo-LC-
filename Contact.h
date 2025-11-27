#pragma once
#include "Rigidbody.h"
#include "Fixture.h"
#include "Collision.h"

//  두 Fixture의 충돌을 처리함.

namespace spe {;

class Contact;

extern const float EPS_FLOAT;
const uint32_t MAX_NUM_SUPPORTS = 100;

using ContactMemberFunction = Contact * (*)(Fixture*, Fixture*);

struct SupportPoint
{
	XMFLOAT3 a;
	XMFLOAT3 b;
	XMVECTOR diff;
};

struct Polytope
{
	SupportPoint supports[MAX_NUM_SUPPORTS];
	uint32_t numSupports = 0;

	Polytope() : numSupports(0) {}

	Polytope(initializer_list<SupportPoint> list)
	{
		numSupports = list.size();
		std::copy(list.begin(), list.begin() + numSupports, supports);
	}
};

struct FaceArray
{
	uint32_t numFaces;
	uint32_t maxNumFaces;
	uint32_t* faces;
	XMFLOAT4* normals;

	FaceArray()
	{
		numFaces = 0;
		maxNumFaces = 64;

		faces = new uint32_t[maxNumFaces * 3];
		normals = new XMFLOAT4[maxNumFaces];
	}

	~FaceArray()
	{
		delete[] faces;
		delete[] normals;
	}
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

	void GenerateManifolds(CollisionInfo& collisionInfo, Manifold& manifold, Fixture* fixtureA, Fixture* fixtureB);

	bool IsTouching()const;
	float GetFriction() const;
	float GetRestitution() const;
	int32_t GetFaceNormals(const Polytope& polytope, FaceArray& faceArray);
	SupportPoint GetSupportPoint(const ConvexInfo& convexA, const ConvexInfo& convexB, XMVECTOR& dir);
	Fixture* GetFixtureA() const;
	Fixture* GetFixtureB() const;
	Manifold& GetManifold();

	ResultEPA GetEPA(Polytope& simplex, const ConvexInfo& convexA, const ConvexInfo& convexB);

protected:
	static const uint32_t MAX_GJK_ITERATION;

	// contact를 이루는 두 fixture A, B의 shape type에 따라 다른 contact를 생성한다.
	// ex) A.type = sphere, B.type = box -> Create SphereToBoxContact : Contact
	const static ContactMemberFunction createContactFunctions_[32];

	bool LineSimplex(Polytope& simplex, XMVECTOR& dir);
	bool TriangleSimplex(Polytope& simplex, XMVECTOR& dir);
	bool TetrahedronSimplex(Polytope& simplex, XMVECTOR& dir);
	bool NextSimplex(Polytope& simplex, XMVECTOR& dir);

	bool GetGJK(Polytope& simplex, const ConvexInfo& convexA, const ConvexInfo& convexB);

	bool IsSimilarDirection(const XMVECTOR& a, const XMVECTOR& b) const;
	bool IsSameDirection(const XMVECTOR& a, const XMVECTOR& b) const;
	void AddIfUniqueEdge(vector<pair<uint32_t, uint32_t>>& uniqueEdges, const uint32_t* faces, uint32_t a, uint32_t b);

	virtual void FindCollisionPoints(const ConvexInfo& convexA, const ConvexInfo& convexB, CollisionInfo& collisionInfo,
		ResultEPA& resultEPA, Polytope& simplexArray) = 0;
	bool IsDuplicatedPoint(const Polytope& polytope, const XMVECTOR& supportPoint)const;

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
	*/

	void MergeFaceArray(FaceArray& faceArray, FaceArray& newFaceArray);
	void SizeUpFaceArray(FaceArray& faceArray, uint32_t newMaxCount);
	void FreeConvexInfo(ConvexInfo& convexA, ConvexInfo& convexB);
	
	float friction_;
	float restitution_;

	Fixture* fixtureA_ = nullptr;
	Fixture* fixtureB_ = nullptr;

	Manifold manifold_;

	// flag
	bool isTouching_ = false;
};

}	// namespace spe

