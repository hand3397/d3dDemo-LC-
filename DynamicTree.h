#pragma once
#include "Collision.h"

namespace spe {;

class BroadPhase;

constexpr int32_t NULL_NODE = -1;;

struct AABBNode
{
    bool IsLeaf()const { return (child1 == NULL_NODE); }

    AABB aabb;
    void* data = nullptr;

    union
    {
        int32_t next = NULL_NODE;   // freeNode 일때 다음 freeNode를 가르킴
        int32_t parent;
    };

    int32_t child1 = NULL_NODE;
    int32_t child2 = NULL_NODE;

    // leaf = 0, freeNode = -1
    int32_t height = -1;
};

class DynamicTree
{
public:
    DynamicTree(uint32_t nodeCapacity = 16);
    ~DynamicTree();

    // 주어진 aabb와 userData로 node에 값 초기화, node 삽입
    int32_t CreateProxy(const AABB& aabb, void* userData);

    // proxyId에 해당하는 node Destroy
    void DestroyProxy(int32_t proxyId);

    // proxyId에 해당하는 node 삭제 후, 적당한 위치로 다시 Insert
    bool MoveProxy(int32_t proxyId, const AABB& aabb, const XMVECTOR& displacement);

    void* GetUserData(int32_t proxyId) const;

    const AABB& GetFatAABB(int32_t proxyId) const;

    template <typename T> void Query(T* callback, const AABB& aabb) const;

private:
    int32_t AllocateNode();
    void FreeNode(int32_t nodeId);

    void InsertLeaf(int32_t leaf);
    void RemoveLeaf(int32_t leaf);

    // 트리가 쏠리지 않게 balance 맞춰줌
    int32_t Balance(int32_t index);
    int32_t RotateLeft(int32_t index);
    int32_t RotateRight(int32_t index);

    float GetInsertionCostForLeaf(const AABB& leafAABB, int32_t child, float inheritedCost);
    float GetInsertionCost(const AABB& leafAABB, int32_t child, float inheritedCost);

    vector<AABBNode> nodes_;
    int32_t root_;
    uint32_t numNodes_;
    uint32_t nodeCapacity_;
    int32_t freeNode_;
};

template<typename T>
void DynamicTree::Query(T* callback, const AABB& aabb) const
{
    if (root_ == NULL_NODE)
        return;

    int stack[64];
    int* sp = stack;
    *sp++ = root_;

    while (sp > stack) {
        int nodeId = *(--sp);
        if (nodeId == NULL_NODE)
            continue;
        const AABBNode* node = &nodes_[nodeId];

        if (node->aabb.Intersects(aabb)) {
            if (node->IsLeaf()) {
                bool proceed = callback->QueryCallback(nodeId);
                if (!proceed)
                    return;
            }
            else {
                *sp++ = node->child1;
                *sp++ = node->child2;
            }
        }
    }
}

}

