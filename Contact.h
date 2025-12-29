#pragma once
#include "Rigidbody.h"
#include "Fixture.h"
#include "Collision.h"

//  두 Fixture의 충돌을 처리함.

namespace spe {;

class Contact;

extern const float EPS_FLOAT;
extern const float EPS_FLOAT_SQ;
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
	int32_t numSupports = 0;

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
	float distance;
};

struct Face
{
	XMFLOAT3 normal; // 정규화 해야함
	XMFLOAT3 points[MAX_MANIFOLD_COUNT];
	uint32_t numPoints = 0;
	float distance;
};

struct ContactFace
{
	XMFLOAT3 points[MAX_MANIFOLD_COUNT];
	XMFLOAT3 buffer[MAX_MANIFOLD_COUNT];
	uint32_t numPoints = 0;
};

struct CollisionInfo
{
	XMFLOAT3 normal[MAX_MANIFOLD_COUNT];
	XMFLOAT3 pointA[MAX_MANIFOLD_COUNT];
	XMFLOAT3 pointB[MAX_MANIFOLD_COUNT];
	float separation[MAX_MANIFOLD_COUNT];
	int32_t size;
};

struct ContactLink
{
	Rigidbody* other;  // 연결된 반대쪽 Body
	Contact* contact;  // 두 Body 간의 Contact 정보
	ContactLink* prev; // 이전 충돌 정보
	ContactLink* next; // 다음 충돌 정보

	ContactLink() : 
		other(nullptr), contact(nullptr), prev(nullptr), next(nullptr) {}
};

enum class ContactFlag : uint32_t
{
	ISLAND = (1 << 0),
	TOUCHING = (1 << 1),
	SENSOR = (1 << 2),
};

class Contact
{
public:
	static Contact* Create(Fixture* fixtureA, Fixture* fixtureB);

	Contact(Fixture* fixtureA, Fixture* fixtureB);
	void Update();
	virtual void Evaluate(Manifold& manifold, const XMMATRIX& transformA, const XMMATRIX& transformB);

	void GenerateManifolds(CollisionInfo& collisionInfo, Manifold& manifold, Fixture* fixtureA, Fixture* fixtureB);

	bool HasFlag(ContactFlag flag) const;
	float GetFriction() const;
	float GetRestitution() const;
	int32_t GetFaceNormals(const Polytope& polytope, FaceArray& faceArray) const;
	SupportPoint GetSupportPoint(const ConvexInfo& convexA, const ConvexInfo& convexB, const XMVECTOR& dir) const;
	Fixture* GetFixtureA() const;
	Fixture* GetFixtureB() const;
	Manifold& GetManifold();
	ResultEPA GetEPA(Polytope& simplex, const ConvexInfo& convexA, const ConvexInfo& convexB) const;
	ContactLink* GetContactLinkA();
	ContactLink* GetContactLinkB();
	Contact* GetNext();
	Contact* GetPrev();

	void SetFlag(ContactFlag flag);
	void ClearFlag(ContactFlag flag);	// 해당 flag만 끄기
	void SetNext(Contact* contact);
	void SetPrev(Contact* contact);

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
	void AddIfUniqueEdge(vector<pair<uint32_t, uint32_t>>& uniqueEdges, const uint32_t* faces, uint32_t a, uint32_t b) const;

	virtual void FindCollisionPoints(const ConvexInfo& convexA, const ConvexInfo& convexB, CollisionInfo& collisionInfo,
		ResultEPA& resultEPA, Polytope* simplexArray) = 0;
	bool IsDuplicatedPoint(const Polytope& polytope, const XMVECTOR& supportPoint)const;

	void ComputeContactPolygon(ContactFace& contactFace, Face& refFace, Face& incFace);
	void ClipPolygonAgainstPlane(ContactFace& contactFace, const XMVECTOR& planeNormal, float planeDist);

	void BuildManifoldFromPolygon(CollisionInfo& collisionInfo, const Face& refFace, const Face& incFace,
		ContactFace& contactFace, ResultEPA& resultEPA);
	void SortVerticesClockwise(XMFLOAT3* vertices, const XMVECTOR& center, const XMVECTOR& normal,
		uint32_t verticesSize);

	void SetBoxFace(Face& face, const ConvexInfo& box, const XMVECTOR& normal);
	//void setCylinderFace(Face& face, const ConvexInfo& cylinder, const XMFLOAT3& normal);
	//void setCapsuleFace(Face& face, const ConvexInfo& capsule, const XMFLOAT3& normal);

	//bool isCollideToHemisphere(const ConvexInfo& capsule, const XMFLOAT3& dir);
	void MergeFaceArray(FaceArray& faceArray, FaceArray& newFaceArray) const;
	void SizeUpFaceArray(FaceArray& faceArray, uint32_t newMaxCount) const;
	void FreeConvexInfo(ConvexInfo& convexA, ConvexInfo& convexB) const;

	float friction_;
	float restitution_;

	Fixture* fixtureA_ = nullptr;
	Fixture* fixtureB_ = nullptr;

	Manifold manifold_;

	uint32_t flags_ = 0;

	Contact* next_ = nullptr;
	Contact* prev_ = nullptr;

	ContactLink linkA;
	ContactLink linkB;
};

}	// namespace spe

