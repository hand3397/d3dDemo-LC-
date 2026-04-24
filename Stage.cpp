#include "Stage.h"

Stage::Stage(uint32_t stageWidth, uint32_t stageLength, uint32_t stageHeight) :
    stageWidth_(stageWidth), stageLength_(stageLength), stageHeight_(stageHeight)
{
    tiles_.clear();
    tiles_.resize(stageWidth_ * stageLength_ * stageHeight_, TileType::TILE_TYPE_AIR);
    
    for (int dy = 0; dy < stageHeight; dy++) {
        for (int dx = 0; dx < stageWidth; dx++) {
            for (int dz = 0; dz < stageLength; dz++) {
                int dIndex = GetIndex(dx, dy, dz);
                if (dx / 5 + dz / 3 >= dy)
                    tiles_[dIndex] = TileType::TILE_TYPE_GRASS;
            }
        }
    }
    

    tileToTextureIndex_.clear();
    // TileType과 텍스처 인덱스 매핑 초기화 (top, side, bottom)
    tileToTextureIndex_.push_back({ -1, -1, -1 }); // TileType::TILE_TYPE_AIR
    tileToTextureIndex_.push_back({ -1, -1, -1 }); // TileType::TILE_TYPE_SAND
    tileToTextureIndex_.push_back({  0,  3,  1 }); // TileType::TILE_TYPE_GRASS
    tileToTextureIndex_.push_back({ -1, -1, -1 }); // TileType::TILE_TYPE_STONE
    tileToTextureIndex_.push_back({ -1, -1, -1 }); // TileType::TILE_TYPE_SEA
    tileToTextureIndex_.push_back({  1,  3,  1 }); // TileType::TILE_TYPE_HILL
}

Stage::~Stage()
{
    tiles_.clear();
    tileToTextureIndex_.clear();
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

TileType Stage::GetTileType(int x, int y, int z) const
{
    return tiles_[GetIndex(x, y, z)];
}

int32_t Stage::GetTextureIndex(TileType tileType, const std::string& face) const
{
    if (face == "up") {
        return tileToTextureIndex_[static_cast<uint16_t>(tileType)].up;
    }
    else if (face == "side") {
        return tileToTextureIndex_[static_cast<uint16_t>(tileType)].side;
    }
    else if (face == "bottom") {
        return tileToTextureIndex_[static_cast<uint16_t>(tileType)].bottom;
    }
    return -1; // Invalid face
}

