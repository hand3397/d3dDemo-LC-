#include "Contact.h"
#include "SphereToSphereContact.h"
#include "SphereToBoxContact.h"
#include "BoxToBoxContact.h"

namespace spe {;

const float EPS_FLOAT = 1e-3f;

const uint32_t Contact::MAX_GJK_ITERATION = 64;

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
    Rigidbody* bodyA = fixtureA_->GetRigidbody();
    Rigidbody* bodyB = fixtureB_->GetRigidbody();
    XMMATRIX transformA = bodyA->GetTransformMatrix();
    XMMATRIX transformB = bodyB->GetTransformMatrix();

    // Evaluate
    manifold_.pointsCount = 0;
    Evaluate(manifold_, transformA, transformB);
    isTouching_ = manifold_.pointsCount > 0;

    for (uint32_t i = 0; i < manifold_.pointsCount; ++i) {
        ManifoldPoint& manifoldPoint = manifold_.points[i];

        manifoldPoint.normalImpulse = 0.0f;
        manifoldPoint.tangentImpulse = 0.0f;
    }
}

void Contact::Evaluate(Manifold& manifold, const XMMATRIX& transformA, const XMMATRIX& transformB)
{
    Shape* shapeA = fixtureA_->GetShape();
    Shape* shapeB = fixtureB_->GetShape();

    ConvexInfo convexA, convexB;
    shapeA->GetConvexInfo(transformA, convexA);
    shapeB->GetConvexInfo(transformB, convexB);

    Polytope simplex;
    CollisionInfo collisionInfo;
    collisionInfo.size = 0;

    // std::cout << "GJK start\n";
    bool isCollide = GetGJK(simplex, convexA, convexB);

    if (isCollide) {
        // std::cout << "EPA start\n";
        ResultEPA resultEPA = GetEPA(simplex, convexA, convexB);

        if (resultEPA.dist == -1.0f) {
            FreeConvexInfo(convexA, convexB);
            return;
        }

        // std::cout << "CLIPPING start\n";
        FindCollisionPoints(convexA, convexB, collisionInfo, resultEPA, simplex);

        // std::cout << "createManifold start\n";
        GenerateManifolds(collisionInfo, manifold, fixtureA_, fixtureB_);
    }

    FreeConvexInfo(convexA, convexB);
}

void Contact::GenerateManifolds(CollisionInfo& collisionInfo, Manifold& manifold, Fixture* fixtureA, Fixture* fixtureB)
{
    int32_t collisionInfoSize = collisionInfo.size;
    manifold.pointsCount = collisionInfoSize;

    for (int32_t i = 0; i < collisionInfoSize; ++i) {
        manifold.points[i].pointA = collisionInfo.pointA[i];
        manifold.points[i].pointB = collisionInfo.pointB[i];
        manifold.points[i].normal = collisionInfo.normal[i];
        manifold.points[i].seperation = collisionInfo.seperation[i] + EPS_FLOAT;
    }
}

bool Contact::IsTouching() const
{
    return isTouching_;
}

float Contact::GetFriction() const
{
    return friction_;
}

float Contact::GetRestitution() const
{
    return restitution_;
}

void Fixture::SetRigidbody(Rigidbody* rigidbody)
{
    rigidbody_ = rigidbody;
}

void Fixture::SetFriction(float friction)
{
    friction_ = friction;
}

void Fixture::SetRestitution(float restitution)
{
    restitution_ = restitution;
}

int32_t Contact::GetFaceNormals(const Polytope& polytope, FaceArray& faceArray)
{
    XMVECTOR center = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);

    uint32_t minFace = 0;
    float  minDistance = FLT_MAX;

    uint32_t numSupprots = polytope.numSupports;
    for (uint32_t i = 0; i < numSupprots; ++i)
        center += polytope.supports[i].diff;
    center /= static_cast<float>(polytope.numSupports);

    for (uint32_t i = 0; i < faceArray.numFaces; i++) {
        uint32_t supportIdx = i * 3;
        XMVECTOR a = polytope.supports[faceArray.faces[supportIdx]].diff;
        XMVECTOR b = polytope.supports[faceArray.faces[supportIdx + 1]].diff;
        XMVECTOR c = polytope.supports[faceArray.faces[supportIdx + 2]].diff;

        XMVECTOR normal = XMVector3Normalize(XMVector3Cross(b - a, c - a));

        // normal을 polytope 외부를 향하도록 한다.
        if (VecDot(normal, a - center) < 0)
            normal = -normal;
        float distance = VecDot(normal, a);
        
        // polytope안에 원점이 포함되어 있다면 distance가 음수가 나올 수 없음
        if (distance < 0)
            return -1;

        XMStoreFloat4(&faceArray.normals[i], normal);
        faceArray.normals[i].w = distance;

        if (distance < minDistance) {
            minFace = i;
            minDistance = distance;
        }
    }

    return minFace;
}

SupportPoint Contact::GetSupportPoint(const ConvexInfo& convexA, const ConvexInfo& convexB, XMVECTOR& dir)
{
    XMVECTOR ddir = XMVector3Normalize(dir);
    SupportPoint support;
    support.a = convexA.GetFarthestPoint(ddir);
    support.b = convexB.GetFarthestPoint(-ddir);
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

ResultEPA Contact::GetEPA(Polytope& polytope, const ConvexInfo& convexA, const ConvexInfo& convexB)
{
    // simplex(polytope)는 항상 4개의 점을 가진 원점을 내부에 포함한 사면체가 들어와야한다.
    uint32_t initFaces[12] = {
        0, 1, 2,
        0, 3, 1,
        0, 2, 3,
        1, 3, 2 };
    FaceArray faceArray;
    FaceArray newFaceArray;
    memcpy(faceArray.faces, initFaces, sizeof(uint32_t) * 12);
    faceArray.numFaces = 4;

    XMVECTOR minNormal = XMVectorZero();
    float minDistance = FLT_MAX;

    uint32_t minFace = GetFaceNormals(polytope, faceArray);

    if (minFace == -1) {
        minDistance = -1.0f;
    }

    while (minDistance == FLT_MAX) {
        // 현재 면중 원점과 가장 가까운 면의 normal을 이용해 해당 방향으로 supportPoint를 구한뒤 polytope를 확장한다.
        minNormal = XMVectorSetW(XMLoadFloat4(&faceArray.normals[minFace]), 0.0f);
        minDistance = faceArray.normals[minFace].w;

        SupportPoint support = GetSupportPoint(convexA, convexB, minNormal);
        float sDistance = VecDot(minNormal, support.diff);
        
        // 새로구한 support의 거리가 polytope의 면 최소거리보다 짧거나 같다면 루프를 종료한다.
        if (std::abs(minDistance - sDistance) > EPS_FLOAT && !IsDuplicatedPoint(polytope, support.diff)) {
            minDistance = FLT_MAX;

            std::vector<std::pair<uint32_t, uint32_t>> uniqueEdges;

            // support에서 보이는 모든면을 삭제한다.
            for (uint32_t i = 0; i < faceArray.numFaces; i++) {
                XMVECTOR normalVec = XMVectorSetW(XMLoadFloat4(&faceArray.normals[i]), 0.0f);
                if (IsSimilarDirection(normalVec, support.diff - polytope.supports[faceArray.faces[i * 3]].diff)) {
                    uint32_t faceIdx = i * 3;

                    // 해당 face에 포함되는 edge를 uniqueEdges에 등록한다.
                    // 1회 : polytope에 새로운 면을 만들 때 사용한다.
                    // 2회 : 제거한다.
                    AddIfUniqueEdge(uniqueEdges, faceArray.faces, faceIdx, faceIdx + 1);
                    AddIfUniqueEdge(uniqueEdges, faceArray.faces, faceIdx + 1, faceIdx + 2);
                    AddIfUniqueEdge(uniqueEdges, faceArray.faces, faceIdx + 2, faceIdx);

                    // face 제거
                    uint32_t lastNormalIdx = faceArray.numFaces - 1;
                    uint32_t lastFaceIdx = (lastNormalIdx) * 3;

                    faceArray.faces[faceIdx] = faceArray.faces[lastFaceIdx];
                    faceArray.faces[faceIdx + 1] = faceArray.faces[lastFaceIdx + 1];
                    faceArray.faces[faceIdx + 2] = faceArray.faces[lastFaceIdx + 2];
                    faceArray.normals[i] = faceArray.normals[lastNormalIdx];
                    --faceArray.numFaces;

                    --i;
                }
            }

            // 면 추가 없음
            if (uniqueEdges.empty()) {
                break;
            }

            newFaceArray.numFaces = 0;
            // uniqueEdges에는 잘린 polytope의 경계를 구성하는 edge만 담겨있다.
            for (auto [edgeIndex1, edgeIndex2] : uniqueEdges) {
                uint32_t pointIdx = newFaceArray.numFaces * 3;
                newFaceArray.faces[pointIdx] = edgeIndex1;
                newFaceArray.faces[pointIdx + 1] = edgeIndex2;
                newFaceArray.faces[pointIdx + 2] = polytope.numSupports;    // 아직 polytope에 점이 추가되지 않음
                ++newFaceArray.numFaces;
            }

            // polytope에 새로운 점 추가
            polytope.supports[polytope.numSupports++] = support;

            uint32_t newMinFace = GetFaceNormals(polytope, newFaceArray);

            if (newMinFace == -1) {
                minDistance = -1.0f;
                minNormal = XMVectorZero();
                break;
            }

            float oldMinDistance = FLT_MAX;
            for (size_t i = 0; i < faceArray.numFaces; ++i) {
                if (faceArray.normals[i].w < oldMinDistance) {
                    oldMinDistance = faceArray.normals[i].w;
                    minFace = i;
                }
            }

            if (newFaceArray.normals[newMinFace].w < oldMinDistance) {
                minFace = newMinFace + faceArray.numFaces;
            }

            MergeFaceArray(faceArray, newFaceArray);
        }
    }

    ResultEPA result;
    result.normal = minNormal;
    result.dist = minDistance;

    return result;
}

bool Contact::LineSimplex(Polytope& simplex, XMVECTOR& dir)
{
    SupportPoint a = simplex.supports[1]; // new Point
    SupportPoint b = simplex.supports[0];

    XMVECTOR ab = b.diff - a.diff;
    XMVECTOR ao = -a.diff;

    if (IsSimilarDirection(ab, ao)) {
        dir = XMVector3Cross(XMVector3Cross(ab, ao), ab);
    }
    else {
        simplex = { a };
        dir = ao;
    }

    return false;
}

bool Contact::TriangleSimplex(Polytope& simplex, XMVECTOR& dir)
{
    SupportPoint a = simplex.supports[2]; // new Point
    SupportPoint b = simplex.supports[1];
    SupportPoint c = simplex.supports[0];

    XMVECTOR ab = b.diff - a.diff;
    XMVECTOR ac = c.diff - a.diff;
    XMVECTOR ao = -a.diff;

    XMVECTOR abc = XMVector3Cross(ab, ac);
    // 원점이 선분 bc밖에 있는 경우는 Line단계에서 걸러진다.

    XMVECTOR acNormal = XMVector3Cross(abc, ac);

    // 1. 원점이 AC 엣지 바깥쪽에 있는가?
    if (IsSimilarDirection(acNormal, ao)) {

        if (IsSimilarDirection(ac, ao)) {
            // Region AC (선분 AC에 가장 가까움)
            simplex = { a, c };
            dir = XMVector3Cross(XMVector3Cross(ac, ao), ac);
        }
        else {
            // Region A (A 점에 가장 가까움 - Star Case)
            simplex = { a }; // <-- Simplex를 {A}로 축소
            dir = ao;        // <-- 검색 방향을 A->O로 설정
            return false;    // Simplex 축소 후 계속 루프
        }
    }
    else {
        XMVECTOR abNormal = XMVector3Cross(ab, abc);

        // 2. 원점이 AB 엣지 바깥쪽에 있는가?
        if (IsSimilarDirection(abNormal, ao)) {
            // Region AB (선분 AB에 가장 가까움)
            return LineSimplex(simplex = { a, b }, dir);
        }
        else {
            // 원점이 삼각형 abc 위에 존재하는지 아래 존재하는지에 따라 다음 support를 구할 direction의 방향을 결정한다.
            if (IsSimilarDirection(abc, ao)) {
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

bool Contact::TetrahedronSimplex(Polytope& simplex, XMVECTOR& dir)
{
    SupportPoint a = simplex.supports[3]; // new Point
    SupportPoint b = simplex.supports[2];
    SupportPoint c = simplex.supports[1];
    SupportPoint d = simplex.supports[0];

    XMVECTOR ab = b.diff - a.diff;
    XMVECTOR ac = c.diff - a.diff;
    XMVECTOR ad = d.diff - a.diff;
    XMVECTOR ao = -a.diff;

    XMVECTOR abc = XMVector3Cross(ab, ac);
    XMVECTOR acd = XMVector3Cross(ac, ad);
    XMVECTOR adb = XMVector3Cross(ad, ab);
    // 원점이 삼각형 bcd 밖에 존재하는 경우는 Triangle단계에서 걸러진다.

    // 원점이 삼각형 abc 밖에 존재함 -> d를 다시 선정한다.
    if (IsSimilarDirection(abc, ao)) {
        return TriangleSimplex(simplex = { a, b, c }, dir);
    }

    // 원점이 삼각형 acd 밖에 존재함 -> b를 다시 선정한다.
    if (IsSimilarDirection(acd, ao)) {
        return TriangleSimplex(simplex = { a, c, d }, dir);
    }

    // 원점이 삼각형 adb 밖에 존재함 -> c를 다시 선정한다.
    if (IsSimilarDirection(adb, ao)) {
        return TriangleSimplex(simplex = { a, d, b }, dir);
    }

    // 원점이 사면체 abcd안에 존재한다.
    return true;
}

bool Contact::NextSimplex(Polytope& simplex, XMVECTOR& dir)
{
    switch (simplex.numSupports) {
    case 2: 
        return LineSimplex(simplex, dir);
    case 3: 
        return TriangleSimplex(simplex, dir);
        
    case 4: 
        // 3차원 충돌만 계산하므로 Tetrahedron에서만 true를 반환할 수 있다.
        return TetrahedronSimplex(simplex, dir);
    }

    // never should be here
    return false;
}

bool Contact::GetGJK(Polytope& simplex, const ConvexInfo& convexA, const ConvexInfo& convexB)
{
    XMVECTOR direction;

    // 첫 번째 support point 구하기
    XMVECTOR centerAB = XMLoadFloat3(&convexB.center) - XMLoadFloat3(&convexA.center);
    if (Vec3LengthSq(centerAB) < 1e-6f) {
        direction = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
    }
    else {
        direction = XMVector3NormalizeEst(centerAB);
    }

    SupportPoint support = GetSupportPoint(convexA, convexB, direction);
    simplex.supports[simplex.numSupports++] = support;

    if (Vec3LengthSq(support.diff) == 0.0f) {
        direction = -direction;
        simplex.supports[0] = GetSupportPoint(convexA, convexB, direction);
    }

    // New direction is towards the origin
    direction = XMVector3NormalizeEst(-simplex.supports[0].diff);

    uint32_t iter = 0;
    while (iter++ < MAX_GJK_ITERATION) {
        support = GetSupportPoint(convexA, convexB, direction);

        // support와 direction의 내적 값이 0보다 작으면 두점 사이에 원점이 포함 되지 않는다. 
        if (VecDot(support.diff, direction) < EPS_FLOAT) {
            return false; // no collision
        }

        simplex.supports[simplex.numSupports++] = support;
        
        // simplex 확장
        if (NextSimplex(simplex, direction)) {
            // 원점 포함 = 충돌
            return true;
        }
    }

    return false;
}

bool Contact::IsSimilarDirection(const XMVECTOR& a, const XMVECTOR& b) const
{
    return VecDot(a, b) > 0.f;
}

bool Contact::IsSameDirection(const XMVECTOR& a, const XMVECTOR& b) const
{
    return XMVectorGetX(XMVector3Length(XMVector3Cross(a, b))) < EPS_FLOAT;
}

void Contact::AddIfUniqueEdge(vector<pair<uint32_t, uint32_t>>& uniqueEdges, const uint32_t* faces, uint32_t a, uint32_t b)
{
    auto reverse = std::find(                       //      0--<--3
        uniqueEdges.begin(),                        //     / \ B /   A: 2-0
        uniqueEdges.end(),                          //    / A \ /    B: 0-2
        std::make_pair(faces[b], faces[a])          //   1-->--2
    );

    if (reverse != uniqueEdges.end()) {
        uniqueEdges.erase(reverse);
    }

    else {
        uniqueEdges.emplace_back(faces[a], faces[b]);
    }
}

bool Contact::IsDuplicatedPoint(const Polytope& polytope, const XMVECTOR& supportPoint)const
{
    // supportPoint가 이미 polytope에 존재한다면 true 반환
    size_t numPoints = polytope.numSupports;
    for (size_t i = 0; i < numPoints; ++i) {
        const XMVECTOR delta = polytope.supports[i].diff - supportPoint;
        if (XMVectorGetX(XMVector3LengthSq(delta)) < EPS_FLOAT)
            return true;
    }
    return false;
}

void Contact::MergeFaceArray(FaceArray& faceArray, FaceArray& newFaceArray)
{
    uint32_t faceArrayCount = faceArray.numFaces;
    uint32_t newFaceArrayCount = newFaceArray.numFaces;
    uint32_t newCount = newFaceArrayCount + faceArrayCount;

    if (newCount > faceArray.maxNumFaces) {
        uint32_t newMaxCount = faceArray.maxNumFaces;
        while (newMaxCount < newCount) {
            newMaxCount = newMaxCount * 2;
        }

        SizeUpFaceArray(faceArray, newMaxCount);
    }

    memcpy(faceArray.faces + faceArrayCount * 3, newFaceArray.faces, sizeof(uint32_t) * newFaceArrayCount * 3);
    memcpy(faceArray.normals + faceArrayCount, newFaceArray.normals, sizeof(XMFLOAT4) * newFaceArrayCount);

    faceArray.numFaces = newCount;
}

void Contact::SizeUpFaceArray(FaceArray& faceArray, uint32_t newMaxNumFaces)
{
    if (newMaxNumFaces <= faceArray.maxNumFaces)
        return;

    uint32_t numFaces = faceArray.numFaces;
    uint32_t* newFaces = new uint32_t[newMaxNumFaces * 3];
    memcpy(newFaces, faceArray.faces, sizeof(uint32_t) * numFaces * 3);
    XMFLOAT4* newNormals = new XMFLOAT4[newMaxNumFaces];
    memcpy(newNormals, faceArray.normals, sizeof(XMFLOAT4) * numFaces);

    delete[] faceArray.faces;
    delete[] faceArray.normals;

    faceArray.numFaces = newMaxNumFaces;
    faceArray.faces = newFaces;
    faceArray.normals = newNormals;
}

void Contact::FreeConvexInfo(ConvexInfo& convexA, ConvexInfo& convexB)
{
    delete[] convexA.axes;
    delete[] convexB.axes;
    delete[] convexA.points;
    delete[] convexB.points;
}

} // naemspace spe