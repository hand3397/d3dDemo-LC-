#include "MeshCollision.h"

XMVECTOR MinkowskiSupport(const Collider& A, const Collider& B, const XMVECTOR& dir)
{
    return (A.FindFurthestPoint(dir) - B.FindFurthestPoint(-dir));
}

bool SameDirection(const XMVECTOR& direction, const XMVECTOR& ao)
{
    return VecDot(direction, ao) > feps;
}

bool LineSimplex(Simplex& points, XMVECTOR& direction)
{
    XMVECTOR a = points[0];
    XMVECTOR b = points[1];

    XMVECTOR ab = b - a;
    XMVECTOR ao = -a;

    if (SameDirection(ab, ao)) {
        direction = XMVector3Cross(XMVector3Cross(ab, ao), ab);
    }
    else {
        points = { a };
        direction = ao;
    }

    return false;
}

bool TriangleSimplex(Simplex& points, XMVECTOR& direction)
{
    XMVECTOR a = points[0];
    XMVECTOR b = points[1];
    XMVECTOR c = points[2];

    XMVECTOR ab = b - a;
    XMVECTOR ac = c - a;
    XMVECTOR ao = -a;

    XMVECTOR abc = XMVector3Cross(ab, ac);
    // 원점이 선분 bc밖에 있는 경우는 Line단계에서 걸러진다.

    // 원점이 선분 ac밖에 존재
    if (SameDirection(XMVector3Cross(abc, ac), ao)) {
        // b 버리기
        if (SameDirection(ac, ao)) {
            points = { a, c };
            direction = XMVector3Cross(XMVector3Cross(ac, ao), ac);
        }
        // c 버리기
        else {
            return LineSimplex(points = { a, b }, direction);
        }
    }
    else {
        // 원점이 선분 ab밖에 존재
        if (SameDirection(XMVector3Cross(ab, abc), ao)) {
            return LineSimplex(points = { a, b }, direction);
        }
        // 원점이 삼각형 abc안에 존재
        else {
            // 원점이 삼각형 abc 위에 존재하는지 아래 존재하는지에 따라 다음 support를 구할 direction의 방향을 결정한다.
            if (SameDirection(abc, ao)) {
                direction = abc;
            }
            else {
                points = { a, c, b };
                direction = -abc;
            }
        }
    }

    return false;
}

bool TetrahedronSimplex(Simplex& points, XMVECTOR& direction)
{
    XMVECTOR a = points[0];
    XMVECTOR b = points[1];
    XMVECTOR c = points[2];
    XMVECTOR d = points[3];

    XMVECTOR ab = b - a;
    XMVECTOR ac = c - a;
    XMVECTOR ad = d - a;
    XMVECTOR ao = -a;

    XMVECTOR abc = XMVector3Cross(ab, ac);
    XMVECTOR acd = XMVector3Cross(ac, ad);
    XMVECTOR adb = XMVector3Cross(ad, ab);
    // 원점이 삼각형 bcd 밖에 존재하는 경우는 Triangle단계에서 걸러진다.

    // 원점이 삼각형 abc 밖에 존재함 -> d를 다시 선정한다.
    if (SameDirection(abc, ao)) {
        return TriangleSimplex(points = { a, b, c }, direction);
    }

    // 원점이 삼각형 acd 밖에 존재함 -> b를 다시 선정한다.
    if (SameDirection(acd, ao)) {
        return TriangleSimplex(points = { a, c, d }, direction);
    }

    // 원점이 삼각형 adb 밖에 존재함 -> c를 다시 선정한다.
    if (SameDirection(adb, ao)) {
        return TriangleSimplex(points = { a, d, b }, direction);
    }

    // 원점이 사면체 abcd안에 존재한다.
    return true;
}

bool NextSimplex(Simplex& simplex, XMVECTOR& direction)
{
    switch (simplex.size()) {
    case 2: return LineSimplex(simplex, direction);
    case 3: return TriangleSimplex(simplex, direction);
        // 3차원 충돌만 계산하므로 Tetrahedron에서만 true를 반환할 수 있다.
    case 4: return TetrahedronSimplex(simplex, direction);
    }

    // never should be here
    return false;
}

bool GJK(Simplex& simplex, const Collider& colliderA, const Collider& colliderB)
{
    // simplex의 첫점을 x 벡터를 통해 구함
    XMVECTOR support = MinkowskiSupport(colliderA, colliderB, XMVectorSet(1.f, 0.f, 0.f, 0.f));

    simplex.push_front(support);

    // New direction is towards the origin
    XMVECTOR direction = -support;

    while (true) {
        support = MinkowskiSupport(colliderA, colliderB, direction);

        // support와 direction의 내적 값이 0보다 작으면 두점 사이에 원점이 포함 되지 않는다. 
        if (VecDot(support, direction) < feps) {
            return false; // no collision
        }

        simplex.push_front(support);

        if (NextSimplex(simplex, direction)) {
            return true;
        }
    }

    return false;
}

std::tuple<std::vector<XMVECTOR>, std::vector<float>, size_t> GetFaceNormals(
    const std::vector<XMVECTOR>& polytope,
    const std::vector<size_t>& faces)
{
    std::vector<XMVECTOR> normals;
    std::vector<float> distances;
    size_t minTriangle = 0;
    float  minDistance = FLT_MAX;

    size_t numFaces = faces.size() / 3;
    normals.reserve(numFaces);
    distances.reserve(numFaces);

    for (size_t i = 0; i < faces.size(); i += 3) {
        XMVECTOR a = polytope[faces[i]];
        XMVECTOR b = polytope[faces[i + 1]];
        XMVECTOR c = polytope[faces[i + 2]];

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

void AddIfUniqueEdge(
    std::vector<std::pair<size_t, size_t>>& edges,
    const std::vector<size_t>& faces,
    size_t a,
    size_t b)
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

pair<XMVECTOR, float> EPA(Simplex& simplex, const Collider& colliderA, const Collider& colliderB)
{
    // simplex는 항상 4개의 점을 가진 원점을 내부에 포함한 사면체가 들어와야한다.
    std::vector<XMVECTOR> polytope(simplex.begin(), simplex.end());
    std::vector<size_t> faces = {
        0, 1, 2,
        0, 3, 1,
        0, 2, 3,
        1, 3, 2
    };

    // list: XMVECTOR(normal, distance), index: min distance
    auto [normals, distances, minFace] = GetFaceNormals(polytope, faces);

    XMVECTOR  minNormal = XMVectorZero();
    float minDistance = FLT_MAX;

    while (minDistance == FLT_MAX) {
        // 현재 면중 원점과 가장 가까운 면의 노말값을 이용해 해당 방향으로 support를 구한뒤 polytope를 확장한다.
        minNormal = normals[minFace];
        minDistance = distances[minFace];

        XMVECTOR support = MinkowskiSupport(colliderA, colliderB, minNormal);
        float sDistance = VecDot(minNormal, support);

        if (sDistance <= minDistance)
            break;

        // 새로구한 support의 거리가 polytope의 면 최소거리보다 짧거나 같다면 루프를 종료한다.
        if (sDistance > minDistance) {
            minDistance = FLT_MAX;

            std::vector<std::pair<size_t, size_t>> uniqueEdges;

            // support에서 보이는 모든면을 삭제한다.
            for (size_t i = 0; i < normals.size(); i++) {
                if (SameDirection(normals[i], support - polytope[faces[i * 3]])) {
                    size_t f = i * 3;

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

            std::vector<size_t> newFaces;
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

    return { minNormal, minDistance };
}