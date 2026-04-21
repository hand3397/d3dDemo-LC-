#include "Contact.h"
#include "SphereToSphereContact.h"
#include "SphereToBoxContact.h"
#include "BoxToBoxContact.h"
#include "CylinderToCylinderContact.h"
#include "SphereToCylinderContact.h"
#include "BoxToCylinderContact.h"
#include "ShapeContact.h"

namespace spe {
    ;

    const float EPS_FLOAT = 1e-3f;
    const float EPS_FLOAT_SQ = 1e-6f;

    const uint32_t Contact::MAX_GJK_ITERATION = 64;

    const ContactMemberFunction Contact::createContactFunctions_[32] = {
        nullptr,							                                // 0
        &SphereToSphereContact::Create,		                                // 01    sphere   | sphere
        &ClippingContact<ShapeType::BOX, ShapeType::BOX>::Create,           // 10    box      | box
        &SphereToConvexContact::Create,		                                // 11    sphere   | box
        &ClippingContact<ShapeType::BOX, ShapeType::BOX>::Create,           // 100   box      | box
        &SphereToConvexContact::Create,		                                // 101   sphere   | box
        &ClippingContact<ShapeType::BOX, ShapeType::BOX>::Create,           // 110   box      | box
        nullptr,						                                    // 111
        &ClippingContact<ShapeType::CYLINDER, ShapeType::CYLINDER>::Create, // 1000  cylinder | cylinder
        &SphereToConvexContact::Create,	                                    // 1001  sphere   | cylinder
        &ClippingContact<ShapeType::BOX, ShapeType::CYLINDER>::Create,      // 1010  box      | cylinder
        nullptr,							                                // 1011
        &ClippingContact<ShapeType::BOX, ShapeType::CYLINDER>::Create,		// 1100  box      | cylinder
        nullptr,						                                    // 1101
        nullptr,						                                    // 1110
        nullptr,						                                    // 1111
        &ClippingContact<ShapeType::CAPSULE, ShapeType::CAPSULE>::Create,   // 10000 capsule  | capsule
        &SphereToConvexContact::Create,                                     // 10001 sphere   | capsule
        &ClippingContact<ShapeType::BOX, ShapeType::CAPSULE>::Create,       // 10010 box      | capsule
        nullptr,							                                // 10011
        &ClippingContact<ShapeType::BOX, ShapeType::CAPSULE>::Create,       // 10100 box      | capsule
        nullptr,							                                // 10101
        nullptr,							                                // 10110
        nullptr,							                                // 10111
        &ClippingContact<ShapeType::CYLINDER, ShapeType::CAPSULE>::Create, // 11000 cylinder | capsule
        nullptr,							                                // 11001
        nullptr,							                                // 11010
        nullptr,							                                // 11011
        nullptr,							                                // 11100
        nullptr,						                                    // 11101
        nullptr,						                                    // 11110
        nullptr,						                                    // 11111
    };

    Contact::~Contact()
    {
        Rigidbody* bodyA = fixtureA_->GetRigidbody();
        Rigidbody* bodyB = fixtureB_->GetRigidbody();

        // 두 rigidbody의 contactLink 끊기
        if (linkA_.prev != nullptr) linkA_.prev->next = linkA_.next;
        if (linkA_.next != nullptr) linkA_.next->prev = linkA_.prev;
        if (bodyA->GetContactLink() == &linkA_) bodyA->SetContactLink(linkA_.next);
        linkA_.contact = nullptr;

        if (linkB_.prev != nullptr) linkB_.prev->next = linkB_.next;
        if (linkB_.next != nullptr) linkB_.next->prev = linkB_.prev;
        if (bodyB->GetContactLink() == &linkB_) bodyB->SetContactLink(linkB_.next);
        linkB_.contact = nullptr;
    }

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

        linkA_.contact = this;
        linkB_.contact = this;
        linkA_.other = fixtureB_->GetRigidbody();
        linkB_.other = fixtureA_->GetRigidbody();

        SetFlag(ContactFlag::TOUCHING);
    }

    void Contact::Update()
    {
        Rigidbody* bodyA = fixtureA_->GetRigidbody();
        Rigidbody* bodyB = fixtureB_->GetRigidbody();
        XMMATRIX transformA = bodyA->GetTransformMatrix();
        XMMATRIX transformB = bodyB->GetTransformMatrix();

        // [Warm Starting 1단계] 이전 프레임의 Manifold 저장
        Manifold oldManifold = manifold_;

        // Evaluate (새로운 충돌 지점 계산)
        manifold_.numPoints = 0;
        Evaluate(manifold_, transformA, transformB);

        // [Warm Starting 2단계] 이전 프레임의 충격량을 현재 프레임으로 계승

        // 매칭 거리 허용치 (너무 크면 엉뚱한 점의 힘을 가져오므로 작게 설정)
        const float matchThresholdSq = 0.05f * 0.05f;

        for (uint32_t i = 0; i < manifold_.numPoints; ++i) {
            ManifoldPoint& newPoint = manifold_.points[i];

            // 기본적으로는 0으로 시작 (새로운 접촉점일 경우)
            newPoint.normalImpulse = 0.0f;
            newPoint.tangentImpulse = 0.0f;

            // 이전 프레임의 접촉점 중 위치가 비슷한 점을 찾음 (Nearest Neighbor Matching)
            for (uint32_t j = 0; j < oldManifold.numPoints; ++j) {
                ManifoldPoint& oldPoint = oldManifold.points[j];

                // World Space 상의 접촉점 거리 비교
                XMVECTOR diff = XMLoadFloat3(&newPoint.pointA) - XMLoadFloat3(&oldPoint.pointA);
                if (XMVectorGetX(XMVector3LengthSq(diff)) < matchThresholdSq) {
                    // 매칭 성공: 이전 프레임의 누적 충격량을 가져옴
                    newPoint.normalImpulse = oldPoint.normalImpulse;
                    newPoint.tangentImpulse = oldPoint.tangentImpulse;
                    break;
                }
            }
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

        bool isCollide = GetGJK(simplex, convexA, convexB);

        if (!isCollide) {
            FreeConvexInfo(convexA, convexB);
            manifold.numPoints = 0;
            return;
        }

        ResultEPA resultEPA = GetEPA(simplex, convexA, convexB);
        //resultEPA normal = (A -> B)
        if (resultEPA.distance == -1.0f) {
            FreeConvexInfo(convexA, convexB);
            manifold.numPoints = 0;
            return;
        }

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
            // pointB -> pointA 방향의 normalVector
            manifold.points[i].normal = collisionInfo.normal[i];
            // 면 침투시 separation은 침투깊이의 크기만큼의 양수값
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
            minNormal = XMVectorSetW(XMLoadFloat4(&faceArray.normals[minFace]), 0.0f);
            minDistance = faceArray.normals[minFace].w;

            SupportPoint support = GetSupportPoint(convexA, convexB, minNormal);
            float sDistance = VecDot(minNormal, support.diff);

            if (std::abs(minDistance - sDistance) > EPS_FLOAT && !IsDuplicatedPoint(polytope, support.diff)) {
                minDistance = FLT_MAX;

                std::vector<std::pair<uint32_t, uint32_t>> uniqueEdges;

                for (uint32_t i = 0; i < faceArray.numFaces; i++) {
                    XMVECTOR normalVec = XMVectorSetW(XMLoadFloat4(&faceArray.normals[i]), 0.0f);
                    if (IsSimilarDirection(normalVec, support.diff - polytope.supports[faceArray.faces[i * 3]].diff)) {
                        uint32_t faceIdx = i * 3;

                        AddIfUniqueEdge(uniqueEdges, faceArray.faces, faceIdx, faceIdx + 1);
                        AddIfUniqueEdge(uniqueEdges, faceArray.faces, faceIdx + 1, faceIdx + 2);
                        AddIfUniqueEdge(uniqueEdges, faceArray.faces, faceIdx + 2, faceIdx);

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

                if (uniqueEdges.empty()) {
                    break;
                }

                newFaceArray.numFaces = 0;
                for (auto [edgeIndex1, edgeIndex2] : uniqueEdges) {
                    uint32_t pointIdx = newFaceArray.numFaces * 3;
                    newFaceArray.faces[pointIdx] = edgeIndex1;
                    newFaceArray.faces[pointIdx + 1] = edgeIndex2;
                    newFaceArray.faces[pointIdx + 2] = polytope.numSupports;
                    ++newFaceArray.numFaces;
                }

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
        return &linkA_;
    }

    ContactLink* Contact::GetContactLinkB()
    {
        return &linkB_;
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
        const SupportPoint& a = simplex.supports[1];
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
        const SupportPoint& a = simplex.supports[2];
        const SupportPoint& b = simplex.supports[1];
        const SupportPoint& c = simplex.supports[0];

        XMVECTOR ab = b.diff - a.diff;
        XMVECTOR ac = c.diff - a.diff;
        XMVECTOR ao = -a.diff;

        XMVECTOR abc = XMVector3Cross(ab, ac);

        XMVECTOR abPerp = XMVector3Cross(ab, abc);
        XMVECTOR acPerp = XMVector3Cross(abc, ac);

        if (IsSimilarDirection(acPerp, ao)) {
            if (IsSimilarDirection(ac, ao)) {
                simplex = { c, a };
                dir = XMVector3Cross(XMVector3Cross(ac, ao), ac);
            }
            else {
                simplex = { a };
                dir = ao;
                return false;
            }
        }
        else if (IsSimilarDirection(abPerp, ao)) {
            if (XMVector3Greater(XMVector3Dot(ab, ao), XMVectorZero())) {
                simplex = { b, a };
                dir = XMVector3Cross(XMVector3Cross(ab, ao), ab);
            }
            else {
                simplex = { a };
                dir = ao;
            }
        }
        else {
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
        const SupportPoint& a = simplex.supports[3];
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

        if (IsSimilarDirection(abc, ao)) {
            return TriangleSimplex(simplex = { c, b, a }, dir);
        }

        if (IsSimilarDirection(acd, ao)) {
            return TriangleSimplex(simplex = { d, c, a }, dir);
        }

        if (IsSimilarDirection(adb, ao)) {
            return TriangleSimplex(simplex = { d, b, a }, dir);
        }

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
            return TetrahedronSimplex(simplex, dir);
        }

        return false;
    }

    bool Contact::GetGJK(Polytope& simplex, const ConvexInfo& convexA, const ConvexInfo& convexB)
    {
        XMVECTOR direction;

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

        direction = XMVector3Normalize(-simplex.supports[0].diff);

        uint32_t iter = 0;
        while (iter++ < MAX_GJK_ITERATION) {
            support = GetSupportPoint(convexA, convexB, direction);

            if (VecDot(support.diff, direction) < -EPS_FLOAT || IsDuplicatedPoint(simplex, support.diff)) {
                return false;
            }

            simplex.supports[simplex.numSupports++] = support;

            if (NextSimplex(simplex, direction)) {
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
        // incFace를 refFace기준으로 잘라 두 물체의 실제 충돌 면인 contactFace를 추출하는 함수
        if (refFace.numPoints == 0 || incFace.numPoints == 0)
            return;

        // contactFace에 incFace 붙여넣기
        memcpy(contactFace.points, incFace.points, sizeof(XMFLOAT3) * incFace.numPoints);
        contactFace.numPoints = incFace.numPoints;

        // sideNormal 보정용 refFace center 구하기
        XMVECTOR refCenter = XMLoadFloat3(&refFace.center);

        int numPoints = refFace.numPoints;
        // contactFace를 refFace로 자르기
        XMVECTOR refFaceNormal = XMLoadFloat3(&refFace.normal);
        for (uint32_t i = 0; i < numPoints; ++i) {
            XMVECTOR start = XMLoadFloat3(&refFace.points[i]);
            XMVECTOR end = XMLoadFloat3(&refFace.points[(i + 1) % numPoints]);

            XMVECTOR edge = end - start;

            XMVECTOR sideNormal = XMVector3Normalize(XMVector3Cross(refFaceNormal, edge));

            // sideNormal이 항상 밖을 향하도록 보정
            if (VecDot(start - refCenter, sideNormal) < 0)
                sideNormal *= -1.0f;

            float sideDist = VecDot(sideNormal, start);
            ClipPolygonAgainstPlane(contactFace, sideNormal, sideDist);

            if (contactFace.numPoints == 0)
                break;
        }
    }

    void Contact::ClipPolygonAgainstPlane(ContactFace& contactFace, const XMVECTOR& planeNormal, float planeDist)
    {
        // contactFace를 평면으로 자르는 함수
        // 점(planeNormal * planeDist)로 부터 planeNormal 방향의 점들을 탈락시킨다.
        int32_t numPoints = contactFace.numPoints;
        if (numPoints == 0)
            return;

        int32_t idx = 0;

        for (size_t i = 0; i < numPoints; i++) {
            const XMVECTOR curr = XMLoadFloat3(&contactFace.points[i]);
            const XMVECTOR next = XMLoadFloat3(&contactFace.points[(i + 1) % numPoints]);

            float distCurr = VecDot(planeNormal, curr) - planeDist;
            float distNext = VecDot(planeNormal, next) - planeDist;

            bool currInside = (distCurr <= EPS_FLOAT);
            bool nextInside = (distNext <= EPS_FLOAT);

            // 1. 둘 다 안쪽: 다음 점 추가
            if (currInside && nextInside) {
                XMStoreFloat3(&contactFace.buffer[idx++], next);
            }
            // 2. 안쪽 -> 바깥쪽: 교차점 추가 (다음 점은 버림)
            else if (currInside && !nextInside) {
                float t = distCurr / (distCurr - distNext);
                XMVECTOR intersect = curr + t * (next - curr);
                XMStoreFloat3(&contactFace.buffer[idx++], intersect);
            }
            // 3. 바깥쪽 -> 안쪽: 교차점 추가 후 다음 점 추가
            else if (!currInside && nextInside) {
                float t = distCurr / (distCurr - distNext);
                XMVECTOR intersect = curr + t * (next - curr);
                XMStoreFloat3(&contactFace.buffer[idx++], intersect);
                XMStoreFloat3(&contactFace.buffer[idx++], next);
            }
            // 4. 둘 다 바깥쪽: 아무것도 추가 안 함
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

        const XMVECTOR refNormal = XMLoadFloat3(&refFace.normal);

        // [수정 핵심] 항상 Reference Face의 법선 방향으로 투영해야 올바른 표면 위치를 찾을 수 있습니다.
        // refNormal은 Reference Body의 바깥쪽을 향하므로, 침투된 지점(Inc)에서 표면(Ref)으로 복구하는 올바른 방향입니다.
        const XMVECTOR normal = refNormal;

        const float refPlaneDist = refFace.distance;

        for (int32_t i = 0; i < numPoints; ++i) {
            const XMVECTOR pointB = XMLoadFloat3(&contactFace.points[i]);

            const float separation = VecDot(pointB, refNormal) - refPlaneDist;
            const float penetration = -separation;

            if (penetration <= EPS_FLOAT_SQ) {
                continue;
            }

            // 수정된 normal(refNormal)을 사용하여 투영
            const XMVECTOR pointA = XMVectorMultiplyAdd(
                normal,
                XMVectorReplicate(penetration),
                pointB
            );

            XMStoreFloat3(&collisionInfo.normal[collisionInfo.size], refNormal);
            XMStoreFloat3(&collisionInfo.pointA[collisionInfo.size], pointA); // Ref Face 위의 점
            XMStoreFloat3(&collisionInfo.pointB[collisionInfo.size], pointB); // Inc Face 위의 점 (침투된 상태)
            collisionInfo.separation[collisionInfo.size] = penetration;
            ++collisionInfo.size;
        }
    }

    void Contact::SortVerticesClockwise(XMFLOAT3* vertices, const XMVECTOR& center, const XMVECTOR& normal, uint32_t verticesSize)
    {
        XMVECTOR u = XMVector3Normalize(XMVector3Cross(normal, XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f)));
        if (XMVectorGetX(XMVector3LengthSq(u)) < EPS_FLOAT_SQ) {
            u = XMVector3Normalize(XMVector3Cross(normal, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)));
        }
        XMVECTOR v = XMVector3Normalize(XMVector3Cross(normal, u));

        auto static angleComparator = [&center, &u, &v](const XMFLOAT3& a, const XMFLOAT3& b) {
            XMVECTOR dA = XMLoadFloat3(&a) - center;
            XMVECTOR dB = XMLoadFloat3(&b) - center;

            float angleA = atan2(VecDot(dA, v), VecDot(dA, u));
            float angleB = atan2(VecDot(dB, v), VecDot(dB, u));

            return angleA > angleB;
            };

        std::sort(vertices, vertices + verticesSize, angleComparator);
    }

    void Contact::SetBoxFace(Face& face, const ConvexInfo& box, const XMVECTOR& normal)
    {
        const XMVECTOR axes[3] = { XMLoadFloat3(&box.axes[0]), XMLoadFloat3(&box.axes[1]) , XMLoadFloat3(&box.axes[2]) };
        const float hs[3] = { box.halfSize.x, box.halfSize.y, box.halfSize.z };

        float vecDots[3] = { VecDot(axes[0], normal), VecDot(axes[1], normal), VecDot(axes[2], normal) };
        float absVecDots[3] = { fabsf(vecDots[0]), fabsf(vecDots[1]), fabsf(vecDots[2]) };

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

        float sign = (vecDots[base] >= 0.0f) ? 1.0f : -1.0f;
        XMVECTOR baseAxis = axes[base] * sign;

        XMStoreFloat3(&face.normal, XMVector3Normalize(baseAxis));

        constexpr static const uint8_t NEXT1[3] = { 1, 2, 0 };
        constexpr static const uint8_t NEXT2[3] = { 2, 0, 1 };

        int i1 = NEXT1[base];
        int i2 = NEXT2[base];

        XMVECTOR h1 = axes[i1] * hs[i1];
        XMVECTOR h2 = axes[i2] * hs[i2];

        XMVECTOR basePoint = XMLoadFloat3(&box.center) + baseAxis * hs[base];
        XMStoreFloat3(&face.center, basePoint);

        face.distance = VecDot(basePoint, baseAxis);

        face.numPoints = 4;
        XMStoreFloat3(&face.points[0], basePoint - h1 - h2);
        XMStoreFloat3(&face.points[1], basePoint + h1 - h2);
        XMStoreFloat3(&face.points[2], basePoint + h1 + h2);
        XMStoreFloat3(&face.points[3], basePoint - h1 + h2);
    }

    void Contact::SetCylinderFace(Face& face, const ConvexInfo& cylinder, const XMVECTOR& normal)
    {
        const XMVECTOR axis = XMLoadFloat3(&cylinder.axes[0]); // Cylinder Up Axis (Y)
        const float radius = cylinder.radius;
        const float halfHeight = cylinder.height * 0.5f;

        // 1. Cap(뚜껑) vs Side(옆면) 판별
        float dot = VecDot(axis, normal);
        // 모서리 방향(Corner Direction)을 기준으로 임계값(Limit) 설정
        // (반지름과 반높이의 비율을 고려하여 45도가 아닌 실제 모서리 각도 사용)
        float limit = halfHeight / sqrtf(radius * radius + halfHeight * halfHeight);
        if (fabsf(dot) > limit) {
            // cylinder의 뚜껑 생성
            float sign = (dot >= 0.0f) ? 1.0f : -1.0f;
            XMVECTOR faceNormal = axis * sign;
            XMStoreFloat3(&face.normal, faceNormal);

            // 중심점: 원기둥 중심에서 축 방향으로 끝까지 이동
            XMVECTOR capCenter = XMLoadFloat3(&cylinder.center) + faceNormal * halfHeight;
            XMStoreFloat3(&face.center, capCenter);
            face.distance = VecDot(capCenter, faceNormal);

            // 원 생성: 축에 수직인 두 기저 벡터(Basis Vectors) 생성
            // (임의의 벡터와 외적하여 수직 벡터를 찾음)
            XMVECTOR right;
            if (fabsf(XMVectorGetX(faceNormal)) < 0.9f) {
                right = XMVector3Cross(faceNormal, XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));
            }
            else {
                right = XMVector3Cross(faceNormal, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
            }
            right = XMVector3Normalize(right);
            XMVECTOR forward = XMVector3Cross(faceNormal, right);

            // 세그먼트 수만큼 회전하며 정점 생성
            constexpr int segments = 20;
            face.numPoints = segments;
            float angleStep = XM_2PI / static_cast<float>(segments);

            for (int i = 0; i < segments; ++i) {
                float angle = i * angleStep;
                float c = cosf(angle);
                float s = sinf(angle);

                // Circle Vertex: Center + (Right * cos) + (Forward * sin) * Radius
                XMVECTOR p = capCenter + (right * c * radius) + (forward * s * radius);
                XMStoreFloat3(&face.points[i], p);
            }
        }
        else {
            // cylinder의 가상의 옆면 생성 (가상의 면은 실린더의 높이 x 반지름)
            // 법선에서 축 성분을 제거하여 완벽한 수평 법선(Radial Normal) 생성
            XMVECTOR sideNormal = normal - axis * dot;

            // 안전장치: 법선 길이가 너무 짧으면 기존 normal 사용
            if (XMVectorGetX(XMVector3LengthSq(sideNormal)) > 1.0e-6f) {
                sideNormal = XMVector3Normalize(sideNormal);
            }
            else {
                sideNormal = normal;
            }
            XMStoreFloat3(&face.normal, sideNormal);

            // 표면 중심: 원기둥 중심에서 옆면 법선 방향으로 반지름만큼 이동
            XMVECTOR surfaceCenter = XMLoadFloat3(&cylinder.center) + sideNormal * radius;
            XMStoreFloat3(&face.center, surfaceCenter);
            face.distance = VecDot(surfaceCenter, sideNormal);

            // 가상 직사각형 생성을 위한 벡터
            // upDir: 원기둥 높이 방향
            // rightDir: 접평면의 가로 방향 (축과 법선의 외적)
            XMVECTOR upDir = axis * halfHeight;
            XMVECTOR rightDir = XMVector3Cross(axis, sideNormal) * radius; // 폭을 반지름 정도로 설정

            face.numPoints = 4;
            // 충돌 지점을 중심으로 하는 직사각형 생성 (Sutherland-Hodgman 클리핑용)
            XMStoreFloat3(&face.points[0], surfaceCenter + upDir - rightDir); // Top-Left
            XMStoreFloat3(&face.points[1], surfaceCenter + upDir + rightDir); // Top-Right
            XMStoreFloat3(&face.points[2], surfaceCenter - upDir + rightDir); // Bottom-Right
            XMStoreFloat3(&face.points[3], surfaceCenter - upDir - rightDir); // Bottom-Left
        }
    }

    void Contact::SetCapsuleFace(Face& face, const ConvexInfo& capsule, const XMVECTOR& normal)
    {
        XMVECTOR axis = XMLoadFloat3(&capsule.axes[0]); // Up Axis (Y)
        XMVECTOR center = XMLoadFloat3(&capsule.center);

        float radius = capsule.radius;
        float halfHeight = capsule.height * 0.5f; // 실린더 부분의 절반 높이 (반구 제외)

        // 법선과 축의 내적 (CosTheta)
        float dot = XMVectorGetX(XMVector3Dot(axis, normal));
        float absDot = fabsf(dot);

        // ---------------------------------------------------
        // CASE 1: Side (옆면, 원기둥 몸통)
        // 법선이 축과 수직에 가까운 경우 (내적값이 작음)
        // ---------------------------------------------------
        if (absDot < 0.99f) {
            // 1. 순수한 측면 법선 계산 (축 성분 제거 및 정규화)
            XMVECTOR sideNormal = normal - axis * dot;
            sideNormal = XMVector3Normalize(sideNormal);

            XMStoreFloat3(&face.normal, sideNormal);

            // 2. 표면 중심점: 캡슐 중심에서 측면 법선 방향으로 반지름만큼 이동
            XMVECTOR surfaceCenter = center + sideNormal * radius;
            XMStoreFloat3(&face.center, surfaceCenter);
            face.distance = XMVectorGetX(XMVector3Dot(surfaceCenter, sideNormal));

            // 3. 가상 직사각형 생성
            // Up 벡터: 캡슐의 축 방향 (길이는 실린더 높이만큼)
            // Right 벡터: 측면 법선과 축의 외적 (폭은 반지름 정도로 설정)
            XMVECTOR upDir = axis * halfHeight;
            XMVECTOR rightDir = XMVector3Cross(axis, sideNormal) * radius;

            face.numPoints = 4;
            XMStoreFloat3(&face.points[0], surfaceCenter + upDir - rightDir); // Top-Left
            XMStoreFloat3(&face.points[1], surfaceCenter + upDir + rightDir); // Top-Right
            XMStoreFloat3(&face.points[2], surfaceCenter - upDir + rightDir); // Bottom-Right
            XMStoreFloat3(&face.points[3], surfaceCenter - upDir - rightDir); // Bottom-Left
        }
        // ---------------------------------------------------
        // CASE 2: Cap (뚜껑, 반구의 극점)
        // 법선이 축과 거의 평행한 경우
        // ---------------------------------------------------
        else {
            // 1. 방향 결정 (위쪽 뚜껑인지 아래쪽 뚜껑인지)
            float sign = (dot >= 0.0f) ? 1.0f : -1.0f;
            XMVECTOR capNormal = axis * sign;
            XMStoreFloat3(&face.normal, capNormal);

            // 2. 극점(Pole) 계산: 캡슐 중심 + (실린더 반높이 + 반지름) * 축방향
            // 캡슐의 가장 끝점입니다.
            XMVECTOR polePoint = center + capNormal * (halfHeight + radius);
            XMStoreFloat3(&face.center, polePoint);
            face.distance = XMVectorGetX(XMVector3Dot(polePoint, capNormal));

            // 3. 접평면 생성 (작은 쿼드)
            // 반구의 끝점은 '점'이지만, 클리핑 안정성을 위해 작은 사각형으로 만듭니다.
            // 축에 수직인 임의의 기저 벡터 생성
            XMVECTOR right, forward;
            XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
            XMVECTOR worldRight = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);

            if (fabsf(XMVectorGetX(capNormal)) < 0.9f) // 법선이 X축이 아니면
                right = XMVector3Cross(capNormal, worldRight);
            else
                right = XMVector3Cross(capNormal, worldUp);

            right = XMVector3Normalize(right);
            forward = XMVector3Cross(capNormal, right); // 이미 수직이므로 정규화 불필요

            // 크기는 반지름의 절반 정도로 설정 (너무 작으면 불안정할 수 있음)
            float size = radius * 0.5f;
            right *= size;
            forward *= size;

            face.numPoints = 4;
            XMStoreFloat3(&face.points[0], polePoint - right + forward);
            XMStoreFloat3(&face.points[1], polePoint + right + forward);
            XMStoreFloat3(&face.points[2], polePoint + right - forward);
            XMStoreFloat3(&face.points[3], polePoint - right - forward);
        }
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
        // delete[] convexA.points;
        // delete[] convexB.points;
    }

} // naemspace spe
