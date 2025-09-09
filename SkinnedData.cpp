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

	startClipTime_ = st;
	endClipTime_ = MathHelper::Max(et, duration_);
}

void AnimationClip::SetDuration(float duration, uint32_t tickPerSecond)
{
	duration_ = duration;
	ticksPerSecond_ = tickPerSecond;
}

float AnimationClip::SecondToTick(float& s)
{
	if (ticksPerSecond_ == 0.0f) {
		s = fmodf(s, duration_);
		return s;
	}
	else {
		s = fmodf(s, duration_ / ticksPerSecond_);
		return s * ticksPerSecond_;
	}
}

float AnimationClip::GetClipStartTime()const
{
	return startClipTime_;
}

float AnimationClip::GetClipEndTime()const
{
	return endClipTime_;
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
	unordered_map<string, uint32_t> nodeToIdx, vector<XMFLOAT4X4>& nodeTransforms)
{
	parentNode_ = parentNode;
	childrenNode_ = childrenNode;
	nodeToIdx_ = nodeToIdx;
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

UINT SkinnedData::NodeCount()const
{
	return nodeToIdx_.size();
}

UINT SkinnedData::BoneCount()const
{
	return nodeToBone_.size();
}
 
void SkinnedData::GetFinalTransforms(const string& clipName, float timePos,  vector<XMFLOAT4X4>& finalTransforms)const
{
	uint32_t numNodes = NodeCount();
	uint32_t numBones = BoneCount();

	
	auto it = animations_.find(clipName);
	fill(finalTransforms.begin(), finalTransforms.begin() + numBones, MathHelper::Identity4x4());
	if (it == animations_.end()) {
		return;
	}
	vector<XMMATRIX> transforms(numNodes, XMMatrixIdentity());
	// 이 클립의 모든 뼈대를 주어진 시간에 맞게 보간한다.
	it->second.Interpolate(timePos, transforms);
	
	// rootNode = 0
	for (uint32_t ni = 1; ni < numNodes; ni++) {
		transforms[ni] = transforms[ni] * transforms[parentNode_[ni]];
	}

	
	//XMMatrixTranspose
	for (uint32_t ni = 0; ni < numNodes; ++ni) {
		int32_t bi = NodeToBone(ni);
		if (bi == -1)
			continue;
		XMMATRIX finalTransform = transforms[ni] * XMLoadFloat4x4(&nodeTransforms_[ni]) * XMLoadFloat4x4(&boneOffsets_[bi]);
		//XMMATRIX finalTransform = XMLoadFloat4x4(&boneOffsets_[i]);
		XMStoreFloat4x4(&finalTransforms[bi], XMMatrixTranspose(finalTransform));
	}
}

int32_t SkinnedData::NodeToIdx(const string& name) const
{
	auto it = nodeToIdx_.find(name);
	if (it == nodeToIdx_.end())
		return -1;
	return it->second;
}

int32_t SkinnedData::NodeToBone(const uint32_t node) const
{
	auto it = nodeToBone_.find(node);
	if (it == nodeToBone_.end())
		return -1;
	return it->second;
}

float SkinnedData::SecondToTick(const string& clipName, float& s)
{
	auto it = animations_.find(clipName);
	if (it != animations_.end()) {
		return it->second.SecondToTick(s);
	}
	return 0.0f;
}
