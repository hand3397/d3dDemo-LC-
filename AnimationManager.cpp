#include "AnimationManager.h"

AnimationManager::~AnimationManager()
{
    Clear();
}

void AnimationManager::Clear()
{
    for (auto& pair : skinnedProfiles_) {
        delete pair.second;
    }
    for (auto& pair : atlasProfiles_) {
        delete pair.second;
    }
    skinnedProfiles_.clear();
    atlasProfiles_.clear();

    for (auto& pair : skinnedClips_) {
        delete pair.second;
    }
    for (auto& pair : atlasClips_) {
        delete pair.second;
    }
    skinnedClips_.clear();
    atlasClips_.clear();
}

void AnimationManager::AddSkinnedClip(const SkinnedAnimationClip* clip)
{
    if (!clip)
        return;
    skinnedClips_[clip->GetName()] = clip;
}

const SkinnedAnimationClip* AnimationManager::GetSkinnedClip(const std::string& name) const
{
    auto it = skinnedClips_.find(name);
    if (it != skinnedClips_.end()) {
        return it->second;
    }
    return nullptr;
}

void AnimationManager::AddSkinnedProfile(const string& profileName, const vector<string>& clipNames)
{
    SkinnedAnimatorProfile* profile = new SkinnedAnimatorProfile(profileName);

    for (const string& clipName : clipNames) {
        const SkinnedAnimationClip* clip = GetSkinnedClip(clipName);
        if (clip) {
            profile->AddClip(clipName, clip);
        }
    }
    
    skinnedProfiles_[profileName] = profile;
}

const SkinnedAnimatorProfile* AnimationManager::GetSkinnedProfile(const string& profileName) const
{
    auto it = skinnedProfiles_.find(profileName);
    if (it != skinnedProfiles_.end()) {
        return it->second;
    }
    return nullptr;
}

void AnimationManager::AddAtlasClip(const AtlasAnimationClip* clip)
{
    if (!clip)
        return;
    atlasClips_[clip->GetName()] = clip;
}

const AtlasAnimationClip* AnimationManager::GetAtlasClip(const std::string& name) const
{
    auto it = atlasClips_.find(name);
    if (it != atlasClips_.end()) {
        return it->second;
    }
    return nullptr;
}

void AnimationManager::AddAtlasProfile(const string& profileName, const vector<string>& clipNames)
{
    AtlasAnimatorProfile* profile = new AtlasAnimatorProfile(profileName);

    for (const string& clipName : clipNames) {
        const AtlasAnimationClip* clip = GetAtlasClip(clipName);
        if (clip) {
            profile->AddClip(clipName, clip);
        }
    }

    atlasProfiles_[profileName] = profile;
}

const AtlasAnimatorProfile* AnimationManager::GetAtlasProfile(const string& profileName) const
{
    auto it = atlasProfiles_.find(profileName);
    if (it != atlasProfiles_.end()) {
        return it->second;
    }
    return nullptr;
}
