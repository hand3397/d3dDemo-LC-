#include "Contact.h"
#include "SphereToSphereContact.h"
#include "SphereToBoxContact.h"
#include "BoxToBoxContact.h"

namespace spe {;

const float EPS_FLOAT = 1e-3f;
const float EPS_FLOAT_SQ = 1e-6f;

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

    linkA.contact = this;
    linkB.contact = this;
    linkA.other = fixtureB_->GetRigidbody();
    linkB.other = fixtureA_->GetRigidbody();

    SetFlag(ContactFlag::TOUCHING);
}

void Contact::Update()
{
    Rigidbody* bodyA = fixtureA_->GetRigidbody();
    Rigidbody* bodyB = fixtureB_->GetRigidbody();
    XMMATRIX transformA = bodyA->GetTransformMatrix();
    XMMATRIX transformB = bodyB->GetTransformMatrix();

    // Evaluate
    manifold_.numPoints = 0;
    Evaluate(manifold_, transformA, transformB);

    for (uint32_t i = 0; i < manifold_.numPoints; ++i) {
        ManifoldPoint& manifoldPoint = manifold_.points[i];

        manifoldPoint.normalImpulse = 0.0f;
        manifoldPoint.tangentImpulse = 0.0f;
    }

    // is Touching
    if (manifold_.numPoints > 0) {
        SetFlag(ContactFlag::TOUCHING);
    }
    else {
        ClearFlag(ContactFlag::TOUCHING);
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

    // 구vs구 충돌의 경우 GJK -> EPA를 사용하지 않아도 단순계산으로 빠르게 충돌여부를 판단할 수 있기 때문에 예외처리를 한다.
    bool isCollide = GetGJK(simplex, convexA, convexB);

    if (!isCollide) {
        FreeConvexInfo(convexA, convexB);
        return;
    }

    ResultEPA resultEPA = GetEPA(simplex, convexA, convexB);

    if (resultEPA.distance == -1.0f) {
        FreeConvexInfo(convexA, convexB);
        return;
    }

    if (shapeA->GetType() == ShapeType::BOX && shapeB->GetType() == ShapeType::BOX)
        //if (fixtureA_->GetRigidbody()->GetType() == RigidbodyType::DYNAMIC && fixtureB_->GetRigidbody()->GetType() == RigidbodyType::DYNAMIC)
            int a = 1;

    FindCollisionPoints(convexA, convexB, collisionInfo, resultEPA, &simplex);
    GenerateManifolds(collisionInfo, manifold, fixtureA_, fixtureB_);

    FreeConvexInfo(convexA, convexB);
}

void Contact::GenerateManifolds(CollisionInfo& collisionInfo, Manifold& manifold, Fixture* fixtureA, Fixture* fixtureB)
{
    int32_t collisionInfoSize = collisionInfo.size;
    manifold.numPoints = collisionInfoSize;

    for (int32_t i = 0; i < collisionInfoSize; ++i) {
        manifold.points[i].pointA = collisionInfo.pointA[i];
        manifold.points[i].pointB = collisionInfo.pointB[i];
        manifold.points[i].normal = collisionInfo.normal[i];
        manifold.points[i].separation = collisionInfo.separation[i];
    }
}

bool Contact::HasFlag(ContactFlag flag) const
{
    return (flags_ & static_cast<uint32_t>(flag)) != 0;
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

int32_t Contact::GetFaceNormals(const Polytope& polytope, FaceArray& faceArray) const
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

SupportPoint Contact::GetSupportPoint(const ConvexInfo& convexA, const ConvexInfo& convexB, const XMVECTOR& dir) const
{
    XMVECTOR ddir = XMVector3Normalize(dir);
    SupportPoint support;
    XMVECTOR pointA = convexA.GetFarthestPoint(ddir);
    XMStoreFloat3(&support.a, pointA);
    XMVECTOR pointB = convexB.GetFarthestPoint(-ddir);
    XMStoreFloat3(&support.b, pointB);
    support.diff = pointA - pointB;

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

ResultEPA Contact::GetEPA(Polytope& polytope, const ConvexInfo& convexA, const ConvexInfo& convexB) const
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
    result.distance = minDistance;

    return result;
}

ContactLink* Contact::GetContactLinkA()
{
    return &linkA;
}

ContactLink* Contact::GetContactLinkB()
{
    return &linkB;
}

Contact* Contact::GetNext()
{
    return next_;
}

Contact* Contact::GetPrev()
{
    return prev_;
}

void Contact::SetFlag(ContactFlag flag)
{
    flags_ |= static_cast<uint32_t>(flag);
}

void Contact::ClearFlag(ContactFlag flag)
{
    flags_ &= ~static_cast<uint32_t>(flag);
}

void Contact::SetNext(Contact* contact)
{
    next_ = contact;
}

void Contact::SetPrev(Contact* contact)
{
    prev_ = contact;
}

bool Contact::LineSimplex(Polytope& simplex, XMVECTOR& dir)
{
    const SupportPoint& a = simplex.supports[1]; // new Point
    const SupportPoint& b = simplex.supports[0];

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
    const SupportPoint& a = simplex.supports[2]; // new Point
    const SupportPoint& b = simplex.supports[1];
    const SupportPoint& c = simplex.supports[0];

    XMVECTOR ab = b.diff - a.diff;
    XMVECTOR ac = c.diff - a.diff;
    XMVECTOR ao = -a.diff;

    XMVECTOR abc = XMVector3Cross(ab, ac);
    // 원점이 선분 bc밖에 있는 경우는 Line단계에서 걸러진다.

    // 삼각형의 법선 계산 (삼각형 평면 위에서 바깥쪽을 향함)
    XMVECTOR abPerp = XMVector3Cross(ab, abc);
    XMVECTOR acPerp = XMVector3Cross(abc, ac);

    // 1. AC Edge 바깥쪽 체크
    if (IsSimilarDirection(acPerp, ao)) {
        if (IsSimilarDirection(ac, ao)) {
            // AC 선분 영역: B를 제거하고 AC로 LineSimplex 처리와 동일
            simplex = { c, a };
            dir = XMVector3Cross(XMVector3Cross(ac, ao), ac);
        }
        else {
            // Region A (A 점에 가장 가까움 - Star Case)
            simplex = { a }; // <-- Simplex를 {A}로 축소
            dir = ao;        // <-- 검색 방향을 A->O로 설정
            return false;    // Simplex 축소 후 계속 루프
        }
    }
    // 2. AB Edge 바깥쪽 체크
    else if (IsSimilarDirection(abPerp, ao)) {
        if (XMVector3Greater(XMVector3Dot(ab, ao), XMVectorZero())) {
            // AB 선분 영역: C를 제거
            simplex = { b, a };
            dir = XMVector3Cross(XMVector3Cross(ab, ao), ab);
        }
        else {
            // A 점 영역
            simplex = { a };
            dir = ao;
        }
    }
    // 3.삼각형 내부 (위 또는 아래)
    else {
        // 원점이 삼각형의 위/아래 중 어디에 있는지 확인하여 방향 설정
        if (IsSimilarDirection(abc, ao)) {
            dir = abc;
        }
        else {
            simplex = { b, c, a };
            dir = -abc;
        }
    }

    return false;
}

bool Contact::TetrahedronSimplex(Polytope& simplex, XMVECTOR& dir)
{
    const SupportPoint& a = simplex.supports[3]; // new Point
    const SupportPoint& b = simplex.supports[2];
    const SupportPoint& c = simplex.supports[1];
    const SupportPoint& d = simplex.supports[0];

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
        return TriangleSimplex(simplex = { c, b, a }, dir);
    }

    // 원점이 삼각형 acd 밖에 존재함 -> b를 다시 선정한다.
    if (IsSimilarDirection(acd, ao)) {
        return TriangleSimplex(simplex = { d, c, a }, dir);
    }

    // 원점이 삼각형 adb 밖에 존재함 -> c를 다시 선정한다.
    if (IsSimilarDirection(adb, ao)) {
        return TriangleSimplex(simplex = { d, b, a }, dir);
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
        direction = XMVector3Normalize(XMVectorSet(0.1f, 0.9f, 0.3f, 0.0f));
    }
    else {
        direction = XMVector3Normalize(centerAB + XMVectorSet(0.1f, 0.1f, 0.1f, 0.0f));
    }

    SupportPoint support = GetSupportPoint(convexA, convexB, direction);
    simplex.supports[simplex.numSupports++] = support;

    if (Vec3LengthSq(support.diff) == 0.0f) {
        direction = -direction;
        simplex.supports[0] = GetSupportPoint(convexA, convexB, direction);
    }

    // New direction is towards the origin
    direction = XMVector3Normalize(-simplex.supports[0].diff);

    uint32_t iter = 0;
    while (iter++ < MAX_GJK_ITERATION) {
        support = GetSupportPoint(convexA, convexB, direction);

        // support와 direction의 내적 값이 0보다 작으면 두점 사이에 원점이 포함될 수 없다. 
        if (VecDot(support.diff, direction) < -EPS_FLOAT || IsDuplicatedPoint(simplex, support.diff)) {
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

void Contact::AddIfUniqueEdge(vector<pair<uint32_t, uint32_t>>& uniqueEdges, const uint32_t* faces, uint32_t a, uint32_t b) const
{
    auto reverse = std::find(uniqueEdges.begin(), uniqueEdges.end(),
        std::make_pair(faces[b], faces[a]));

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

void Contact::ComputeContactPolygon(ContactFace& contactFace, Face& refFace, Face& incFace)
{
    if (refFace.numPoints == 0 || incFace.numPoints == 0)
        return;

    // incFace를 refFace에 대해 clipping한다.
    // 1. incFace의 각 점을 refFace의 각 edge에 대해 clipping
    // 2. incFace의 각 점을 refFace에 대해 clipping
    memcpy(contactFace.points, incFace.points, sizeof(XMFLOAT3) * incFace.numPoints);
    contactFace.numPoints = incFace.numPoints;

    int numPoints = refFace.numPoints;
    XMVECTOR refFaceNormal = XMLoadFloat3(&refFace.normal);
    for (uint32_t i = 0; i < numPoints; ++i) {
        XMVECTOR start = XMLoadFloat3(&refFace.points[i]);
        XMVECTOR end = XMLoadFloat3(&refFace.points[(i + 1) % numPoints]);

        // edge
        XMVECTOR edge = end - start;

        // refFace.normal과 edge의 cross => 사이드 plane normal
        XMVECTOR sideNormal = XMVector3Normalize(XMVector3Cross(refFaceNormal, edge));
        float sideDist = VecDot(sideNormal, start);
        ClipPolygonAgainstPlane(contactFace, sideNormal, sideDist);

        if (contactFace.numPoints == 0)
            break;
    }

    // incFace 평면에 대해서도 클리핑
    //ClipPolygonAgainstPlane(contactFace, refFaceNormal, refFace.distance);
}

void Contact::ClipPolygonAgainstPlane(ContactFace& contactFace, const XMVECTOR& planeNormal, float planeDist)
{
    int32_t numPoints = contactFace.numPoints;
    if (numPoints == 0)
        return;

    int32_t idx = 0;

    for (size_t i = 0; i < numPoints; i++) {
        const XMVECTOR curr = XMLoadFloat3(&contactFace.points[i]);
        const XMVECTOR next = XMLoadFloat3(&contactFace.points[(i + 1) % numPoints]);

        float distCurr = VecDot(planeNormal, curr) - planeDist;
        float distNext = VecDot(planeNormal, next) - planeDist;
        bool currInside = (distCurr >= -EPS_FLOAT);
        bool nextInside = (distNext >= -EPS_FLOAT);

        // CASE1: 둘 다 내부
        if (currInside && nextInside) {
            XMStoreFloat3(&contactFace.buffer[idx++], next);
        }
        // CASE2: 밖->안
        else if (!currInside && nextInside) {
            float t = distCurr / (distCurr - distNext);
            XMVECTOR intersect = curr + t * (next - curr);
            XMStoreFloat3(&contactFace.buffer[idx++], intersect);
            XMStoreFloat3(&contactFace.buffer[idx++], next);
        }
        // CASE3: 안->밖
        else if (currInside && !nextInside) {
            float t = distCurr / (distCurr - distNext);
            XMVECTOR intersect = curr + t * (next - curr);
            XMStoreFloat3(&contactFace.buffer[idx++], intersect);
        }
        // CASE4: 둘 다 밖 => nothing
    }

    memcpy(contactFace.points, contactFace.buffer, sizeof(XMFLOAT3) * idx);
    contactFace.numPoints = idx;
}

void Contact::BuildManifoldFromPolygon(CollisionInfo& collisionInfo, const Face& refFace, const Face& incFace, 
    ContactFace& contactFace, ResultEPA& resultEPA)
{
    const uint32_t numPoints = contactFace.numPoints;
    if (numPoints == 0) {
        return;
    }

    // Preload SIMD values
    const XMVECTOR refNormal = XMLoadFloat3(&refFace.normal);
    const XMVECTOR normal = resultEPA.normal;

    const float refPlaneDist = refFace.distance;

    for (int32_t i = 0; i < numPoints; ++i) {
        const XMVECTOR pointB = XMLoadFloat3(&contactFace.points[i]);

        // signed distance from ref face plane
        const float separation = VecDot(pointB, refNormal) - refPlaneDist;

        // penetration depth (positive value)
        const float penetration = -separation;

        if (penetration <= EPS_FLOAT_SQ) {
            continue; // 접촉 아님
        }

        // pointB를 normal방향으로 penetration만큼 밀어내 refFace위의 pointA를 만든다.
        const XMVECTOR pointA = XMVectorMultiplyAdd(
            normal,
            XMVectorReplicate(penetration),
            pointB
        );

        XMStoreFloat3(&collisionInfo.normal[collisionInfo.size], refNormal);
        XMStoreFloat3(&collisionInfo.pointA[collisionInfo.size], pointA);
        XMStoreFloat3(&collisionInfo.pointB[collisionInfo.size], pointB);
        collisionInfo.separation[collisionInfo.size] = penetration;
        ++collisionInfo.size;
    }
}

void Contact::SortVerticesClockwise(XMFLOAT3* vertices, const XMVECTOR& center, const XMVECTOR& normal, uint32_t verticesSize)
{
    // 1. 법선 벡터 기준으로 평면의 두 축 정의
    XMVECTOR u = XMVector3Normalize(XMVector3Cross(normal, XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f)));
    if (XMVectorGetX(XMVector3LengthSq(u)) < EPS_FLOAT_SQ) {
        // normal이 x축과 평행한 경우 y축 사용
        u = XMVector3Normalize(XMVector3Cross(normal, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)));
    }
    XMVECTOR v = XMVector3Normalize(XMVector3Cross(normal, u)); // 법선과 u의 외적

    // 2. 각도 계산 및 정렬
    auto static angleComparator = [&center, &u, &v](const XMFLOAT3& a, const XMFLOAT3& b) {
        // a와 b를 u, v 축 기준으로 투영
        XMVECTOR dA = XMLoadFloat3(&a) - center;
        XMVECTOR dB = XMLoadFloat3(&b) - center;

        float angleA = atan2(VecDot(dA, v), VecDot(dA, u));
        float angleB = atan2(VecDot(dB, v), VecDot(dB, u));

        return angleA > angleB; // 시계 방향 정렬
        };

    std::sort(vertices, vertices + verticesSize, angleComparator);
}

void Contact::SetBoxFace(Face& face, const ConvexInfo& box, const XMVECTOR& normal)
{
    const XMVECTOR axes[3] = { XMLoadFloat3(&box.axes[0]), XMLoadFloat3(&box.axes[1]) , XMLoadFloat3(&box.axes[2]) };
    const float hs[3] = { box.halfSize.x, box.halfSize.y, box.halfSize.z };

    // dot(±axis, normal) → sign만 비교하면 되므로 dot은 축당 1회
    float vecDots[3] = { VecDot(axes[0], normal), VecDot(axes[1], normal), VecDot(axes[2], normal) };
    
    // 각 축에 대해 더 큰 절대값이 곧 해당 축에서 가장 평행한 face
    float absVecDots[3] = { fabsf(vecDots[0]), fabsf(vecDots[1]), fabsf(vecDots[2])};
    
    int base = 0;
    float maxA = absVecDots[0];
    if (absVecDots[1] > maxA) {
        base = 1; 
        maxA = absVecDots[1];
    }
    if (absVecDots[2] > maxA) {
        base = 2; 
        maxA = absVecDots[2];
    }

    // base 축의 방향 sign
    float sign = (vecDots[base] >= 0.0f) ? 1.0f : -1.0f;

    // baseAxis = ± axes[base]
    XMVECTOR baseAxis = axes[base] * sign;

    // 법선 저장
    XMStoreFloat3(&face.normal, baseAxis);

    // index1 / index2 lookup
    constexpr static const uint8_t NEXT1[3] = { 1, 2, 0 };
    constexpr static const uint8_t NEXT2[3] = { 2, 0, 1 };

    int i1 = NEXT1[base];
    int i2 = NEXT2[base];

    XMVECTOR h1 = axes[i1] * hs[i1];
    XMVECTOR h2 = axes[i2] * hs[i2];

    XMVECTOR basePoint = XMLoadFloat3(&box.center) + baseAxis * hs[base];
    // 평면 정보 저장
    face.distance = VecDot(basePoint, baseAxis);

    // 정점 4개 생성 (고정 와인딩: CW)
    face.numPoints = 4;
    XMStoreFloat3(&face.points[0], basePoint - h1 - h2);
    XMStoreFloat3(&face.points[1], basePoint + h1 - h2);
    XMStoreFloat3(&face.points[2], basePoint + h1 + h2);
    XMStoreFloat3(&face.points[3], basePoint - h1 + h2);
}

void Contact::MergeFaceArray(FaceArray& faceArray, FaceArray& newFaceArray) const
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

void Contact::SizeUpFaceArray(FaceArray& faceArray, uint32_t newMaxNumFaces) const
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

    faceArray.maxNumFaces = newMaxNumFaces;
    faceArray.faces = newFaces;
    faceArray.normals = newNormals;
}

void Contact::FreeConvexInfo(ConvexInfo& convexA, ConvexInfo& convexB) const
{
    delete[] convexA.axes;
    delete[] convexB.axes;
    delete[] convexA.points;
    delete[] convexB.points;
}

} // naemspace spe