#include "Contact.h"
#include "SphereToSphereContact.h"
#include "SphereToBoxContact.h"
#include "BoxToBoxContact.h"

namespace spe {;

const float EPS_FLOAT = 1e-6f;

const ContactMemberFunction Contact::createContactFunctions_[32] = {
    nullptr,							// 0
    &SphereToSphereContact::Create,		// 01
    &BoxToBoxContact::Create,			// 10
    &SphereToBoxContact::Create,		// 11
    &BoxToBoxContact::Create,			// 100
    &SphereToBoxContact::Create,		// 101
    &BoxToBoxContact::Create,			// 110
    nullptr,							// 111
    nullptr,//&CylinderToCylinderContact::create, // 1000
    nullptr,//&SphereToCylinderContact::create,	// 1001
    nullptr,//&BoxToCylinderContact::create,		// 1010
    nullptr,							// 1011
    nullptr,//&BoxToCylinderContact::create,		// 1100
    nullptr,							// 1101
    nullptr,							// 1110
    nullptr,							// 1111
    nullptr,//&CapsuleToCapsuleContact::create,	// 10000
    nullptr,//&SphereToCapsuleContact::create,	// 10001
    nullptr,//&BoxToCapsuleContact::create,		// 10010
    nullptr,							// 10011
    nullptr,//&BoxToCapsuleContact::create,		// 10100
    nullptr,							// 10101
    nullptr,							// 10110
    nullptr,							// 10111
    nullptr,//&CylinderToCapsuleContact::create,	// 11000
    nullptr,							// 11001
    nullptr,							// 11010
    nullptr,							// 11011
    nullptr,							// 11100
    nullptr,							// 11101
    nullptr,							// 11110
    nullptr,							// 11111
};

Contact* Contact::Create(Fixture* fixtureA, Fixture* fixtureB)
{
    // 각 fixture의 shape의 type 가져오기
    ShapeType typeA = fixtureA->GetShapeType();
    ShapeType typeB = fixtureB->GetShapeType();

    // SPHERE = (1 << 0)
    // BOX = (1 << 1)
    // GROUND = (1 << 2)
    // CYLINDER = (1 << 3)
    // CAPSULE = (1 << 4)
    // 더 작은 타입이 fixtureA가 된다
    if (typeA > typeB) {
        swap(fixtureA, fixtureB);
    }
        
    return createContactFunctions_[typeA | typeB](fixtureA, fixtureB);
}

Contact::Contact(Fixture* fixtureA, Fixture* fixtureB) :
    fixtureA_(fixtureA), fixtureB_(fixtureB)
{
    friction_ = std::sqrt(fixtureA_->GetFriction() * fixtureB_->GetFriction());
    restitution_ = std::max(fixtureA_->GetRestitution(), fixtureB_->GetRestitution());
}

void Contact::Update()
{
}

void Contact::Evaluate(Manifold& manifold, const XMMATRIX& transformA, const XMMATRIX& transformB)
{
    Shape* shapeA = fixtureA_->GetShape();
    Shape* shapeB = fixtureB_->GetShape();

    ConvexInfo convexA = shapeA->GetConvexInfo(transformA);
    ConvexInfo convexB = shapeB->GetConvexInfo(transformB);

    Simplex simplex;
    CollisionInfo collisionInfo;
    collisionInfo.size = 0;

    // std::cout << "GJK start\n";
    bool isCollide = GetGJK(simplex, convexA, convexB);

    if (isCollide) {
        // std::cout << "EPA start\n";
        ResultEPA epaInfo = GetEPA(simplex, convexA, convexB);

        if (epaInfo.dist == -1.0f) {
            //freeConvexInfo(convexA, convexB);
            return;
        }

        // std::cout << "CLIPPING start\n";
        findCollisionPoints(convexA, convexB, collisionInfo, epaInfo, simplex);

        // std::cout << "createManifold start\n";
        generateManifolds(collisionInfo, manifold, fixtureA_, fixtureB_);
    }

    //freeConvexInfo(convexA, convexB);
}

void Contact::generateManifolds(CollisionInfo& collisionInfo, Manifold& manifold, Fixture* fixtureA, Fixture* fixtureB)
{
    int32_t collisionInfoSize = collisionInfo.size;
    manifold.pointsCount = collisionInfoSize;

    for (int32_t i = 0; i < collisionInfoSize; ++i) {
        manifold.points[i].pointA = collisionInfo.pointA[i];
        manifold.points[i].pointB = collisionInfo.pointB[i];
        manifold.points[i].normal = collisionInfo.normal[i];
        manifold.points[i].seperation = collisionInfo.seperation[i];
    }
}

float Contact::GetFriction() const
{
    return friction_;
}

float Contact::GetRestitution() const
{
    return restitution_;
}

std::tuple<std::vector<XMVECTOR>, std::vector<float>, uint32_t> Contact::GetFaceNormals(const std::vector<SupportPoint>& polytope, const std::vector<uint32_t>& faces)
{
    std::vector<XMVECTOR> normals;
    std::vector<float> distances;
    uint32_t minTriangle = 0;
    float  minDistance = FLT_MAX;

    size_t numFaces = faces.size() / 3;
    normals.reserve(numFaces);
    distances.reserve(numFaces);

    for (size_t i = 0; i < faces.size(); i += 3) {
        XMVECTOR a = polytope[faces[i]].diff;
        XMVECTOR b = polytope[faces[i + 1]].diff;
        XMVECTOR c = polytope[faces[i + 2]].diff;

        XMVECTOR normal = XMVector3Normalize(XMVector3Cross(b - a, c - a));
        float distance = VecDot(normal, a);

        if (distance < 0) {
            normal *= -1;
            distance *= -1;
        }

        normals.emplace_back(normal);
        distances.emplace_back(distance);

        if (distance < minDistance) {
            minTriangle = i / 3;
            minDistance = distance;
        }
    }

    return { normals, distances, minTriangle };
}

SupportPoint Contact::GetSupportPoint(const ConvexInfo& convexA, const ConvexInfo& convexB, XMVECTOR& dir)
{
    SupportPoint support;
    support.a = convexA.GetFarthestPoint(dir);
    support.b = convexB.GetFarthestPoint(-dir);
    support.diff = XMLoadFloat3(&support.a) - XMLoadFloat3(&support.b);
    return support;
}

Fixture* Contact::GetFixtureA() const
{
    return fixtureA_;
}

Fixture* Contact::GetFixtureB() const
{
    return fixtureB_;
}

Manifold& Contact::GetManifold()
{
    return manifold_;
}

ResultEPA Contact::GetEPA(Simplex& simplex, const ConvexInfo& convexA, const ConvexInfo& convexB)
{
    // simplex는 항상 4개의 점을 가진 원점을 내부에 포함한 사면체가 들어와야한다.
    std::vector<SupportPoint> polytope(simplex.begin(), simplex.end());
    std::vector<uint32_t> faces = {
        0, 1, 2,
        0, 3, 1,
        0, 2, 3,
        1, 3, 2
    };

    // list: [vector<XMVECTOR>normal, vector<XMVECTOR>distance, uint32_t index: min distance]
    auto [normals, distances, minFace] = GetFaceNormals(polytope, faces);

    XMVECTOR  minNormal = XMVectorZero();
    float minDistance = FLT_MAX;

    while (minDistance == FLT_MAX) {
        // 현재 면중 원점과 가장 가까운 면의 normal을 이용해 해당 방향으로 supportPoint를 구한뒤 polytope를 확장한다.
        minNormal = normals[minFace];
        minDistance = distances[minFace];

        SupportPoint support = GetSupportPoint(convexA, convexB, minNormal);
        float sDistance = VecDot(minNormal, support.diff);

        // XMFLOAT3 f3;
        // XMStoreFloat3(&f3, support.diff);
        // std::cout << "(" << f3.x << ", " << f3.y << ", " << f3.z << ")\n";
        // std::cout << "minDistance: " << minDistance << ", distance: " << sDistance << "\n";
        
        // 새로구한 support의 거리가 polytope의 면 최소거리보다 짧거나 같다면 루프를 종료한다.
        if (sDistance > minDistance) {
            minDistance = FLT_MAX;

            std::vector<std::pair<uint32_t, uint32_t>> uniqueEdges;

            // support에서 보이는 모든면을 삭제한다.
            for (size_t i = 0; i < normals.size(); i++) {
                if (SameDirection(normals[i], support.diff - polytope[faces[i * 3]].diff)) {
                    uint32_t f = i * 3;

                    AddIfUniqueEdge(uniqueEdges, faces, f, f + 1);
                    AddIfUniqueEdge(uniqueEdges, faces, f + 1, f + 2);
                    AddIfUniqueEdge(uniqueEdges, faces, f + 2, f);

                    faces[f + 2] = faces.back(); faces.pop_back();
                    faces[f + 1] = faces.back(); faces.pop_back();
                    faces[f] = faces.back(); faces.pop_back();

                    normals[i] = normals.back(); // pop-erase
                    normals.pop_back();

                    distances[i] = distances.back(); // pop-erase
                    distances.pop_back();

                    i--;
                }
            }

            std::vector<uint32_t> newFaces;
            for (auto [edgeIndex1, edgeIndex2] : uniqueEdges) {
                newFaces.push_back(edgeIndex1);
                newFaces.push_back(edgeIndex2);
                newFaces.push_back(polytope.size());
            }

            polytope.push_back(support);

            auto [newNormals, newDistances, newMinFace] = GetFaceNormals(polytope, newFaces);

            float oldMinDistance = FLT_MAX;
            for (size_t i = 0; i < normals.size(); i++) {
                if (distances[i] < oldMinDistance) {
                    oldMinDistance = distances[i];
                    minFace = i;
                }
            }

            if (newDistances[newMinFace] < oldMinDistance) {
                minFace = newMinFace + normals.size();
            }

            faces.insert(faces.end(), newFaces.begin(), newFaces.end());
            distances.insert(distances.end(), newDistances.begin(), newDistances.end());
            normals.insert(normals.end(), newNormals.begin(), newNormals.end());
        }
    }

    ResultEPA result;
    result.normal = minNormal;
    result.dist = minDistance + EPS_FLOAT;

    return result;
}

bool Contact::LineSimplex(Simplex& simplex, XMVECTOR& dir)
{
    SupportPoint a = simplex[0];
    SupportPoint b = simplex[1];

    XMVECTOR ab = b.diff - a.diff;
    XMVECTOR ao = -a.diff;

    if (SameDirection(ab, ao)) {
        dir = XMVector3Cross(XMVector3Cross(ab, ao), ab);
    }
    else {
        simplex = { a };
        dir = ao;
    }

    return false;
}

bool Contact::TriangleSimplex(Simplex& simplex, XMVECTOR& dir)
{
    SupportPoint a = simplex[0];
    SupportPoint b = simplex[1];
    SupportPoint c = simplex[2];

    XMVECTOR ab = b.diff - a.diff;
    XMVECTOR ac = c.diff - a.diff;
    XMVECTOR ao = -a.diff;

    XMVECTOR abc = XMVector3Cross(ab, ac);
    // 원점이 선분 bc밖에 있는 경우는 Line단계에서 걸러진다.

    // 원점이 선분 ac밖에 존재
    if (SameDirection(XMVector3Cross(abc, ac), ao)) {
        // b 버리기
        if (SameDirection(ac, ao)) {
            simplex = { a, c };
            dir = XMVector3Cross(XMVector3Cross(ac, ao), ac);
        }
        // c 버리기
        else {
            return LineSimplex(simplex = { a, b }, dir);
        }
    }
    else {
        // 원점이 선분 ab밖에 존재
        if (SameDirection(XMVector3Cross(ab, abc), ao)) {
            return LineSimplex(simplex = { a, b }, dir);
        }
        // 원점이 삼각형 abc안에 존재
        else {
            // 원점이 삼각형 abc 위에 존재하는지 아래 존재하는지에 따라 다음 support를 구할 direction의 방향을 결정한다.
            if (SameDirection(abc, ao)) {
                dir = abc;
            }
            else {
                simplex = { a, c, b };
                dir = -abc;
            }
        }
    }

    return false;
}

bool Contact::TetrahedronSimplex(Simplex& simplex, XMVECTOR& dir)
{
    SupportPoint a = simplex[0];
    SupportPoint b = simplex[1];
    SupportPoint c = simplex[2];
    SupportPoint d = simplex[3];

    XMVECTOR ab = b.diff - a.diff;
    XMVECTOR ac = c.diff - a.diff;
    XMVECTOR ad = d.diff - a.diff;
    XMVECTOR ao = -a.diff;

    XMVECTOR abc = XMVector3Cross(ab, ac);
    XMVECTOR acd = XMVector3Cross(ac, ad);
    XMVECTOR adb = XMVector3Cross(ad, ab);
    // 원점이 삼각형 bcd 밖에 존재하는 경우는 Triangle단계에서 걸러진다.

    // 원점이 삼각형 abc 밖에 존재함 -> d를 다시 선정한다.
    if (SameDirection(abc, ao)) {
        return TriangleSimplex(simplex = { a, b, c }, dir);
    }

    // 원점이 삼각형 acd 밖에 존재함 -> b를 다시 선정한다.
    if (SameDirection(acd, ao)) {
        return TriangleSimplex(simplex = { a, c, d }, dir);
    }

    // 원점이 삼각형 adb 밖에 존재함 -> c를 다시 선정한다.
    if (SameDirection(adb, ao)) {
        return TriangleSimplex(simplex = { a, d, b }, dir);
    }

    // 원점이 사면체 abcd안에 존재한다.
    return true;
}

bool Contact::NextSimplex(Simplex& simplex, XMVECTOR& dir)
{
    switch (simplex.size()) {
    case 2: return LineSimplex(simplex, dir);
    case 3: return TriangleSimplex(simplex, dir);
        // 3차원 충돌만 계산하므로 Tetrahedron에서만 true를 반환할 수 있다.
    case 4: return TetrahedronSimplex(simplex, dir);
    }

    // never should be here
    return false;
}

bool Contact::GetGJK(Simplex& simplex, const ConvexInfo& convexA, const ConvexInfo& convexB)
{
    // simplex의 첫점을 아무 방향이나 넣어서 구함
    SupportPoint support = GetSupportPoint(convexA, convexB, XMVectorSet(1.f, 0.f, 0.f, 0.f));

    simplex.push_front(support);

    // New direction is towards the origin
    XMVECTOR direction = -support.diff;

    while (true) {
        support = GetSupportPoint(convexA, convexB, direction);

        // support와 direction의 내적 값이 0보다 작으면 두점 사이에 원점이 포함 되지 않는다. 
        if (VecDot(support.diff, direction) < EPS_FLOAT) {
            return false; // no collision
        }

        simplex.push_front(support);

        if (NextSimplex(simplex, direction)) {
            return true;
        }
    }

    return false;
}

bool Contact::SameDirection(const XMVECTOR& direction, const XMVECTOR& ao) const
{
    return VecDot(direction, ao) > 0.f;
}

void Contact::AddIfUniqueEdge(std::vector<std::pair<uint32_t, uint32_t>>& edges, const std::vector<uint32_t>& faces, uint32_t a, uint32_t b)
{
    auto reverse = std::find(                       //      0--<--3
        edges.begin(),                              //     / \ B /   A: 2-0
        edges.end(),                                //    / A \ /    B: 0-2
        std::make_pair(faces[b], faces[a])          //   1-->--2
    );

    if (reverse != edges.end()) {
        edges.erase(reverse);
    }

    else {
        edges.emplace_back(faces[a], faces[b]);
    }
}

} // naemspace spe