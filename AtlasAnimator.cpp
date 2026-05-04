#include "AtlasAnimator.h"

// AtlasClip

AtlasAnimationClip::AtlasAnimationClip(const string& name) :
    name_(name)
{
}

uint16_t AtlasAnimationClip::GetAtlasIndexAtTime(const float t, float& currentTimePos, float& nextTimePos) const
{
    if (keyFrames_.empty()) {
        nextTimePos = duration_;
        currentTimePos = 0.0f;
        return 0;
    }

    // 첫 프레임 이전이거나 같을 때
    if (t <= keyFrames_.front().timePos) {
        nextTimePos = (keyFrames_.size() > 1) ? keyFrames_[1].timePos : duration_;
        currentTimePos = keyFrames_.front().timePos;
        return keyFrames_.front().atlasIndex;
    }
    // 마지막 프레임 이후이거나 같을 때
    else if (t >= keyFrames_.back().timePos) {
        nextTimePos = duration_;
        currentTimePos = keyFrames_.back().timePos;
        return keyFrames_.back().atlasIndex;
    }
   
    auto it = std::upper_bound(keyFrames_.begin(), keyFrames_.end(), t,
        [](float t, const AtlasFrame& frame) {
            return t < frame.timePos;
        });

    auto prevIt = std::prev(it);
    nextTimePos = it->timePos;
    currentTimePos = prevIt->timePos;
    return prevIt->atlasIndex;
}

void AtlasAnimationClip::SetDuration(float duration)
{
    duration_ = duration;
}

const string& AtlasAnimationClip::GetName() const
{
    return name_;
}

float AtlasAnimationClip::GetDuration() const
{
    return duration_;
}

void AtlasAnimationClip::SetFrames(vector<AtlasFrame>& frames)
{
    sort(frames.begin(), frames.end(), [](const AtlasFrame& a, const AtlasFrame& b) {
        return a.timePos < b.timePos; });
    keyFrames_ = move(frames);
}

// AtlasAnimator

void AtlasAnimator::SetAnimationProfile(const AtlasAnimatorProfile* animationProfile)
{
    animationProfile_ = animationProfile;
}

void AtlasAnimator::Update(float deltaTime)
{
    // 실행할 필요가 없는 상태면 조기 종료
    if (isPaused_ || !currentClip_ || isFinished_ || playSpeed_ == 0.f)
        return;

    float clipDuration = currentClip_->GetDuration();

    // 방어 코드: 클립 길이가 0 이하라면 즉시 종료 처리 (fmod NaN 방지)
    if (clipDuration <= 0.0f) {
        isFinished_ = true;
        return;
    }

    bool needUpdateIndex = false; // 강제 갱신 플래그

    // 시간 누적
    currentTime_ += deltaTime * playSpeed_;

    // 역방향 재생 처리 (playSpeed_가 음수일 때)
    if (playSpeed_ < 0.0f) {
        if (currentTime_ < 0.0f) {
            if (isLoop_) {
                currentTime_ = clipDuration + fmod(currentTime_, clipDuration);
            }
            else {
                currentTime_ = 0.0f;
                isFinished_ = true;
            }
        }
    }
    // 정방향 재생 처리 (playSpeed_가 양수일 때)
    else {
        if (currentTime_ >= clipDuration) {
            if (isLoop_) {
                currentTime_ = fmod(currentTime_, clipDuration);
                needUpdateIndex = true; // 루프돌아 처음으로 돌아갈때 프레임 강제 갱신
            }
            else {
                currentTime_ = clipDuration;
                isFinished_ = true;
            }
        }
    }

    if (needUpdateIndex || currentTime_ >= nextTimePos_ || currentTime_ < currentTimePos_) {
        currentAtlasIndex_ = currentClip_->GetAtlasIndexAtTime(currentTime_, currentTimePos_, nextTimePos_);
    }
}

void AtlasAnimator::Play(const std::string& name, bool loop, float speed)
{
    if (!animationProfile_)
        return;

    const AtlasAnimationClip* clip = animationProfile_->GetClip(name);

    if (clip) {
        currentClipName_ = name;
        currentClip_ = clip;
        isLoop_ = loop;
        playSpeed_ = speed;
        currentTime_ = 0.0f;
        isFinished_ = false;

        currentAtlasIndex_ = currentClip_->GetAtlasIndexAtTime(currentTime_, currentTimePos_, nextTimePos_);
    }
    else {
        Stop();
    }
}

void AtlasAnimator::Stop()
{
    currentClipName_.clear();
    currentClip_ = nullptr;
    currentTime_ = 0.0f;
    isFinished_ = false;
    isLoop_ =false;
    playSpeed_ = 1.0f;
}

const string& AtlasAnimator::GetCurrentClipName() const
{
    return currentClipName_;
}

float AtlasAnimator::GetProgress() const
{
    // 현재 재생 중인 클립이 존재하지 않으면 0.0f를 반환
    if (!currentClip_) {
        return 0.0f;
    }

    float clipDuration = currentClip_->GetDuration();
    return clipDuration > 0.0f ? min(currentTime_ / clipDuration, 1.0f) : 0.0f;
}

uint16_t AtlasAnimator::GetCurrentAtlasIndex() const
{
    return currentAtlasIndex_;
}
