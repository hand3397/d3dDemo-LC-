#include "Corps.h"

Corps::Corps() :
    numUnits_(0), currentState_(CorpsState::IDLE)
{
    units_ = vector<Character*>(MAX_CORPS_SIZE, nullptr);
}

Corps::~Corps()
{
}

void Corps::Update(float dt)
{
    const vector<Nav::Dir>& currentFlowField = stage_->RequestFlowField(2, 2);
    for (Character* unit : units_) {
        if (!unit)
            continue;
        XMFLOAT3 pos = unit->GetPosition();
        int tileIndex = stage_->GetTileIndexFromWorldPos(pos);
        if (tileIndex == -1)
            continue;
        uint8_t dir = static_cast<uint8_t>(currentFlowField[tileIndex]);
        
        unit->GetRigidbody()->SetLinearVelocity(XMFLOAT3(Nav::dxf[dir], 0.0f, Nav::dzf[dir]));
        unit->Update(dt);
    }
}

void Corps::SetUnits(int numUnits, vector<Character*>& units)
{
    numUnits_ = numUnits;
    fill(units_.begin(), units_.end(), nullptr);
    for (int i = 0; i < numUnits && i < units.size() && i < MAX_CORPS_SIZE; ++i) {
        units_[i] = units[i];
    }
}

void Corps::SetStage(TStaeg* stage)
{
    stage_ = stage;
}

void Corps::CommandMove(const XMFLOAT3& destination)
{
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
