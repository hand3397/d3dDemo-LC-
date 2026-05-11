#pragma once
#include <cstdint>

namespace spe {;

enum class ColCategory : uint16_t
{
    NONE = 0x0000,
    DEFAULT = 0x0001,     // 기본 물체 (벽 등)
    CHARACTER = 0x0002,      // 플레이어
    PASSCHARACTER = 0x0004,      // 플레이어
    ENEMY = 0x0008,       // 적
    PROJECTILE = 0x0010,  // 투사체
    GHOST = 0x0020,       // 유체화/무적 상태
    A = 0x0040,       // A
    B = 0x0080,       // B
    C = 0x0100,       // C
    D = 0x0200,       // D
    E = 0x0400,       // E
    ALL = 0xFFFF        // 모든 것과 충돌
};

struct ContactFilter
{
    ContactFilter() = default;
    ContactFilter(uint16_t category, uint16_t mask);

    // 기본값 초기화
    uint16_t categoryBits = static_cast<uint16_t>(ColCategory::DEFAULT);
    uint16_t maskBits = static_cast<uint16_t>(ColCategory::ALL);

    void Set(uint16_t category, uint16_t mask);

    // 특정 대상과의 충돌을 허용
    void AddCollision(uint16_t targetCategory);

    // 특정 대상과의 충돌을 무시
    void IgnoreCollision(uint16_t targetCategory);

    // 내 정체성(카테고리) 추가/제거 
    void AddCategory(uint16_t category);
    void RemoveCategory(uint16_t category);
};

// 전역 도우미 함수 선언
bool ShouldCollide(const ContactFilter& filterA, const ContactFilter& filterB);

class FilterPreset
{
public:
    static ContactFilter GetDefaultFilter()
    {
        static const ContactFilter filter = []() {
            ContactFilter f(static_cast<uint16_t>(ColCategory::DEFAULT), static_cast<uint16_t>(ColCategory::ALL));
            return f;
            }();
        return filter;
    }
    
    static ContactFilter GetCharacterFilter()
    {
        static const ContactFilter filter = []() {
            ContactFilter f(static_cast<uint16_t>(ColCategory::CHARACTER), static_cast<uint16_t>(ColCategory::ALL));
            f.IgnoreCollision(static_cast<uint16_t>(ColCategory::PASSCHARACTER)); // PASS 카테고리와 충돌 무시
            return f;
            }();
        return filter;
    }

    static ContactFilter GetPassCharacterFilter()
    {
        static const ContactFilter filter = []() {
            ContactFilter f(static_cast<uint16_t>(ColCategory::PASSCHARACTER), static_cast<uint16_t>(ColCategory::ALL));
            f.IgnoreCollision(static_cast<uint16_t>(ColCategory::CHARACTER)); // CHARACTER 카테고리와 충돌 무시
            f.IgnoreCollision(static_cast<uint16_t>(ColCategory::PASSCHARACTER)); // PASSCHARACTER 카테고리와 충돌 무시
            return f;
            }();
        return filter;
    }
};

} // namespace spe