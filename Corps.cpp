#include "Corps.h"

Corps::Corps() :
    numUnits_(0), currentState_(CorpsState::IDLE)
{
    units_ = vector<Character*>(MAX_CORPS_SIZE, nullptr);
}

Corps::~Corps()
{
}

void Corps::SetUnits(int numUnits, vector<Character*>& units)
{
    numUnits_ = numUnits;
    fill(units_.begin(), units_.end(), nullptr);
    for (int i = 0; i < numUnits && i < units.size() && i < MAX_CORPS_SIZE; ++i) {
        units_[i] = units[i];
    }
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

void Corps::SetTileIndex(pair<int, int> tileIndex)
{
    tileIndex_ = tileIndex;
}

pair<int, int> Corps::GetTileIndex() const
{
    return tileIndex_;
}

void Corps::CalculateFormationPositions(const XMFLOAT3& targetDestination)
{

}
