#include "SkinnedData.h"

using namespace DirectX;
 
float BoneAnimation::GetStartTime()const
{
	// keyframes are sorted by time, so first keyframe gives start time.
	return keyframes_.front().timePos_;
}

float BoneAnimation::GetEndTime()const
{
	// keyframes are sorted by time, so last keyframe gives end time.
	float f = keyframes_.back().timePos_;

	return f;
}

void BoneAnimation::Interpolate(float t, XMFLOAT4X4& M)const
{
	if( t <= keyframes_.front().timePos_)
	{
		XMVECTOR S = XMLoadFloat3(&keyframes_.front().scale_);
		XMVECTOR P = XMLoadFloat3(&keyframes_.front().translation_);
		XMVECTOR Q = XMLoadFloat4(&keyframes_.front().rotationQuat_);

		XMVECTOR zero = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		XMStoreFloat4x4(&M, XMMatrixAffineTransformation(S, zero, Q, P));
	}
	else if( t >= keyframes_.back().timePos_ )
	{
		XMVECTOR S = XMLoadFloat3(&keyframes_.back().scale_);
		XMVECTOR P = XMLoadFloat3(&keyframes_.back().translation_);
		XMVECTOR Q = XMLoadFloat4(&keyframes_.back().rotationQuat_);

		XMVECTOR zero = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		XMStoreFloat4x4(&M, XMMatrixAffineTransformation(S, zero, Q, P));
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
				XMStoreFloat4x4(&M, XMMatrixAffineTransformation(S, zero, Q, P));

				break;
			}
		}
	}
}

float AnimationClip::GetClipStartTime()const
{
	// Find smallest start time over all bones in this clip.
	float t = MathHelper::Infinity;
	for(UINT i = 0; i < boneAnimations_.size(); ++i)
	{
		t = MathHelper::Min(t, boneAnimations_[i].GetStartTime());
	}

	return t;
}

float AnimationClip::GetClipEndTime()const
{
	// Find largest end time over all bones in this clip.
	float t = 0.0f;
	for(UINT i = 0; i < boneAnimations_.size(); ++i)
	{
		t = MathHelper::Max(t, boneAnimations_[i].GetEndTime());
	}

	return t;
}

void AnimationClip::Interpolate(float t, vector<XMFLOAT4X4>& boneTransforms)const
{
	for(UINT i = 0; i < boneAnimations_.size(); ++i)
	{
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

void SkinnedData::Set(vector<int>& parentBone, vector<vector<int>>& childrenBone, 
	unordered_map<string, uint32_t> nameToIdx, vector<XMFLOAT4X4>& boneOffsets)
{
	parentBone_ = parentBone;
	childrenBone_ = childrenBone;
	nameToIdx_ = nameToIdx;
	boneOffsets_ = boneOffsets;
}

void SkinnedData::AddAnimaiton(const string& clipName, const AnimationClip& animationClip)
{
	animations_[clipName] = animationClip;
}

UINT SkinnedData::BoneCount()const
{
	return parentBone_.size();
}
 
void SkinnedData::GetFinalTransforms(const string& clipName, float timePos,  vector<XMFLOAT4X4>& finalTransforms)const
{
	UINT numBones = boneOffsets_.size();

	vector<XMFLOAT4X4> toParentTransforms(numBones);

	// 이 클립의 모든 뼈대를 주어진 시간에 맞게 보간한다.
	auto clip = animations_.find(clipName);
	clip->second.Interpolate(timePos, toParentTransforms);

	//
	// Traverse the hierarchy and transform all the bones to the root space.
	//

	vector<XMFLOAT4X4> toRootTransforms(numBones);

	// The root bone has index 0.  The root bone has no parent, so its toRootTransform
	// is just its local bone transform.
	toRootTransforms[0] = toParentTransforms[0];

	// Now find the toRootTransform of the children.
	for(UINT i = 1; i < numBones; ++i)
	{
		XMMATRIX toParent = XMLoadFloat4x4(&toParentTransforms[i]);

		int parentIndex = parentBone_[i];
		XMMATRIX parentToRoot = XMLoadFloat4x4(&toRootTransforms[parentIndex]);

		XMMATRIX toRoot = XMMatrixMultiply(toParent, parentToRoot);

		XMStoreFloat4x4(&toRootTransforms[i], toRoot);
	}

	// Premultiply by the bone offset transform to get the final transform.
	for(UINT i = 0; i < numBones; ++i)
	{
		XMMATRIX offset = XMLoadFloat4x4(&boneOffsets_[i]);
		XMMATRIX toRoot = XMLoadFloat4x4(&toRootTransforms[i]);
        XMMATRIX finalTransform = XMMatrixMultiply(offset, toRoot);
		XMStoreFloat4x4(&finalTransforms[i], XMMatrixTranspose(finalTransform));
	}
}

int64_t SkinnedData::NameToIdx(const string& name)
{
	if (nameToIdx_.find(name) == nameToIdx_.end())
		return -1;
	return nameToIdx_[name];
}
