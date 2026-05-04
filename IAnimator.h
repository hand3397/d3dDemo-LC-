#pragma once
#include <string>
#include <unordered_map>

using namespace std;
// 애니메이터 공통 인터페이스
// 2D Atlas 방식과 3D Skinned Mesh 방식의 애니메이터가 존재

// "기사", "궁수", "마법사" 등 직업군(프리팹) 단위로 하나씩만 생성되는 공유 객체입니다.
// AnimatorProfile은 애니메이션 클립들을 이름과 함께 보관하는 역할을 한다.
template<typename AnimationType>
class AnimatorProfile
{
public:

    AnimatorProfile(const string& name) : name_(name) {}

    const string& GetName() const { return name_; }

    void AddClip(const std::string& stateName, const AnimationType* clip)
    {
        clips_[stateName] = clip;
    }

    const AnimationType* GetClip(const std::string& stateName) const
    {
        auto it = clips_.find(stateName);
        return (it != clips_.end()) ? it->second : nullptr;
    }

private:

    string name_;
    unordered_map<string, const AnimationType*> clips_;

};

class IAnimator
{
public:
    virtual ~IAnimator() = default;

    // 매 프레임 애니메이션 시간을 갱신하고 상태를 업데이트
    virtual void Update(float deltaTime) = 0;

    // 특정 이름의 애니메이션 클립을 재생
    virtual void Play(const string& name, bool loop = true, float speed = 1.0f) = 0;

    // 재생 중인 애니메이션을 중단
    virtual void Stop() = 0;

    // 현재 재생 중인 애니메이션의 이름을 반환합니다.
    virtual const string& GetCurrentClipName() const = 0;

    // 현재 애니메이션의 진행률(0.0 ~ 1.0)을 반환
    virtual float GetProgress() const = 0;

    // 현재 애니메이션이 끝났는지 확인 (루프가 아닐 때 유용)
    bool IsFinished() const;
    // 재생 중인 애니메이션을 일시정지
    void Pause();
    // 일시정지된 애니메이션을 다시 재생
    void Resume();

    // 재생 속도를 실시간으로 조절
    void SetPlaySpeed(float speed);
    float GetPlaySpeed() const;

protected:
    string currentClipName_;  // 현재 재생 중인 클립 이름

    float currentTime_ = 0.0f;     // 현재 재생 시간
    float playSpeed_ = 1.0f;       // 재생 속도 배율
    bool isLoop_ = true;           // 반복 재생 여부
    bool isFinished_ = false;      // 종료 여부
    bool isPaused_ = false;        // 일시정지 여부
};