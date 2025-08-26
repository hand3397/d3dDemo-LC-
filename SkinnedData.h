#pragma once

#include "d3dUtil.h"
#include "MathHelper.h"

// Keyframe은 특정 시점에서의 본 변환 상태를 정의
struct Keyframe
{
	Keyframe();
	~Keyframe();

    float TimePos;
	DirectX::XMFLOAT3 Translation;
    DirectX::XMFLOAT3 Scale;
    DirectX::XMFLOAT4 RotationQuat;
};

// BoneAnimation은 키프레임들의 리스트로 정의된다.  
// 특정 시간이 두 키프레임 사이에 있을 경우, 그 시간을 감싸고 있는 두 인접한 키프레임을 기준으로 보간한다.
// 애니메이션은 항상 최소 두 개의 키프레임을 가진다고 가정한다.
struct BoneAnimation
{
	float GetStartTime()const;
	float GetEndTime()const;

    void Interpolate(float t, DirectX::XMFLOAT4X4& M)const;

	std::vector<Keyframe> Keyframes; 	
};

// AnimationClip은 '걷기','뛰기','공격' 같은 개별 애니메이션 클립을 대표한다.
// 하나의 AnimationClip 객체는 애니메이션 클립을 구성하는 BoneAnimation 인스턴스들을 담는다.
struct AnimationClip
{
	float GetClipStartTime()const;
	float GetClipEndTime()const;

    void Interpolate(float t, std::vector<DirectX::XMFLOAT4X4>& boneTransforms)const;

    std::vector<BoneAnimation> BoneAnimations; 	
};

class SkinnedData
{
public:

	UINT BoneCount()const;

	float GetClipStartTime(const std::string& clipName)const;
	float GetClipEndTime(const std::string& clipName)const;

	void Set(std::vector<int>& boneHierarchy, 
		std::vector<DirectX::XMFLOAT4X4>& boneOffsets,
		std::unordered_map<std::string, AnimationClip>& animations);

	 // In a real project, you'd want to cache the result if there was a chance
	 // that you were calling this several times with the same clipName at 
	 // the same timePos.
    void GetFinalTransforms(const std::string& clipName, float timePos, 
		 std::vector<DirectX::XMFLOAT4X4>& finalTransforms)const;

private:
    // Gives parentIndex of ith bone.
	std::vector<int> mBoneHierarchy;

	std::vector<DirectX::XMFLOAT4X4> mBoneOffsets;
   
	std::unordered_map<std::string, AnimationClip> mAnimations;
};
