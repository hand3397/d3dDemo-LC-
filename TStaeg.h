#pragma once
#include <vector>
#include <DirectXMath.h>
#include <unordered_map>
#include "NavEdges.h"
#include "NavDir.h"

using namespace std;
using namespace DirectX;

struct FlowField
{
    uint16_t targetTileX;
    uint16_t targetTileZ;
    vector<Nav::Dir> directions;
    int refCount = 0; // 이 필드를 사용하는 분대 수
};
// 타일 -> block -> stage의 구조로 스테이지가 구성된다.

class TStaeg
{
public:

    TStaeg();
    ~TStaeg();
    
    // 필드 요청: 있으면 반환, 없으면 생성
    const vector<Nav::Dir>& RequestFlowField(int tx, int tz);

    // 사용 종료 알림
    void ReleaseFlowField(int tx, int tz);

    float GetTileHeight(int tx, int tz) const;
    float GetTileCountX() const;
    float GetTileCountZ() const;
    float GetTileSize() const;
    XMFLOAT3 GetTileOffset() const;

    inline int32_t GetTileIndex(int tx, int tz) const;
    int GetTileIndexFromWorldPos(const XMFLOAT3& worldPos) const;

private:

    void CreateFlowFields(int tx, int tz, FlowField& newField);

    vector<float> tileHeightMap_; // 높이 정보 (x, z)
    vector<NavEdges> navEdges_;

    unordered_map<uint32_t, FlowField> flowFields_;

    float tileSize_ = 0.2f;
    uint32_t tileCountX_ = 0;
    uint32_t tileCountZ_ = 0;
    XMFLOAT3 tileOffset_ = { 0.f, 0.f, 0.f }; // 타일의 월드 좌표에서의 오프셋 
};

