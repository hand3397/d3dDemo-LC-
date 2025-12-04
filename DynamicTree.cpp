#include "DynamicTree.h"

namespace spe {;

DynamicTree::DynamicTree(uint32_t nodeCapacity = 16) :
	root_(NULL_NODE), numNodes_(0), nodeCapacity_(nodeCapacity), freeNode_(0)
{
	nodes_.resize(nodeCapacity_);
	for (uint32_t i = 0; i < nodeCapacity_ - 1; ++i) {
		nodes_[i].next = i + 1;
	}
	nodes_[nodeCapacity_ - 1].next = NULL_NODE;
}

DynamicTree::~DynamicTree()
{
	nodes_.clear();
}

int32_t DynamicTree::CreateProxy(const AABB& aabb, void* userData)
{
	int32_t proxyId = AllocateNode();
	AABBNode* node = &nodes_[proxyId];

	// 물체가 조금만 움직여도 트리가 변경되는걸 막기 위해
	// tree의 AABB는 실제 AABB보다 크게 만든다.
	XMVECTOR aabbMargin = XMVectorSet(0.1f, 0.1f, 0.1f, 0.0f);
	node->aabb = AABB(XMLoadFloat3(&aabb.lowerBound) - aabbMargin,
		XMLoadFloat3(&aabb.upperBound) + aabbMargin);
	node->data = userData;
	node->height = 0;

	InsertLeaf(proxyId);

	return proxyId;
}

void DynamicTree::DestroyProxy(int32_t proxyId)
{
	RemoveLeaf(proxyId);
	FreeNode(proxyId);
}

bool DynamicTree::MoveProxy(int32_t proxyId, const AABB& aabb, const XMVECTOR& displacement)
{
	// 물체의 AABB가 아직 node의 AABB 내부에 존재한다면 node의 위치를 변경하지않는다.
	if (nodes_[proxyId].aabb.Contains(aabb)) {
		return false;
	}

	RemoveLeaf(proxyId);

	// 물체가 조금만 움직여도 트리가 변경되는걸 막기 위해
	// tree의 AABB는 실제 AABB보다 크게 만든다.
	XMVECTOR aabbMargin = XMVectorSet(0.1f, 0.1f, 0.1f, 0.0f);
	XMVECTOR lowerBound = XMLoadFloat3(&aabb.lowerBound) - aabbMargin;
	XMVECTOR upperBound = XMLoadFloat3(&aabb.upperBound) + aabbMargin;

	XMVECTOR vD = displacement * 2.0f;
	XMVECTOR negative = XMVectorSelect(XMVectorZero(), vD, XMVectorLess(vD, XMVectorZero()));
	XMVECTOR positive = XMVectorSelect(XMVectorZero(), vD, XMVectorGreaterOrEqual(vD, XMVectorZero()));

	lowerBound = XMVectorAdd(lowerBound, negative);
	upperBound = XMVectorAdd(upperBound, positive);

	nodes_[proxyId].aabb = AABB(lowerBound, upperBound);

	InsertLeaf(proxyId);

	return true;
}

void* DynamicTree::GetUserData(int32_t proxyId) const
{
	return nodes_[proxyId].data;
}

const AABB& DynamicTree::GetFatAABB(int32_t proxyId) const
{
	return nodes_[proxyId].aabb;
}

int32_t DynamicTree::AllocateNode()
{
	// freeNode_를 가져와 새로운 노드로 할당한다.
	// freeNode_가 NULL_NODE라면 tree의 nodeCapacity_를 2배 늘린다.
	if (freeNode_ == NULL_NODE) {
		nodeCapacity_ *= 2;
		nodes_.resize(nodeCapacity_);
		for (int i = numNodes_; i < nodeCapacity_ - 1; i++) {
			nodes_[i].next = i + 1;
		}
		nodes_[nodeCapacity_ - 1].next = NULL_NODE;
		freeNode_ = numNodes_;
	}

	int32_t nodeId = freeNode_;
	freeNode_ = nodes_[nodeId].next;
	nodes_[nodeId].parent = NULL_NODE;
	nodes_[nodeId].child1 = NULL_NODE;
	nodes_[nodeId].child2 = NULL_NODE;
	nodes_[nodeId].height = 0;
	nodes_[nodeId].data = nullptr;
	++numNodes_;

	return nodeId;
}

void DynamicTree::FreeNode(int32_t nodeId)
{
	nodes_[nodeId].next = freeNode_;
	nodes_[nodeId].height = -1;
	freeNode_ = nodeId;
	--numNodes_;
}

void DynamicTree::InsertLeaf(int32_t leaf)
{
	if (root_ == NULL_NODE) {
		root_ = leaf;
		nodes_[root_].parent = NULL_NODE;
		return;
	}

	AABB leafAABB = nodes_[leaf].aabb;
	int32_t index = root_;

	// 루트부터 내려가며 최적의 위치를 찾는다
	while (nodes_[index].isLeaf() == false) {
		int32_t child1 = nodes_[index].child1;
		int32_t child2 = nodes_[index].child2;

		float area = nodes_[index].aabb.GetSurface();
		AABB aabb;
		aabb.Combine(nodes_[index].aabb, leafAABB);
		float combinedArea = aabb.GetSurface();

		float cost = 2.0f * combinedArea;
		float inheritedCost = 2.0f * (combinedArea - area);

		float cost1;
		if (nodes_[child1].isLeaf()) {
			// child가 leaf라면: leaf와 leaf를 묶어서 새로운 내부 노드를 만드는 비용
			cost1 = GetInsertionCostForLeaf(leafAABB, child1, inheritedCost);
		}
		else {
			// 내부 노드라면: 그 subtree 안쪽으로 leaf를 삽입할 때의 예상 비용
			cost1 = GetInsertionCost(leafAABB, child1, inheritedCost);
		}

		float cost2;
		if (nodes_[child2].isLeaf()) {
			cost2 = GetInsertionCostForLeaf(leafAABB, child2, inheritedCost);
		}
		else {
			cost2 = GetInsertionCost(leafAABB, child2, inheritedCost);
		}

		// 현재 위치에 바로 붙이는 비용(cost)이 두 자식으로 내려가는 비용보다 작으면
		// 더 내려가면 오히려 비용이 증가하므로 여기서 삽입
		if (cost < cost1 && cost < cost2)
			break;

		if (cost1 < cost2) {
			index = child1;
		}
		else {
			index = child2;
		}
	}

	// 최적의 위치인 index에 있던 노드를 형제노드로 만들고 새로운 부모노드를 만든다.
	int32_t sibling = index;

	int32_t oldParent = nodes_[sibling].parent;
	int32_t newParent = AllocateNode();

	nodes_[newParent].parent = oldParent;
	nodes_[newParent].data = nullptr;
	nodes_[newParent].aabb.Combine(leafAABB, nodes_[sibling].aabb);
	nodes_[newParent].height = nodes_[sibling].height + 1;

	// 새로운 부모노드에 insert할 노드와 기존 노드를 연결
	if (oldParent != NULL_NODE) {
		if (nodes_[oldParent].child1 == sibling) {
			nodes_[oldParent].child1 = newParent;
		}
		else {
			nodes_[oldParent].child2 = newParent;
		}

		nodes_[newParent].child1 = sibling;
		nodes_[newParent].child2 = leaf;
		nodes_[sibling].parent = newParent;
		nodes_[leaf].parent = newParent;
	}
	else {
		nodes_[newParent].child1 = sibling;
		nodes_[newParent].child2 = leaf;
		nodes_[sibling].parent = newParent;
		nodes_[leaf].parent = newParent;
		root_ = newParent;
	}

	// 노드가 삽입된 위치의 부모로부터 root로 올라오며 aabb와 height를 업데이트 한다.
	index = nodes_[leaf].parent;
	while (index != NULL_NODE) {
		index = Balance(index);

		int32_t child1 = nodes_[index].child1;
		int32_t child2 = nodes_[index].child2;

		assert(child1 != NULL_NODE);
		assert(child2 != NULL_NODE);

		nodes_[index].height = std::max(nodes_[child1].height, nodes_[child2].height) + 1;
		nodes_[index].aabb.Combine(nodes_[child1].aabb, nodes_[child2].aabb);

		index = nodes_[index].parent;
	}

}

void DynamicTree::RemoveLeaf(int32_t leaf)
{
	if (leaf == root_) {
		root_ = NULL_NODE;
		return;
	}

	int32_t parent = nodes_[leaf].parent;
	int32_t grandParent = nodes_[parent].parent;
	int32_t sibling = nodes_[parent].child1 == leaf ?
		nodes_[parent].child2 : nodes_[parent].child1;

	// parent가 root일떄
	if (grandParent == NULL_NODE) {
		root_ = sibling;
		nodes_[sibling].parent = NULL_NODE;
		FreeNode(parent);
	}
	else {
		if (nodes_[grandParent].child1 == parent) {
			nodes_[grandParent].child1 = sibling;
		}
		else {
			nodes_[grandParent].child2 = sibling;
		}
		nodes_[sibling].parent = grandParent;
		FreeNode(parent);

		int32_t index = grandParent;
		while (index != NULL_NODE) {
			index = Balance(index);

			int32_t child1 = nodes_[index].child1;
			int32_t child2 = nodes_[index].child2;

			nodes_[index].height = std::max(nodes_[child1].height, nodes_[child2].height) + 1;
			nodes_[index].aabb.Combine(nodes_[child1].aabb, nodes_[child2].aabb);

			index = nodes_[index].parent;
		}
	}
}

int32_t DynamicTree::Balance(int32_t iA)
{
	// tree를 밸런싱 할때 각 노드의 깊이만 가지고 밸런싱을 진행한다.
	// 추후에 aabb의 표면적을 가장 적게 늘리는 방향으로 변경해야한다.
	AABBNode* A = &nodes_[iA];

	if (A->isLeaf() || A->height < 2) {
		return iA;
	}

	int32_t iB = A->child1;
	int32_t iC = A->child2;

	AABBNode* B = &nodes_[iB];
	AABBNode* C = &nodes_[iC];

	int32_t balance = C->height - B->height;

	// 오른쪽 자식이 더 무거움 왼쪽 회전
	if (balance > 1) {
		return RotateLeft(iA);
	}
	// 왼쪽 자식이 더 무거움 오른쪽 회전
	if (balance < -1) {
		return RotateRight(iA);
	}

	return iA;
}

int32_t DynamicTree::RotateLeft(int32_t iA)
{
	//case - R
	int32_t iB = nodes_[iA].child1;
	int32_t iC = nodes_[iA].child2;
	int32_t iF = nodes_[iC].child1;
	int32_t iG = nodes_[iC].child2;

	AABBNode* A = &nodes_[iA];
	AABBNode* B = &nodes_[iB];
	AABBNode* C = &nodes_[iC];
	AABBNode* F = &nodes_[iF];
	AABBNode* G = &nodes_[iG];

	C->child1 = iA;
	C->parent = A->parent;
	A->parent = iC;

	if (C->parent != NULL_NODE) {
		if (nodes_[C->parent].child1 == iA) {
			nodes_[C->parent].child1 = iC;
		}
		else {
			nodes_[C->parent].child2 = iC;
		}
	}
	else {
		root_ = iC;
	}

	// C의 자식노드 회전 진행
	// case - RL
	if (F->height > G->height) {
		C->child2 = iF;
		A->child2 = iG;
		G->parent = iA;
		nodes_[iG].parent = iA;
		A->aabb.Combine(B->aabb, G->aabb);
		C->aabb.Combine(A->aabb, F->aabb);

		A->height = std::max(B->height, G->height) + 1;
		C->height = std::max(A->height, F->height) + 1;
	}
	// case - RR
	else {
		C->child2 = iG;
		A->child2 = iF;
		F->parent = iA;
		A->aabb.Combine(B->aabb, F->aabb);
		C->aabb.Combine(A->aabb, G->aabb);

		A->height = std::max(B->height, F->height) + 1;
		C->height = std::max(A->height, G->height) + 1;
	}

	// 새로운 부모노드인 C를 반환 (A가 C의 자식노드가 됨)
	return iC;
}

int32_t DynamicTree::RotateRight(int32_t iA)
{
	//case - L
	int32_t iB = nodes_[iA].child1;
	int32_t iC = nodes_[iA].child2;
	int32_t iD = nodes_[iB].child1;
	int32_t iE = nodes_[iB].child2;

	AABBNode* A = &nodes_[iA];
	AABBNode* B = &nodes_[iB];
	AABBNode* C = &nodes_[iC];
	AABBNode* D = &nodes_[iD];
	AABBNode* E = &nodes_[iE];

	B->child1 = iA;
	B->parent = A->parent;
	A->parent = iB;

	if (B->parent != NULL_NODE) {
		if (nodes_[B->parent].child1 == iA) {
			nodes_[B->parent].child1 = iB;
		}
		else {
			nodes_[B->parent].child2 = iB;
		}
	}
	else {
		root_ = iB;
	}

	// B의 자식노드 회전진행
	// case - LL
	if (D->height > E->height) {
		B->child2 = iD;
		A->child1 = iE;
		E->parent = iA;
		A->aabb.Combine(C->aabb, E->aabb);
		B->aabb.Combine(A->aabb, D->aabb);

		A->height = std::max(C->height, E->height) + 1;
		B->height = std::max(A->height, D->height) + 1;
	}
	// case - LR
	else {
		B->child2 = iE;
		A->child1 = iD;
		D->parent = iA;
		A->aabb.Combine(C->aabb, D->aabb);
		B->aabb.Combine(A->aabb, E->aabb);

		A->height = std::max(C->height, D->height) + 1;
		B->height = std::max(A->height, E->height) + 1;
	}

	// 새로운 부모노드인 B를 반환 (A가 B의 자식노드가 됨)
	return iB;
}

float DynamicTree::GetInsertionCostForLeaf(const AABB& leafAABB, int32_t child, float inheritedCost)
{
	AABB aabb;
	aabb.Combine(leafAABB, nodes_[child].aabb);
	return aabb.GetSurface() + inheritedCost;
}

float DynamicTree::GetInsertionCost(const AABB& leafAABB, int32_t child, float inheritedCost)
{
	float oldArea = nodes_[child].aabb.GetSurface();
	AABB aabb;
	aabb.Combine(leafAABB, nodes_[child].aabb);
	float newArea = aabb.GetSurface();
	return (newArea - oldArea) + inheritedCost;
}

}