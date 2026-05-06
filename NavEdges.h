#pragma once
#include <cstdint>
#include <NavDir.h>

// 주변 8방향 슬롯에 대한 이동 정보를 갖는다 (총 16비트로 8방향의 지형 속성을 저장)
struct NavEdges
{
public:
    // 지형 속성 (2비트: 0~3)
    enum class Type : uint16_t
    {
        Blocked = 0, // 00
        Flat = 1, // 01
        Rough = 2, // 10
        Stair = 3  // 11
    };

    inline void SetEdge(Nav::Dir dir, Type type)
    {
        // 몇 번째 비트부터 시작하는지 계산 (방향 인덱스 * 2)
        int shift = static_cast<int>(dir) * 2;

        // 기존 해당 자리의 2비트를 00으로 초기화 (Clear)
        connectionMask &= ~(0x3 << shift);
        // 새로운 타입을 해당 자리의 2비트로 설정 (Set)
        connectionMask |= (static_cast<uint16_t>(type) << shift);
    }

    inline Type GetEdge(Nav::Dir dir) const
    {
        int shift = static_cast<int>(dir) * 2;

        // 원하는 자리를 맨 우측으로 밀고, 0x3(11)과 AND 연산하여 2비트만 추출
        return static_cast<Type>((connectionMask >> shift) & 0x3);
    }

private:
    uint16_t connectionMask = 0; // 0으로 초기화 (모든 방향 Blocked)

};


