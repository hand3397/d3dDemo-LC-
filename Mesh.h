#pragma once

#include "d3dUtil.h"

using namespace std;
using namespace DirectX;

struct Vertex 
{
    Vertex() = default;
    Vertex(const XMFLOAT3& p, const XMFLOAT3& n, const XMFLOAT2& t) :
        Pos(p), Normal(n), TexC(t) {}
    Vertex(float px, float py, float pz,
        float nx, float ny, float nz,
        float tx, float ty) :
        Pos({ px, py, pz }), Normal({ nx, ny,nz }), TexC({tx, ty}) {}

    XMFLOAT3 Pos = XMFLOAT3(0.0f, 0.0f, 0.0f);
    XMFLOAT3 Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);;
    XMFLOAT2 TexC = XMFLOAT2(0.0f, 0.0f);;
};

struct SkinedVertex : Vertex
{
    SkinedVertex() = default;

    XMFLOAT3 BoneWeights = XMFLOAT3(0.0f, 0.0f, 0.0f);
    BYTE BoneIndices[4] = {0, 0, 0, 0};
};

struct Submesh
{
    Submesh() = default;
    Submesh(uint32_t indexCount, uint32_t startIndexLocation, int baseVertexLocation, BoundingBox boundingBox) :
        IndexCount(indexCount), StartIndexLocation(startIndexLocation), BaseVertexLocation(baseVertexLocation), Bounds(boundingBox) {}

    uint32_t IndexCount = 0;
    uint32_t StartIndexLocation = 0;
    int BaseVertexLocation = 0;

    BoundingBox Bounds;
};

class Mesh
{
public:
    vector<Vertex> vertices;
    vector<uint32_t> indices;

    void clear() { 
        vertices.clear();
        indices.clear();
        indices16.clear();
    }
private:
    vector<uint16_t> indices16;
};

class SkinedMesh
{
public:
    vector<SkinedVertex> vertices;
    vector<uint32_t> indices;

    void clear()
    {
        vertices.clear();
        indices.clear();
        indices16.clear();
    }
private:
    vector<uint16_t> indices16;
};