#pragma once
#include "IAnimator.h"
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <DirectXMath.h>

using namespace std;
using namespace DirectX;

// 3D Max, Maya, Blender 같은 툴이나 여기서 뽑아낸 데이터 파일 내부를 까보면
// 애니메이션 길이나 키프레임 위치가 정수형인 틱(Ticks)으로 저장되어 있는 경우가 많다.
// 이를 매번 틱과 초단위로 변경하지 말고
// 모든 것을 로드 타임(Load Time)에 초 단위로 바꾼다.

// Keyframe은 특정 시점에서의 본 변환 상태를 정의
struct Keyframe
{
	Keyframe() = default;
	~Keyframe() = default;

	float timePos_ = 0.0f; // 초 단위
	XMFLOAT3 translation_ = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3 scale_ = XMFLOAT3(1.0f, 1.0f, 1.0f);
	XMFLOAT4 rotationQuat_ = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
};

// BoneAnimation은 한 뼈대의 키프레임 리스트로 정의된다.  
// 특정 시간이 두 키프레임 사이에 있을 경우, 그 시간을 감싸고 있는 두 인접한 키프레임을 기준으로 보간한다.
// 애니메이션은 항상 최소 두 개의 키프레임을 가진다고 가정한다.
struct BoneAnimation
{
	void Interpolate(float t, XMFLOAT4X4& M)const;
	Keyframe InterpolateKeyframe(float t) const;
	
	void SetFrames(vector<Keyframe> frames);
private:
	// 키프레임은 timePos로 오름차순 정렬되어 있다
	vector<Keyframe> keyframes_;
};

// AnimationClip은 '걷기','뛰기','공격' 같은 개별 애니메이션 클립을 대표한다.
// 하나의 AnimationClip 객체는 애니메이션 클립을 구성하는 각각의 BoneAnimation 인스턴스들을 담는다.
// boneAnimations_.size()는 Bone Animation 을 구성하는 뼈대의 개수와 같다.
struct SkinnedAnimationClip
{
	SkinnedAnimationClip() = default;
	SkinnedAnimationClip(const string& name);

	// 인자로 들어오는 vector의 size가 이미 numBones 만큼 resize가 되어 있다고 가정함.
	void InterpolateAll(float t, vector<XMFLOAT4X4>& boneTransforms) const;
	void Interpolate(float t, int i, XMFLOAT4X4& boneTransforms) const;
	void SetBoneAnimaitions(const vector<BoneAnimation>& boneAnimations);

	const string& GetName() const;
	uint32_t NumBones() const; // boneAnimation의 size

	void SetDuration(float duration);
	float GetDuration() const;

private:
	string name_;

	vector<BoneAnimation> boneAnimations_;
	uint32_t numBones = 0;
	float duration_ = 0.0f; // 애니메이션 총 길이 (초 단위)
};

using SkinnedAnimatorProfile = AnimatorProfile<SkinnedAnimationClip>;

class SkinnedAnimator : public IAnimator
{
public:

    void SetAnimationProfile(const SkinnedAnimatorProfile* animationProfile);
	// 매 프레임 애니메이션 시간을 갱신하고 상태를 업데이트
	virtual void Update(float deltaTime) override;

	// 특정 이름의 애니메이션 클립을 재생
	virtual void Play(const std::string& name, bool loop = true, float speed = 1.0f) override;

	// 재생 중인 애니메이션을 중단
	virtual void Stop() override;

	// 현재 재생 중인 애니메이션의 이름을 반환합니다.
	virtual const string& GetCurrentClipName() const override;

	// 현재 애니메이션의 진행률(0.0 ~ 1.0)을 반환
	virtual float GetProgress() const override;

	const vector<XMFLOAT4X4>& GetBoneTransforms() const;
	const XMFLOAT4X4& GetBoneTransform(uint32_t boneIdx) const;

private:

	const SkinnedAnimatorProfile* animationProfile_ = nullptr;
	const SkinnedAnimationClip* currentClip_ = nullptr;

	vector<XMFLOAT4X4> currentBoneTransform_; // 본별 현재 tarnsformMatrix를 캐싱해둠
};

