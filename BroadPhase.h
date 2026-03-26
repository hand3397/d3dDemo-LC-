#pragma once
#include "DynamicTree.h"

namespace spe {;

// DynamicTree에 node를 추가 제거를 관리하고 
// moveProxy(기존 fatAABB를 벗어난 움직임을 감지)를 따로 저장하여
// 각moveProxy에 대해 DynamicTree와 AABB 충돌을 판정해 
// 충돌가능성이 있는 data쌍을 contactManager로 넘김
class BroadPhase
{
public:
    BroadPhase(uint32_t numCapacity = 16);
    ~BroadPhase();

	// AABB에 해당하는 proxy 생성 - DynamicTree의 nodeId를 반환한다
	int32_t CreateProxy(const AABB& aabb, void* userData);

	void DestroyProxy(int32_t proxyId);

    // 움직인 노드가 tree의 aabb를 벗어난다면 moves_에 proxyId를 넣는다.
	void MoveProxy(int32_t proxyId, const AABB& aabb, const XMVECTOR& displacement);

	void BufferMove(int32_t proxyId);

    // ray와 충돌한 물체의 nodeId를 반환
    void* RayCast(const Ray& ray);

	template <typename T> void UpdatePairs(T* callback);

private:
    friend class DynamicTree;
    bool QueryCallback(int32_t proxyId);

    DynamicTree tree_;
    std::set<std::pair<int32_t, int32_t>> proxySet_;

    vector<int32_t> moves_;
    uint32_t numMoves_;
    uint32_t moveCapacity_;
    int32_t queryProxyId_;   // 현재 query중인 proxyId
};

template <typename T> 
void BroadPhase::UpdatePairs(T* callback)
{
    for (uint32_t i = 0; i < numMoves_; ++i) 
    {
        queryProxyId_ = moves_[i];
        if (queryProxyId_ == NULL_NODE)
            continue;

        const AABB& fatAABB = tree_.GetFatAABB(queryProxyId_);
        tree_.Query(this, fatAABB);
    }

    numMoves_ = 0;
    for (const auto& [a, b] : proxySet_)
    {
        callback->AddPair(tree_.GetUserData(a), tree_.GetUserData(b));
    }
}

} // namespace spe



