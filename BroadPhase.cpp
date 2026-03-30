#include "BroadPhase.h"

namespace spe {;

BroadPhase::BroadPhase(uint32_t moveCapacity) :
    numMoves_(0), moveCapacity_(moveCapacity)
{
    moves_.resize(moveCapacity, -1);
}

BroadPhase::~BroadPhase()
{
    moves_.clear();
}

int32_t BroadPhase::CreateProxy(const AABB& aabb, void* userData)
{
    int32_t id = tree_.CreateProxy(aabb, userData);
    BufferMove(id);
    return id;
}

void BroadPhase::DestroyProxy(int32_t proxyId)
{
    tree_.DestroyProxy(proxyId);

    // proxySet_에서 target이 포함된 원소를 제거한다.
    for (auto it = proxySet_.begin(); it != proxySet_.end(); ) {
        if (it->first == proxyId || it->second == proxyId)
            it = proxySet_.erase(it);
        else
            ++it;
    }
}

void BroadPhase::MoveProxy(int32_t proxyId, const AABB& aabb, const XMVECTOR& displacement)
{
    if (tree_.MoveProxy(proxyId, aabb, displacement)) {
        BufferMove(proxyId);
    }
}

void BroadPhase::BufferMove(int32_t proxyId)
{
    if (numMoves_ == moveCapacity_) {
        moveCapacity_ *= 2;
        moves_.resize(moveCapacity_);
    }
    moves_[numMoves_++] = proxyId;
}

void* BroadPhase::RayCast(const Ray& ray)
{
    return tree_.Query(ray);
}

bool BroadPhase::QueryCallback(int32_t proxyId)
{
    if (proxyId == queryProxyId_) {
        // 자기 자신과 충돌을 거른다.
        return true;
    }
    // contact의 중복을 피하기 위함
    proxySet_.insert({ std::min(proxyId, queryProxyId_), std::max(proxyId, queryProxyId_) });
    
    return true;
}

}