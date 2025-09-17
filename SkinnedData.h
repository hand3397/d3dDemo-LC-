#pragma once

#include "d3dUtil.h"
#include "MathHelper.h"

using namespace std;
using namespace DirectX;

// Keyframe은 특정 시점에서의 본 변환 상태를 정의
struct Keyframe
{
	Keyframe() = default;
	~Keyframe() = default;

    float timePos_ = 0.0f;
	XMFLOAT3 translation_ = XMFLOAT3(0.0f, 0.0f, 0.0f);
    XMFLOAT3 scale_ = XMFLOAT3(1.0f, 1.0f, 1.0f);
    XMFLOAT4 rotationQuat_ = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	float weight_ = 1.0f;
};

// BoneAnimation은 키프레임들의 리스트로 정의된다.  
// 특정 시간이 두 키프레임 사이에 있을 경우, 그 시간을 감싸고 있는 두 인접한 키프레임을 기준으로 보간한다.
// 애니메이션은 항상 최소 두 개의 키프레임을 가진다고 가정한다.
struct BoneAnimation
{
	float GetStartTime()const;
	float GetEndTime()const;

    void Interpolate(float t, XMMATRIX& M)const;

	vector<Keyframe> keyframes_;
};

// AnimationClip은 '걷기','뛰기','공격' 같은 개별 애니메이션 클립을 대표한다.
// 하나의 AnimationClip 객체는 애니메이션 클립을 구성하는 BoneAnimation 인스턴스들을 담는다.
struct AnimationClip
{
	void SetClipTime();
	void SetDuration(float duration, uint32_t tickPerSecond);
	float SecondToTick(float& s);

	float GetClipStartTime() const;
	float GetClipEndTime() const;

    void Interpolate(float t, vector<XMMATRIX>& boneTransforms) const;

    vector<BoneAnimation> boneAnimations_;

private:
	float duration_ = 0.0f;
	// 값이 0일 경우 keyframe의 timePos_를 tick이 아닌 초단위로 해석
	float ticksPerSecond_ = 0.0f;

	float startClipTime_ = 0.0f;
	float endClipTime_ = 0.0f;
};

class SkinnedData
{
public:
	UINT NodeCount()const;
	UINT BoneCount()const;

	float GetClipStartTime(const string& clipName)const;
	float GetClipEndTime(const string& clipName)const;

	void SetNode(vector<uint32_t>& parentNode, vector<vector<uint32_t>>& childrenNode,
		unordered_map<string, uint32_t> nameToIdx, vector<XMFLOAT4X4>& nodeTrnasforms);

	void SetBone(unordered_map<uint32_t, uint32_t> nodeToBone, vector<XMFLOAT4X4>& boneOffset);

	void AddAnimaiton(const string& clipName, const AnimationClip& animationClip);

	 // In a real project, you'd want to cache the result if there was a chance
	 // that you were calling this several times with the same clipName at 
	 // the same timePos.
    void GetFinalTransforms(const string& clipName, float timePos, 
		 vector<XMFLOAT4X4>& finalTransforms)const;

	int32_t NodeToIdx(const string& name) const;
	int32_t NodeToBone(const uint32_t node) const;

	float SecondToTick(const string& clipName, float& s);
private:
	// rootNode = 0;
	vector<uint32_t> parentNode_;
	vector<vector<uint32_t>> childrenNode_;

	unordered_map<string, uint32_t> nodeToIdx_;
	unordered_map<uint32_t, uint32_t> nodeToBone_;

	vector<XMFLOAT4X4> nodeTransforms_;
	vector<XMFLOAT4X4> boneOffsets_;
    
	unordered_map<string, AnimationClip> animations_;
};

struct SkinnedModelInstance
{
	SkinnedData* skinnedInfo_ = nullptr;
	vector<XMFLOAT4X4> finalTransforms_;
	string clipName_;
	float timePos_ = 0.0f;

	void SetAnimtion(const string& clipName, float animationTime)
	{
		clipName_ = clipName;
		timePos_ = animationTime;
	}

	// Called every frame and increments the time position, interpolates the 
	// animations for each bone based on the current animation clip, and 
	// generates the final transforms which are ultimately set to the effect
	// for processing in the vertex shader.
	void UpdateSkinnedAnimation()
	{
		// Loop animation
		float animationTick = skinnedInfo_->SecondToTick(clipName_, timePos_);

		// Compute the final transforms for this time position.
		skinnedInfo_->GetFinalTransforms(clipName_, animationTick, finalTransforms_);
	}
};