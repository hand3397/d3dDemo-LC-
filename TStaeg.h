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
    
    void OccupyTiles(const uint32_t numUnits, const vector<uint32_t>& unitIDs, 
        int tileIndex, const vector<pair<int, int>>& oldXZ, vector<pair<int, int>>& newXZ);

    // 필드 요청: 있으면 반환, 없으면 생성
    const vector<Nav::Dir>& RequestFlowField(int tx, int tz);
    void ReleaseFlowField(int tx, int tz);

    float GetTileHeight(int tx, int tz) const;
    float GetTileCountX() const;
    float GetTileCountZ() const;
    float GetTileSize() const;
    XMFLOAT3 GetTileOffset() const;
    XMFLOAT3 GetTileCenter(int tx, int tz) const;

    inline int32_t GetTileIndex(int tx, int tz) const;
    inline pair<int, int> GetTileIndexXZ(int tileIndex) const;
    int GetTileIndexFromWorldPos(const XMFLOAT3& worldPos) const;
    XMFLOAT3 GetWorldPosFromTileIndex(int tx, int tz) const;

private:

    void CreateFlowFields(int tx, int tz, FlowField& newField);

    // 타일 점유 관리: 유닛이 타일을 점유하려고 시도할 때, 해당 타일이 비어있고 이동 가능한지 검사 후 점유 처리
    bool TryOccupyTile(uint32_t unitID, int tx, int tz);
    bool UpdateOccupancy(uint32_t unitID, int oldX, int oldZ, int newX, int newZ);
    void ReleaseTile(int tx, int tz);

private:

    vector<float> tileHeightMap_; // 높이 정보 (x, z)
    vector<NavEdges> navEdges_; // 타일마다 8방향의 지형 정보를 16비트로 압축하여 저장 (x, z)

    unordered_map<uint32_t, FlowField> flowFields_; // 목적지 타일 인덱스 -> FlowField (목적지 타일마다 하나씩 존재, 분대들이 공유)

    vector<uint32_t> occupantIDs_; // 타일에 현재 점유하고 있는 유닛의 ID (없으면 INVALID_ID == 0)

    float tileSize_ = 0.2f;
    uint32_t tileCountX_ = 0;
    uint32_t tileCountZ_ = 0;
    XMFLOAT3 tileOffset_ = { 0.f, 0.f, 0.f }; // 타일의 월드 좌표에서의 오프셋 

    // 타일 개수만큼 할당해두고 영구히 재사용하는 방문 기록 배열
    vector<uint32_t> visitedTickets_;
    uint32_t currentTicket_ = 0; // 매 BFS마다 1씩 증가 (초기화 비용 0)
};

