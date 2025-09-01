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

    void Interpolate(float t, XMFLOAT4X4& M)const;

	vector<Keyframe> keyframes_;
};

// AnimationClip은 '걷기','뛰기','공격' 같은 개별 애니메이션 클립을 대표한다.
// 하나의 AnimationClip 객체는 애니메이션 클립을 구성하는 BoneAnimation 인스턴스들을 담는다.
struct AnimationClip
{
	void SetClipStartTime();
	void SetClipEndTime();

	float GetClipStartTime()const;
	float GetClipEndTime()const;

    void Interpolate(float t, vector<XMFLOAT4X4>& boneTransforms)const;

	float startTime;
	float endTime;

    vector<BoneAnimation> boneAnimations_;
};

class SkinnedData
{
public:

	UINT BoneCount()const;

	float GetClipStartTime(const string& clipName)const;
	float GetClipEndTime(const string& clipName)const;

	void Set(vector<int>& parentBone,
		vector<vector<int>>& childrenBone,
		unordered_map<string, uint32_t> nameToIdx,
		vector<XMFLOAT4X4>& boneOffsets);

	void AddAnimaiton(const string& clipName, const AnimationClip& animationClip);

	 // In a real project, you'd want to cache the result if there was a chance
	 // that you were calling this several times with the same clipName at 
	 // the same timePos.
    void GetFinalTransforms(const string& clipName, float timePos, 
		 vector<XMFLOAT4X4>& finalTransforms)const;

	int64_t NameToIdx(const string& name);
private:
    // Gives parentIndex of ith bone.
	vector<int> parentBone_;
	vector<vector<int>> childrenBone_;

	unordered_map<string, uint32_t> nameToIdx_;

	vector<XMFLOAT4X4> boneOffsets_;
    
	unordered_map<string, AnimationClip> animations_;
};
