#include "SkinnedData.h"

using namespace DirectX;
 
float BoneAnimation::GetStartTime()const
{
	// keyframes are sorted by time, so first keyframe gives start time.
	if (keyframes_.empty())
		return MathHelper::Infinity;
	return keyframes_.front().timePos_;
}

float BoneAnimation::GetEndTime()const
{
	// keyframes are sorted by time, so last keyframe gives end time.
	if (keyframes_.empty())
		return 0.0f;
	return keyframes_.back().timePos_;
}

void BoneAnimation::Interpolate(float t, XMMATRIX& M)const
{
	if (keyframes_.empty()) {
		M = XMMatrixIdentity();
		return;
	}
		
	if( t <= keyframes_.front().timePos_)
	{
		XMVECTOR S = XMLoadFloat3(&keyframes_.front().scale_);
		XMVECTOR P = XMLoadFloat3(&keyframes_.front().translation_);
		XMVECTOR Q = XMLoadFloat4(&keyframes_.front().rotationQuat_);

		XMVECTOR zero = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		M = XMMatrixAffineTransformation(S, zero, Q, P);
	}
	else if( t >= keyframes_.back().timePos_ )
	{
		XMVECTOR S = XMLoadFloat3(&keyframes_.back().scale_);
		XMVECTOR P = XMLoadFloat3(&keyframes_.back().translation_);
		XMVECTOR Q = XMLoadFloat4(&keyframes_.back().rotationQuat_);

		XMVECTOR zero = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		M = XMMatrixAffineTransformation(S, zero, Q, P);
	}
	else
	{
		for(UINT i = 0; i < keyframes_.size()-1; ++i)
		{
			if( t >= keyframes_[i].timePos_ && t <= keyframes_[i+1].timePos_ )
			{
				float lerpPercent = (t - keyframes_[i].timePos_) / (keyframes_[i+1].timePos_ - keyframes_[i].timePos_);

				XMVECTOR s0 = XMLoadFloat3(&keyframes_[i].scale_);
				XMVECTOR s1 = XMLoadFloat3(&keyframes_[i+1].scale_);

				XMVECTOR p0 = XMLoadFloat3(&keyframes_[i].translation_);
				XMVECTOR p1 = XMLoadFloat3(&keyframes_[i+1].translation_);

				XMVECTOR q0 = XMLoadFloat4(&keyframes_[i].rotationQuat_);
				XMVECTOR q1 = XMLoadFloat4(&keyframes_[i+1].rotationQuat_);

				XMVECTOR S = XMVectorLerp(s0, s1, lerpPercent);
				XMVECTOR P = XMVectorLerp(p0, p1, lerpPercent);
				XMVECTOR Q = XMQuaternionSlerp(q0, q1, lerpPercent);

				XMVECTOR zero = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
				M = XMMatrixAffineTransformation(S, zero, Q, P);

				break;
			}
		}
	}
}

void AnimationClip::SetClipTime()
{
	// Find smallest start time over all bones in this clip.
	float st = MathHelper::Infinity, et = 0.0f;
	for (UINT i = 0; i < boneAnimations_.size(); ++i) {
		st = MathHelper::Min(st, boneAnimations_[i].GetStartTime());
		et = MathHelper::Max(et, boneAnimations_[i].GetEndTime());
	}

	startTime_ = st;
	endTime_ = et;
}

float AnimationClip::GetClipStartTime()const
{
	return startTime_;
}

float AnimationClip::GetClipEndTime()const
{
	return endTime_;
}

void AnimationClip::Interpolate(float t, vector<XMMATRIX>& boneTransforms)const
{
	int numBoenAnims = boneAnimations_.size();
	for(UINT i = 0; i < numBoenAnims; ++i) {
		boneAnimations_[i].Interpolate(t, boneTransforms[i]);
	}
}

float SkinnedData::GetClipStartTime(const string& clipName)const
{
	auto clip = animations_.find(clipName);
	return clip->second.GetClipStartTime();
}

float SkinnedData::GetClipEndTime(const string& clipName)const
{
	auto clip = animations_.find(clipName);
	return clip->second.GetClipEndTime();
}

void SkinnedData::SetNode(vector<uint32_t>& parentNode, vector<vector<uint32_t>>& childrenNode,
	unordered_map<string, uint32_t> nameToIdx, vector<XMFLOAT4X4>& nodeTransforms)
{
	parentNode_ = parentNode;
	childrenNode_ = childrenNode;
	nameToIdx_ = nameToIdx;
	nodeTransforms_ = nodeTransforms;
}

void SkinnedData::SetBone(unordered_map<uint32_t, uint32_t> nodeToBone, vector<XMFLOAT4X4>& boneOffset)
{
	nodeToBone_ = nodeToBone;
	boneOffsets_ = boneOffset;
}

void SkinnedData::AddAnimaiton(const string& clipName, const AnimationClip& animationClip)
{
	animations_[clipName] = animationClip;
}

UINT SkinnedData::BoneCount()const
{
	return nodeToBone_.size();
}
 
void SkinnedData::GetFinalTransforms(const string& clipName, float timePos,  vector<XMFLOAT4X4>& finalTransforms)const
{
	uint32_t numBones = BoneCount();

	
	// 이 클립의 모든 뼈대를 주어진 시간에 맞게 보간한다.
	auto it = animations_.find(clipName);
	if (it == animations_.end()) {
		fill(finalTransforms.begin(), finalTransforms.begin() + numBones, MathHelper::Identity4x4());
		return;
	}
	vector<XMMATRIX> transforms(numBones, XMMatrixIdentity());
	it->second.Interpolate(timePos, transforms);

	/*
	queue<uint32_t> q;
	for (uint32_t rb : rootBone_)
		for(uint32_t cb : childrenBone_[rb])
			q.push(cb);
	
	while (!q.empty()) {
		uint32_t boneIdx = q.front(); q.pop();
		transforms[boneIdx] = transforms[parentBone_[boneIdx]] * transforms[boneIdx];
		for (uint32_t cb : childrenBone_[boneIdx])
			q.push(cb);
	}

	//XMMatrixTranspose
	for (uint32_t i = 0; i < numBones; ++i) {
		XMMATRIX finalTransform = transforms[i] * XMLoadFloat4x4(&boneOffsets_[i]);
		//XMMATRIX finalTransform = XMLoadFloat4x4(&boneOffsets_[i]);
		XMStoreFloat4x4(&finalTransforms[i], XMMatrixTranspose(finalTransform));
	}
	*/
}

int32_t SkinnedData::NameToIdx(const string& name)
{
	if (nameToIdx_.find(name) == nameToIdx_.end())
		return -1;
	return nameToIdx_[name];
}

int32_t SkinnedData::NodeToBone(int node)
{
	if (nodeToBone_.find(node) == nodeToBone_.end())
		return -1;
	return nodeToBone_[node];
}
