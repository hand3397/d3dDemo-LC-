#pragma once
#include "IAnimator.h"
#include <vector>
#include <unordered_map>
#include <algorithm>

using namespace std;

struct AtlasFrame
{
	uint16_t atlasIndex = 0;
	float timePos = 0.0f;
};

struct AtlasAnimationClip
{
    AtlasAnimationClip() = default;
    AtlasAnimationClip(const string& name);

    uint16_t GetAtlasIndexAtTime(const float time, float& currentTimePos, float& nextTimePos) const;

	void SetDuration(float duration);

    const string& GetName() const;
	float GetDuration() const;

    void SetFrames(vector<AtlasFrame>& frames);

private:

	string name_;
    // 항상 모든 프레임이 timePos 오름차순으로 정렬된 상태
    vector<AtlasFrame> keyFrames_;

	// 애니메이션 길이 (초 단위)
	float duration_ = 0.0f;
};

using AtlasAnimatorProfile = AnimatorProfile<AtlasAnimationClip>;

class AtlasAnimator : public IAnimator
{
public:
    void SetAnimationProfile(const AtlasAnimatorProfile* animationProfile);

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

    uint16_t GetCurrentAtlasIndex() const;

private:

    // 애니메이션 클립 이름과 클립 객체를 매핑하는 객체
    const AtlasAnimatorProfile* animationProfile_ = nullptr;
    const AtlasAnimationClip* currentClip_ = nullptr;

    uint16_t currentAtlasIndex_ = 0;
    float nextTimePos_ = 0.f; // 다음 프레임의 시작 시간
    float currentTimePos_ = 0.f; // 현재 프레임의 시작 시간
};

