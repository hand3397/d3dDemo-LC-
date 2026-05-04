#pragma once
#include "AtlasAnimator.h"
#include "SkinnedAnimator.h"

// 애니메이션 클립과 애니메이터 프로필을 소유 및관리하는 클래스입니다.
class AnimationManager
{
public:

    AnimationManager() = default;
    ~AnimationManager();

    void Clear();

    // 3D 애니메이션용
    void AddSkinnedClip(const SkinnedAnimationClip* clip);
    const SkinnedAnimationClip* GetSkinnedClip(const std::string& name) const;
    // 프로필을 만들기 전에는 클립이 모두 추가되어 있어야 한다.
    void AddSkinnedProfile(const string& profileName, const vector<string>& clipNames);
    const SkinnedAnimatorProfile* GetSkinnedProfile(const string& profileName) const;

    // 2D 아틀라스 애니메이션용
    void AddAtlasClip(const AtlasAnimationClip* clip);
    const AtlasAnimationClip* GetAtlasClip(const std::string& name) const;
    // 프로필을 만들기 전에는 클립이 모두 추가되어 있어야 한다.
    void AddAtlasProfile(const string& profileName, const vector<string>& clipNames);
    const AtlasAnimatorProfile* GetAtlasProfile(const string& profileName) const;

private:
    // 3D 애니메이션용
    unordered_map<string, const SkinnedAnimationClip*> skinnedClips_;
    unordered_map<string, SkinnedAnimatorProfile*> skinnedProfiles_;

    // 2D 아틀라스 애니메이션용
    unordered_map<string, const AtlasAnimationClip*> atlasClips_;
    unordered_map<string, AtlasAnimatorProfile*> atlasProfiles_;
};

