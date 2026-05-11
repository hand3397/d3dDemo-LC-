#include "Corps.h"

Corps::Corps() :
    numUnits_(0), currentState_(CorpsState::IDLE)
{
    units_.assign(MAX_CORPS_SIZE, nullptr);
    unitIDs_.assign(MAX_CORPS_SIZE, INVALID_ID); 
    tileIndicesXZ_.assign(MAX_CORPS_SIZE, make_pair(-1, -1));
}

Corps::~Corps()
{
}

void Corps::Update(int x, int z, float dt)
{
    const vector<Nav::Dir>& currentFlowField = stage_->RequestFlowField(x, z);
    for (Character* unit : units_) {
        if (!unit)
            continue;
        XMFLOAT3 pos = unit->GetPosition();
        int tileIndex = stage_->GetTileIndexFromWorldPos(pos);
        if (tileIndex == -1)
            continue;
        uint8_t dir = static_cast<uint8_t>(currentFlowField[tileIndex]);
        
        unit->Move(XMFLOAT3(Nav::dxf[dir], 0.0f, Nav::dzf[dir]));
        unit->Update(dt);
    }
}

void Corps::SetUnits(int numUnits, vector<Character*>& units)
{
    numUnits_ = numUnits;
    fill(units_.begin(), units_.end(), nullptr);
    fill(unitIDs_.begin(), unitIDs_.end(), INVALID_ID);
    for (int i = 0; i < numUnits && i < units.size() && i < MAX_CORPS_SIZE; ++i) {
        units_[i] = units[i];
        if (units[i]) {
            unitIDs_[i] = units[i]->GetID();
        }
    }
}

void Corps::SetStage(TStaeg* stage)
{
    stage_ = stage;
}

void Corps::CommandMove(const XMFLOAT3& destination)
{
    currentDestination_ = destination;

    vector<pair<int, int>> newXZ(numUnits_);
    int tileIndex = stage_->GetTileIndexFromWorldPos(destination);
    stage_->OccupyTiles(numUnits_, unitIDs_, tileIndex, tileIndicesXZ_, newXZ);
    tileIndicesXZ_ = newXZ;

    for (size_t i = 0; i < numUnits_; ++i) {
        const auto& [newX, newZ] = newXZ[i];
        units_[i]->SetTargetPos(newX, newZ, 
            stage_->GetTileCenter(newX, newZ));
    }

    const auto& [tx, tz] = stage_->GetTileIndexXZ(tileIndex);
    stage_->RequestFlowField(tx, tz);
}

void Corps::CommandAttack(Corps* targetEnemy)
{
}

void Corps::CommandStop()
{
}

int Corps::GetNumUnits() const
{
    return numUnits_;
}

bool Corps::IsEmpty() const
{
    return numUnits_ == 0;
}

XMFLOAT3 Corps::GetPosition() const
{
    return currentPosition_;
}

void Corps::CalculateFormationPositions(const XMFLOAT3& targetDestination)
{

}

const vector<uint32_t>& Corps::GetUnitIDs() const
{
    return unitIDs_;
}

const vector<pair<int, int>>& Corps::GetTileIndicesXZ_() const
{
    return tileIndicesXZ_;
}
