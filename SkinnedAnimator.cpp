#include "SkinnedAnimator.h"

// BoneAnimation

void BoneAnimation::Interpolate(float t, XMFLOAT4X4& M) const
{
    const Keyframe& kf = InterpolateKeyframe(t);

    XMVECTOR S = XMLoadFloat3(&kf.scale_);
    XMVECTOR P = XMLoadFloat3(&kf.translation_);
    XMVECTOR Q = XMLoadFloat4(&kf.rotationQuat_);

    XMStoreFloat4x4(&M, XMMatrixAffineTransformation(S, XMVectorZero(), Q, P));
}

Keyframe BoneAnimation::InterpolateKeyframe(float t) const
{
	Keyframe out;
    if (keyframes_.empty()) {
        return out;
    }

    if (t <= keyframes_.front().timePos_) {
        return keyframes_.front();
    }

    if (t >= keyframes_.back().timePos_) {
        return keyframes_.back();
    }

    // t보다 큰 첫 번째 timePos_를 가진 키프레임을 찾기
    auto it = std::upper_bound(keyframes_.begin(), keyframes_.end(), t,
        [](float time, const auto& kf) {
            return time < kf.timePos_;
        });

    const auto& kf0 = *(it - 1);
    const auto& kf1 = *it;

    // 보간 비율 계산 및 보간 수행
    float lerpAlpha = (t - kf0.timePos_) / (kf1.timePos_ - kf0.timePos_);

    XMVECTOR s0 = XMLoadFloat3(&kf0.scale_);
    XMVECTOR s1 = XMLoadFloat3(&kf1.scale_);

    XMVECTOR p0 = XMLoadFloat3(&kf0.translation_);
    XMVECTOR p1 = XMLoadFloat3(&kf1.translation_);

    XMVECTOR q0 = XMLoadFloat4(&kf0.rotationQuat_);
    XMVECTOR q1 = XMLoadFloat4(&kf1.rotationQuat_);

    out.timePos_ = t;
    XMStoreFloat3(&out.scale_, XMVectorLerp(s0, s1, lerpAlpha));
    XMStoreFloat3(&out.translation_, XMVectorLerp(p0, p1, lerpAlpha));
    XMStoreFloat4(&out.rotationQuat_, XMQuaternionSlerp(q0, q1, lerpAlpha));

    return out;
}

void BoneAnimation::SetFrames(vector<Keyframe> frames)
{
	sort(frames.begin(), frames.end(), [](const Keyframe& a, const Keyframe& b) {
		return a.timePos_ < b.timePos_;
		});

	keyframes_ = move(frames);
}

// AnimationClip

SkinnedAnimationClip::SkinnedAnimationClip(const string& name) :
    name_(name)
{
}

void SkinnedAnimationClip::InterpolateAll(float t, vector<XMFLOAT4X4>& boneTransforms) const
{
    for (uint32_t i = 0; i < numBones; ++i) {
        boneAnimations_[i].Interpolate(t, boneTransforms[i]);
    }
}

void SkinnedAnimationClip::Interpolate(float t, int i, XMFLOAT4X4& boneTransforms) const
{
    boneAnimations_[i].Interpolate(t, boneTransforms);
}

void SkinnedAnimationClip::SetBoneAnimaitions(const vector<BoneAnimation>& boneAnimations)
{
	boneAnimations_ = boneAnimations;
    numBones = boneAnimations_.size();
}

const string& SkinnedAnimationClip::GetName() const
{
    return name_;
}

uint32_t SkinnedAnimationClip::NumBones() const
{
    return numBones;
}

void SkinnedAnimationClip::SetDuration(float duration)
{
	duration_ = duration;
}

float SkinnedAnimationClip::GetDuration() const
{
	return duration_;
}

// SkinnedAnimator

void SkinnedAnimator::SetAnimationProfile(const SkinnedAnimatorProfile* animationProfile)
{
    animationProfile_ = animationProfile;
}

void SkinnedAnimator::Update(float deltaTime)
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
            }
            else {
                currentTime_ = clipDuration;
                isFinished_ = true;
            }
        }
    }

    currentClip_->InterpolateAll(currentTime_, currentBoneTransform_);
}

void SkinnedAnimator::Play(const std::string& name, bool loop, float speed)
{
    const SkinnedAnimationClip* clip = animationProfile_->GetClip(name);

    if (clip) {
        currentClipName_ = name;
        currentClip_ = clip;
        isLoop_ = loop;
        playSpeed_ = speed;
        currentTime_ = 0.0f;
        isFinished_ = false;

        currentBoneTransform_.resize(currentClip_->NumBones());
        currentClip_->InterpolateAll(0.f, currentBoneTransform_);
    }
    else {
        Stop();
    }
}

void SkinnedAnimator::Stop()
{
    currentBoneTransform_.clear();
    currentClipName_.clear();
    currentClip_ = nullptr;
    currentTime_ = 0.0f;
    isFinished_ = false;
    isLoop_ = false;
    playSpeed_ = 1.0f;
}

const string& SkinnedAnimator::GetCurrentClipName() const
{
	return currentClipName_;
}

float SkinnedAnimator::GetProgress() const
{
    // 현재 재생 중인 클립이 존재하지 않으면 0.0f를 반환
    if (!currentClip_) {
        return 0.0f;
    }

    float clipDuration = currentClip_->GetDuration();
    return clipDuration > 0.0f ? min(currentTime_ / clipDuration, 1.0f) : 0.0f;
}

const vector<XMFLOAT4X4>& SkinnedAnimator::GetBoneTransforms() const
{
    return currentBoneTransform_;
}

const XMFLOAT4X4& SkinnedAnimator::GetBoneTransform(uint32_t boneIdx) const
{
    return currentBoneTransform_[boneIdx];
}


