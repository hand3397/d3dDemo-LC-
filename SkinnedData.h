#pragma once

#include "d3dUtil.h"
#include "MathHelper.h"
#include "SkinnedAnimator.h"

using namespace std;
using namespace DirectX;

class SkinnedData
{
public:
	UINT NodeCount()const;
	UINT BoneCount()const;

	void SetNode(vector<uint32_t>& parentNode, vector<vector<uint32_t>>& childrenNode,
		unordered_map<string, uint32_t> nameToIdx, vector<XMFLOAT4X4>& nodeTrnasforms);

	void SetBone(unordered_map<uint32_t, uint32_t> nodeToBone, vector<XMFLOAT4X4>& boneOffset);

	void AddAnimaiton(const string& clipName, const SkinnedAnimationClip& clip);

	 // In a real project, you'd want to cache the result if there was a chance
	 // that you were calling this several times with the same clipName at 
	 // the same timePos.
    void GetFinalTransforms(const string& clipName, float timePos, 
		 vector<XMFLOAT4X4>& finalTransforms)const;
	void GetFinalTransforms(const string& clipName1, float timePos,
		const string& clipName2, float alpha, vector<XMFLOAT4X4>& finalTransforms)const;
	
	void AddBlendingAnimation(const string& name, const string& clip1, const string& clip2, float alpha = 0.5f);
	void BlendInterpolate(const SkinnedAnimationClip& clip1, const SkinnedAnimationClip& clip2, float timePos,
		vector<XMMATRIX>& transforms, float alpha = 0.5f)const;

	int32_t NodeToIdx(const string& name) const;
	int32_t NodeToBone(const uint32_t node) const;
private:
	// rootNode = 0;
	vector<uint32_t> parentNode_;
	vector<vector<uint32_t>> childrenNode_;

	unordered_map<string, uint32_t> nodeToIdx_;
	unordered_map<uint32_t, uint32_t> nodeToBone_;

	vector<XMFLOAT4X4> nodeTransforms_;
	vector<XMFLOAT4X4> boneOffsets_;
    
	unordered_map<string, SkinnedAnimationClip> animations_;
};

struct SkinnedModelInstance
{
	SkinnedData* skinnedInfo_ = nullptr;
	vector<XMFLOAT4X4> finalTransforms_;
	// 여러 애니메이션이 들어오면 애니메이션을 혼합
	string clipName_;
	string blendingClipName_;
	float blendingAlpha_ = 1.0f; // (blendingClipNames_) 0.0f ~ 1.0f (clipNames_)
	float timePos_ = 0.0f;

	void SetAnimtion(const string& clipName, float animationTime)
	{
		clipName_ = clipName;
		blendingClipName_.clear();
		timePos_ = animationTime;
	}

	void SetAnimtion(const string& clipName, float animationTime, const string& blendingClip, float alpha)
	{
		clipName_ = clipName;
		timePos_ = animationTime;
		blendingClipName_ = blendingClip;
		blendingAlpha_ = alpha;
	}

	// Called every frame and increments the time position, interpolates the 
	// animations for each bone based on the current animation clip, and 
	// generates the final transforms which are ultimately set to the effect
	// for processing in the vertex shader.
	void UpdateSkinnedAnimation()
	{
		// Loop animation
		float animationTick = timePos_;

		if (blendingClipName_.empty()) {
			// Compute the final transforms for this time position.
			skinnedInfo_->GetFinalTransforms(clipName_, animationTick, finalTransforms_);
		}
		else {
			if (clipName_ == blendingClipName_ || blendingAlpha_ >= 1.0f)
				skinnedInfo_->GetFinalTransforms(clipName_, animationTick, finalTransforms_);
			else
				skinnedInfo_->GetFinalTransforms(clipName_, animationTick, blendingClipName_, blendingAlpha_, finalTransforms_);
		}
	}
};