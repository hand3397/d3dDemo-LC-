#include "TStaeg.h"
#include <queue>

TStaeg::TStaeg()
{
    tileCountX_ = 20;
    tileCountZ_ = 20;
    tileOffset_ = XMFLOAT3(-tileSize_ * tileCountX_ * 0.5f, 0.f, -tileSize_ * tileCountZ_ * 0.5f);

    tileHeightMap_.resize(tileCountX_ * tileCountZ_, 1.0f);
    navEdges_.resize(tileCountX_ * tileCountZ_);
    for (int i = 0; i < 10; i++) {
        tileHeightMap_[GetTileIndex(i, 9)] = 2.0f;
        tileHeightMap_[GetTileIndex(i, 10)] = 2.0f;
    }

    // 모든 타일을 순회하며 NavEdges 베이킹
    for (int z = 0; z < tileCountZ_; ++z) {
        for (int x = 0; x < tileCountX_; ++x) {
            int currentIndex = GetTileIndex(x, z);
            float currentHeight = tileHeightMap_[currentIndex];

            // 주변 8방향 검사
            for (int d = 0; d < 8; ++d) {
                Nav::Dir currentDir = static_cast<Nav::Dir>(d);
                int nx = x + Nav::dx[d];
                int nz = z + Nav::dz[d];

                // [규칙 1] 맵 경계를 벗어나는가?
                if (nx < 0 || nx >= tileCountX_ || nz < 0 || nz >= tileCountZ_) {
                    navEdges_[currentIndex].SetEdge(currentDir, NavEdges::Type::Blocked);
                    continue;
                }

                int neighborIndex = GetTileIndex(nx, nz);
                float neighborHeight = tileHeightMap_[neighborIndex];

                // [규칙 2] 높이 차이가 등반 가능한 수준인가? (임계값: 0.5f로 가정)
                float heightDiff = std::abs(currentHeight - neighborHeight);

                if (heightDiff > 0.5f) {
                    // 높이 차이가 너무 커서 절벽/벽으로 판정
                    navEdges_[currentIndex].SetEdge(currentDir, NavEdges::Type::Blocked);
                }
                else {
                    // 평지이거나 완만한 경사라서 이동 가능
                    navEdges_[currentIndex].SetEdge(currentDir, NavEdges::Type::Flat);
                }

                if (d % 2 != 0) // 대각선 방향(1, 3, 5, 7)인 경우에만 추가 검사
                {
                    // 대각선을 구성하는 두 개의 직교 방향 (예: NE(1)의 경우 N(0)과 E(2))
                    int dir1 = (d - 1) % 8;
                    int dir2 = (d + 1) % 8;

                    // 이미 이전 루프에서 검사된 값(또는 별도 계산)을 통해 양옆이 벽인지 확인
                    int nx1 = x + Nav::dx[dir1]; int nz1 = z + Nav::dz[dir1];
                    int nx2 = x + Nav::dx[dir2]; int nz2 = z + Nav::dz[dir2];

                    // 둘 중 하나라도 높이가 달라서 막혀있다면, 대각선 통과도 불가능 처리!
                    bool isWall1 = (nx1 >= 0 && nx1 < tileCountX_ && nz1 >= 0 && nz1 < tileCountZ_) && (std::abs(currentHeight - tileHeightMap_[GetTileIndex(nx1, nz1)]) > 0.5f);
                    bool isWall2 = (nx2 >= 0 && nx2 < tileCountX_ && nz2 >= 0 && nz2 < tileCountZ_) && (std::abs(currentHeight - tileHeightMap_[GetTileIndex(nx2, nz2)]) > 0.5f);

                    if (isWall1 || isWall2) {
                        navEdges_[currentIndex].SetEdge(currentDir, NavEdges::Type::Blocked);
                    }
                }
            }
        }
    }
}

TStaeg::~TStaeg()
{
}

const vector<Nav::Dir>& TStaeg::RequestFlowField(int tx, int tz)
{
    uint32_t idx = GetTileIndex(tx, tz);

    if (flowFields_.find(idx) == flowFields_.end()) {
        // 새 목적지라면 BFS/Dijkstra로 필드 생성
        flowFields_[idx] = FlowField();
        CreateFlowFields(tx, tz, flowFields_[idx]);
    }

    flowFields_[idx].refCount++;
    return flowFields_[idx].directions;
}

void TStaeg::ReleaseFlowField(int tx, int tz)
{
    uint32_t idx = GetTileIndex(tx, tz);
    if (--flowFields_[idx].refCount <= 0) {
        flowFields_.erase(idx); // 아무도 안 쓰면 삭제[cite: 1]
    }
}

float TStaeg::GetTileHeight(int tx, int tz) const
{
    if (tx < 0 || tz < 0 || tx >= tileCountX_ || tz >= tileCountZ_)
        return -1.0f; // 범위를 벗어난 경우 -1 반환
    return tileHeightMap_[GetTileIndex(tx, tz)];
}

float TStaeg::GetTileCountX() const
{
    return tileCountX_;
}

float TStaeg::GetTileCountZ() const
{
    return tileCountZ_;
}

float TStaeg::GetTileSize() const
{
    return tileSize_;
}

XMFLOAT3 TStaeg::GetTileOffset() const
{
    return tileOffset_;
}

inline int32_t TStaeg::GetTileIndex(int tx, int tz) const
{
    if (tx < 0 || tz < 0 || tx >= tileCountX_ || tz >= tileCountZ_)
        return -1;
    return tz * tileCountX_ + tx;
}

int TStaeg::GetTileIndexFromWorldPos(const XMFLOAT3& worldPos) const
{
    float localX = worldPos.x - tileOffset_.x - tileSize_ * 0.5f;
    float localZ = worldPos.z - tileOffset_.z - tileSize_ * 0.5f;

    int tx = static_cast<int>(localX / tileSize_);
    int tz = static_cast<int>(localZ / tileSize_);

    if (tx < 0 || tx >= tileCountX_ || tz < 0 || tz >= tileCountZ_) {
        return -1; // 맵을 벗어남 (Invalid Index)
    }

    return GetTileIndex(tx, tz);
}

void TStaeg::CreateFlowFields(int tx, int tz, FlowField& newField)
{
    newField.targetTileX = tx;
    newField.targetTileZ = tz;
    newField.refCount = 0; // 아직 참조하는 유닛 없음

    int totalTiles = tileCountX_ * tileCountZ_;

    // 방향 배열 초기화: 모든 타일의 방향을 NONE으로 시작
    newField.directions.assign(totalTiles, Nav::Dir::NONE);

    // Integration Field (목적지로부터의 누적 비용 계산)
    vector<float> integrationField(totalTiles, 10000000.0f); // 무한대에 가까운 값으로 초기화

    // 우선순위 큐: {누적 비용, 타일 인덱스} -> 비용이 적은 것부터 뽑기 위해 Min-Heap 사용
    priority_queue<pair<float, int>, vector<pair<float, int>>, greater<>> pq;

    int targetIndex = GetTileIndex(tx, tz);
    integrationField[targetIndex] = 0.0f;
    pq.push({ 0.0f, targetIndex });

    while (!pq.empty()) {
        float currentCost = pq.top().first;
        int currentIndex = pq.top().second;
        pq.pop();

        // 큐에서 꺼낸 비용이 이미 기록된 최솟값보다 크면 무시
        if (currentCost > integrationField[currentIndex]) 
            continue;

        int cx = currentIndex % tileCountX_;
        int cz = currentIndex / tileCountX_;

        // 8방향 이웃 탐색 (목적지에서 바깥으로 퍼져나감 - Reverse Dijkstra)
        for (int d = 0; d < 8; ++d) {
            Nav::Dir dir = static_cast<Nav::Dir>(d);

            // [엔진 설계 포인트] 목적지에서 출발지 방향으로 역탐색 중이므로,
            // 이웃 타일에서 현재 타일로 "들어올 수 있는지"를 확인해야 함.
            // 양방향 통행이 보장된 평면 맵이라면 그냥 currentIndex의 NavEdges를 써도 무방하다.
            if (navEdges_[currentIndex].GetEdge(dir) == NavEdges::Type::Blocked) {
                continue;
            }

            int nx = cx + Nav::dx[d];
            int nz = cz + Nav::dz[d];

            // 맵 경계 체크 (베이킹 시 벽으로 막아뒀다면 생략 가능하지만 안전을 위해 추가)
            if (nx < 0 || nx >= tileCountX_ || nz < 0 || nz >= tileCountZ_) 
                continue;

            int neighborIndex = GetTileIndex(nx, nz);

            // 이동 가중치: 십자 방향(0,2,4,6)은 1.0, 대각선(1,3,5,7)은 1.414 (루트 2)
            float moveCost = (d % 2 == 0) ? 1.0f : 1.414f;
            float nextCost = currentCost + moveCost;

            // 더 적은 비용으로 도달할 수 있는 경로를 찾았다면 갱신
            if (nextCost < integrationField[neighborIndex]) {
                integrationField[neighborIndex] = nextCost;
                pq.push({ nextCost, neighborIndex });
            }
        }
    }

    // Vector Field (각 타일에서 이동해야 할 최적의 방향 결정)
    for (int i = 0; i < totalTiles; ++i) {
        // 목적지 자신이거나 도달 불가능한 타일(벽 등)은 방향을 계산하지 않음
        if (i == targetIndex || integrationField[i] >= 10000000.0f) {
            continue;
        }

        int cx = i % tileCountX_;
        int cz = i / tileCountX_;

        float minCost = integrationField[i];
        int bestDir = -1;

        // 현재 타일에서 8방향을 둘러보고, '가장 비용이 낮은(목적지와 가까운)' 이웃 타일의 방향을 선택
        for (int d = 0; d < 8; ++d) {
            Nav::Dir dir = static_cast<Nav::Dir>(d);

            // 해당 방향으로 지형이 막혀있다면 갈 수 없음
            if (navEdges_[i].GetEdge(dir) == NavEdges::Type::Blocked) {
                continue;
            }

            int nx = cx + Nav::dx[d];
            int nz = cz + Nav::dz[d];

            // 경계 체크
            if (nx < 0 || nx >= tileCountX_ || nz < 0 || nz >= tileCountZ_) 
                continue;

            int neighborIndex = GetTileIndex(nx, nz);

            // 이웃 타일의 누적 비용이 역대 최저라면 갱신
            if (integrationField[neighborIndex] < minCost) {
                minCost = integrationField[neighborIndex];
                bestDir = d;
            }
        }

        // 최적의 방향을 찾았다면 구조체 배열에 저장
        if (bestDir != -1) {
            newField.directions[i] = static_cast<Nav::Dir>(bestDir);
        }
    }
}
