#include "Stage.h"

Stage::Stage(uint32_t stageWidth, uint32_t stageLength, uint32_t stageHeight) :
    stageWidth_(stageWidth), stageLength_(stageLength), stageHeight_(stageHeight)
{
    // stage의 중심이 world의 중심이 되도록 하고, 바닥은 y = 0 평면에 맞춘다.
    blockOffset_ = { -blockSize_ * 0.5f * (stageWidth - 1), blockHeight_ * 0.5f, -blockSize_ * 0.5f * (stageLength - 1) };
    
    blocks_.clear();
    blocks_.resize(stageWidth_ * stageLength_ * stageHeight_, BlockType::BLOCK_TYPE_AIR);
    
    for (int dy = 0; dy < stageHeight; dy++) {
        for (int dx = 0; dx < stageWidth; dx++) {
            for (int dz = 0; dz < stageLength; dz++) {
                int dIndex = GetBlockIndex(dx, dy, dz);
                if (dx >= dy)
                    blocks_[dIndex] = BlockType::BLOCK_TYPE_GRASS;
            }
        }
    }
    
    blockToTextureIndex_.clear();
    // BlockType과 텍스처 인덱스 매핑 초기화 (top, side, bottom)
    blockToTextureIndex_.push_back({ -1, -1, -1 }); // BlockType::BLOCK_TYPE_AIR
    blockToTextureIndex_.push_back({ -1, -1, -1 }); // BlockType::BLOCK_TYPE_SAND
    blockToTextureIndex_.push_back({  0,  3,  1 }); // BlockType::BLOCK_TYPE_GRASS
    blockToTextureIndex_.push_back({ -1, -1, -1 }); // BlockType::BLOCK_TYPE_STONE
    blockToTextureIndex_.push_back({ -1, -1, -1 }); // BlockType::BLOCK_TYPE_SEA
    blockToTextureIndex_.push_back({  1,  3,  1 }); // BlockType::BLOCK_TYPE_HILL

    tiles_.clear();
    // 각 타일마다 캐릭터 슬롯 정보를 초기화한다. (스테이지에는 지하나 동굴이 없다 stageHeigt는 비포함)
    tiles_.resize(stageWidth_ * stageLength_);
}

Stage::~Stage()
{
    blocks_.clear();
    blockToTextureIndex_.clear();
    tiles_.clear();
}

void Stage::DeployTile(std::vector<XMFLOAT3>& targetPos, uint32_t numCharacters, int x, int z)
{
    // 실제 유닛이 배치될 월드 위치를 계산하여 targetPos에 채운다.
    targetPos.clear();
    
    if (x < 0 || z < 0 || x >= stageWidth_ || z >= stageLength_)
        return;

    uint32_t tileIndex = GetTileIndex(x, z);
    Tile& tile = tiles_[tileIndex];

    // 타일 하나에는 최대 STAGE_TILE_SIZE x STAGE_TILE_SIZE 만큼의 유닛이 배치될 수 있다. 이를 초과하는 경우는 배치하지 않는다.
    if (numCharacters > STAGE_TILE_SIZE * STAGE_TILE_SIZE)
        return;

    targetPos.reserve(numCharacters);
    uint32_t deployedCount = 0;
    vector<vector<bool>> visited(STAGE_TILE_SIZE, vector<bool>(STAGE_TILE_SIZE, false));
    XMFLOAT3 tilePos = GetTilePos(x, z);

    int head = 0;
    int tail = 0;

    const static pair<int, int> ddir[4] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };
    pair<int, int> q[STAGE_TILE_SIZE * STAGE_TILE_SIZE + 1];

    // 시작점 push
    int startXZ = (STAGE_TILE_SIZE - 1) / 2; // 중앙에서 시작

    // 시작점과 그 주변 3칸을 큐에 넣는다. (총 4칸)
    q[tail++] = { startXZ, startXZ }; // 값을 넣고 tail을 1 증가
    q[tail++] = { startXZ + 1, startXZ }; // 값을 넣고 tail을 1 증가
    q[tail++] = { startXZ, startXZ + 1 }; // 값을 넣고 tail을 1 증가
    q[tail++] = { startXZ + 1, startXZ + 1 }; // 값을 넣고 tail을 1 증가
    visited[startXZ][startXZ] = true;
    visited[startXZ + 1][startXZ] = true;
    visited[startXZ][startXZ + 1] = true;
    visited[startXZ + 1][startXZ + 1] = true;

    while (head < tail) {
        int tx = q[head].first;
        int tz = q[head].second;
        head++;

        int slotIndex = tz * STAGE_TILE_SIZE + tx;

        // 해당 타일에 유닛 배치
        if (tile.isWalkable[slotIndex] && !tile.slot[slotIndex]) {
            tile.slot[slotIndex] = true; // 슬롯을 채운다
            ++deployedCount;
            XMFLOAT3 slotOffset = GetTileSlotOffset(tx, tz);
            targetPos.emplace_back(tilePos.x + slotOffset.x, tilePos.y + slotOffset.y, tilePos.z + slotOffset.z);
            if (deployedCount == numCharacters) {
                tile.isEmpty = false; // 타일이 더 이상 비어있지 않음을 표시한다.
                return;
            }
        }

        for (int i = 0; i < 4; i++) {
            int dx = tx + ddir[i].first;
            int dz = tz + ddir[i].second;

            if (dx < 0 || dz < 0 || dx >= STAGE_TILE_SIZE || dz >= STAGE_TILE_SIZE)
                continue;

            if (visited[dx][dz])
                continue;

            visited[dx][dz] = true;
            q[tail++] = { dx, dz };
        }
    }

    return;
}

void Stage::UndeployTile(int x, int z)
{
    if (x < 0 || z < 0 || x >= stageWidth_ || z >= stageLength_)
        return;

    tiles_[GetTileIndex(x, z)] = Tile();
}

bool Stage::IsBlockSolid(int x, int y, int z) const
{
    if (x < 0 || x >= static_cast<int>(stageWidth_) ||
        y < 0 || y >= static_cast<int>(stageHeight_) ||
        z < 0 || z >= static_cast<int>(stageLength_)) {
        return false; // 범위를 벋어난 경우는 solid하지 않다고 간주한다.
    }
    else {
        BlockType blockType = blocks_[GetBlockIndex(x, y, z)];
        return blockType != BlockType::BLOCK_TYPE_AIR;
    }

    return false;
}

uint32_t Stage::GetStageWidth() const
{
    return stageWidth_;
}

uint32_t Stage::GetStageLength() const
{
    return stageLength_;
}

uint32_t Stage::GetStageHeight() const
{
    return stageHeight_;
}

float Stage::GetBlockSize() const
{
    return blockSize_;
}

float Stage::GetBlockHeight() const
{
    return blockHeight_;
}

inline uint32_t Stage::GetBlockIndex(int x, int y, int z) const
{
    return y* stageWidth_* stageLength_ + z * stageWidth_ + x;
}

inline uint32_t Stage::GetTileIndex(int x, int z) const
{
    return z * stageWidth_ + x;
}

BlockType Stage::GetBlockType(int x, int y, int z) const
{
    return blocks_[GetBlockIndex(x, y, z)];
}

int32_t Stage::GetTextureIndex(BlockType blockType, const std::string& face) const
{
    if (face == "up") {
        return blockToTextureIndex_[static_cast<uint16_t>(blockType)].up;
    }
    else if (face == "side") {
        return blockToTextureIndex_[static_cast<uint16_t>(blockType)].side;
    }
    else if (face == "bottom") {
        return blockToTextureIndex_[static_cast<uint16_t>(blockType)].bottom;
    }
    return -1; // Invalid face
}

XMFLOAT3 Stage::BlockOffset() const
{
    return blockOffset_;
}

// 각 타일의 중심을 반환
XMFLOAT3 Stage::GetTilePos(int x, int z) const
{
    float dx = x * blockSize_, dz = z * blockSize_;
    return XMFLOAT3(blockOffset_.x + dx, 0.f, blockOffset_.z + dz);
}

XMFLOAT3 Stage::GetTileSlotOffset(int x, int z) const
{
    float dSlot = blockSize_ / STAGE_TILE_SIZE;
    float slotOffset = -dSlot * 1.5f;
    return XMFLOAT3(slotOffset + dSlot * x, 0.f, slotOffset + dSlot * z);
}

