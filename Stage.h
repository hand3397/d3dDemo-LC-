#pragma once
#include <cstdint>
#include <vector>

enum class TileType
{
    TILE_TYPE_AIR,
    TILE_TYPE_SAND,
    TILE_TYPE_GRASS,
    TILE_TYPE_STONE,
    TILE_TYPE_SEA,
    TILE_TYPE_HILL,
    COUNT
};

class Stage
{
public:
    Stage(uint32_t stageWidth = 10, uint32_t stageLength = 10, uint32_t stageHeight = 5);
    ~Stage();

    bool IsBlockSolid(int x, int y, int z) const;

    uint32_t GetStageWidth() const;
    uint32_t GetStageLength() const;
    uint32_t GetStageHeight() const;
    float GetTileSize() const;
    float GetTileHeight() const;
    inline uint32_t GetIndex(int x, int y, int z) const;

private:
    std::vector<TileType> tiles_;

    const uint32_t stageWidth_; // x축 방향 타일 수
    const uint32_t stageLength_; // z축 방향 타일 수
    const uint32_t stageHeight_; // y축 방향 타일 수 (높이)

    const float tileSize_ = 1.0f;
    const float tileHeight_ = 1.0f;
};

