#pragma once
#include <cstdint>
#include <vector>
#include <map>
#include <string>
#include <DirectXMath.h>

using namespace DirectX;
using namespace std;

//
// 스테이지를 구성하는 메시 단위 = 블록 (x, y, z)
// 스테이지에서 캐릭터가 이동할 수 있는 지점 = 타일. (x, z)
//

constexpr uint32_t STAGE_TILE_SIZE = 4; // 타일 하나의 길이 (하나의 타일에는 최대 STAGE_TILE_SIZE x STAGE_TILE_SIZE 만큼의 유닛이 있을 수 있음)

enum class BlockType : uint16_t
{
    BLOCK_TYPE_AIR = 0,
    BLOCK_TYPE_SAND,
    BLOCK_TYPE_GRASS,
    BLOCK_TYPE_STONE,
    BLOCK_TYPE_SEA,
    BLOCK_TYPE_HILL,
    COUNT
};

struct BlockTextrueIndex
{
    int up;
    int side;
    int bottom;
};

struct Tile
{
    Tile() {
        isEmpty = true;
        fill(begin(isWalkable), end(isWalkable), true);
        fill(begin(slot), end(slot), false);
    }

    bool isEmpty; // 타일이 비어있는지 여부 (캐릭터가 들어갈 수 있는지 여부)
    bool isWalkable[STAGE_TILE_SIZE * STAGE_TILE_SIZE];
    bool slot[STAGE_TILE_SIZE * STAGE_TILE_SIZE];
};

class Stage
{
public:
    Stage(uint32_t stageWidth = 10, uint32_t stageLength = 10, uint32_t stageHeight = 5);
    ~Stage();

    void DeployTile(vector<XMFLOAT3>& targetPos, uint32_t numCharacters, int x, int z);
    void UndeployTile(int x, int z);

    bool IsBlockSolid(int x, int y, int z) const;

    uint32_t GetStageWidth() const;
    uint32_t GetStageLength() const;
    uint32_t GetStageHeight() const;
    float GetBlockSize() const;
    float GetBlockHeight() const;
    inline uint32_t GetBlockIndex(int x, int y, int z) const;
    inline uint32_t GetTileIndex(int x, int z) const;
    BlockType GetBlockType(int x, int y, int z) const;
    int32_t GetTextureIndex(BlockType tileType, const std::string& face) const;

    XMFLOAT3 BlockOffset() const;
    XMFLOAT3 GetTilePos(int x, int z) const;
    XMFLOAT3 GetTileSlotOffset(int x, int z) const;

private:
    std::vector<BlockType> blocks_;
    std::vector<BlockTextrueIndex> blockToTextureIndex_; // map[BlockType] = { up, side, bottom }

    std::vector<Tile> tiles_; // 각 타일마다 캐릭터가 들어갈 수 있는 슬롯 정보

    const uint32_t stageWidth_; // x축 방향 타일 수
    const uint32_t stageLength_; // z축 방향 타일 수
    const uint32_t stageHeight_; // y축 방향 타일 수 (높이)

    const float blockSize_ = 1.0f;
    const float blockHeight_ = 0.5f;
    XMFLOAT3 blockOffset_;
};

