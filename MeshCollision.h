#pragma once
#include "d3dUtil.h"

const float feps = 1e-6f;

static inline float VecDot(const XMVECTOR& a, const XMVECTOR& b) noexcept
{
    return XMVectorGetX(XMVector3Dot(a, b));
}

struct Collider
{
    virtual XMVECTOR FindFurthestPoint(const XMVECTOR& direction) const = 0;
};

struct MeshCollider : Collider
{
public:
    MeshCollider(std::initializer_list<XMVECTOR> vertices)
    {
        vertices_.reserve(vertices.size());
        for (const XMVECTOR& v : vertices)
            vertices_.push_back(v);
    };
    MeshCollider(const std::vector<XMFLOAT3>& vertices)
    {
        size_t numVertices = vertices.size();
        vertices_.resize(numVertices);
        for (size_t i = 0; i < numVertices; ++i)
            vertices_[i] = XMLoadFloat3(&vertices[i]);
    }
    MeshCollider(const XMFLOAT3 vertices[], size_t numVertices)
    {
        vertices_.resize(numVertices);
        for (size_t i = 0; i < numVertices; ++i)
            vertices_[i] = XMLoadFloat3(&vertices[i]);
    }

    XMVECTOR FindFurthestPoint(const XMVECTOR& direction) const override
    {
        XMVECTOR  maxPoint = XMVectorZero();
        float maxDistance = -FLT_MAX;

        for (const XMVECTOR& vertex : vertices_) {
            float distance = VecDot(vertex, direction);
            if (distance > maxDistance) {
                maxDistance = distance;
                maxPoint = vertex;
            }
        }

        return maxPoint;
    }
private:
    std::vector<XMVECTOR> vertices_;
};

struct SphereCollider : Collider
{
public:
    SphereCollider(const XMVECTOR& center, float radius) :
        center_(center), radius_(radius)
    {
    }

    XMVECTOR FindFurthestPoint(const XMVECTOR& direction) const override
    {
        return center_ + XMVector3NormalizeEst(direction) * radius_;
    }
private:
    XMVECTOR center_;
    float radius_;
};

struct Simplex
{
public:
    Simplex() {}

    Simplex& operator=(std::initializer_list<XMVECTOR> list)
    {
        size_ = 0;
        for (const XMVECTOR& point : list)
            points_[size_++] = point;

        return *this;
    }

    void push_front(const XMVECTOR& point)
    {
        points_ = { point, points_[0], points_[1], points_[2] };
        size_ = (size_ + 1 < 4 ? size_ + 1 : 4);
    }

    XMVECTOR& operator[](int i) { return points_[i]; }
    size_t size() const { return size_; }

    auto begin() const { return points_.begin(); }
    auto end() const { return points_.end() - (4 - size_); }

private:
    std::array<XMVECTOR, 4> points_;
    int size_ = 0;
};

struct CollisionPoints
{
    XMVECTOR normal_ = XMVectorZero();
    float penetrationDepth_ = 0.f;
    bool hasCollision_ = true;
};

XMVECTOR MinkowskiSupport(const Collider& A, const Collider& B, const XMVECTOR& dir);

bool SameDirection(const XMVECTOR& direction, const XMVECTOR& ao);

bool LineSimplex(Simplex& points, XMVECTOR& direction);

bool TriangleSimplex(Simplex& points, XMVECTOR& direction);

bool TetrahedronSimplex(Simplex& points, XMVECTOR& direction);

bool NextSimplex(Simplex& simplex, XMVECTOR& direction);

bool GJK(Simplex& simplex, const Collider& colliderA, const Collider& colliderB);

std::tuple<std::vector<XMVECTOR>, std::vector<float>, size_t> GetFaceNormals(
    const std::vector<XMVECTOR>& polytope,
    const std::vector<size_t>& faces);

void AddIfUniqueEdge(
    std::vector<std::pair<size_t, size_t>>& edges,
    const std::vector<size_t>& faces,
    size_t a, size_t b);;

pair<XMVECTOR, float> EPA(Simplex& simplex, const Collider& colliderA, const Collider& colliderB);



