#include "Stage.h"

Stage::Stage(uint32_t stageWidth, uint32_t stageLength, uint32_t stageHeight) :
    stageWidth_(stageWidth), stageLength_(stageLength), stageHeight_(stageHeight)
{
    tiles_.resize(stageWidth_ * stageLength_ * stageHeight_, TileType::TILE_TYPE_GRASS);
}

Stage::~Stage()
{
    tiles_.clear();
}

bool Stage::IsBlockSolid(int x, int y, int z) const
{
    if (x < 0 || x >= static_cast<int>(stageWidth_) ||
        y < 0 || y >= static_cast<int>(stageHeight_) ||
        z < 0 || z >= static_cast<int>(stageLength_)) {
        return false; // 범위를 벋어난 경우는 solid하지 않다고 간주한다.
    }
    else {
        TileType tile = tiles_[GetIndex(x, y, z)];
        return tile != TileType::TILE_TYPE_AIR;
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

float Stage::GetTileSize() const
{
    return tileSize_;
}

float Stage::GetTileHeight() const
{
    return tileHeight_;
}

uint32_t Stage::GetIndex(int x, int y, int z) const
{
    return y * stageWidth_ * stageLength_ + z * stageWidth_ + x;
}

