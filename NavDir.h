#pragma once
#include <cstdint>
#include <DirectXMath.h>

using namespace DirectX;

namespace Nav
{
    // 8방향 인덱스 (0 ~ 7)
    enum class Dir : uint8_t
    {
        N = 0, NE = 1, E = 2, SE = 3,
        S = 4, SW = 5, W = 6, NW = 7,
        COUNT = 8,
        NONE = 255 // 예외 처리용 (유효한 방향이 아님)
    };

    // Nav::Dir의 방향과 일치하도록 dx, dz 배열을 정의 (N, NE, E, SE, S, SW, W, NW)
    static constexpr int dx[8] = { 0,  1,  1,  1,  0, -1, -1, -1 };
    static constexpr int dz[8] = { 1,  1,  0, -1, -1, -1,  0,  1 };
    static constexpr float moveCost[8] = { 1.0f, 1.414f, 1.0f, 1.414f, 1.0f, 1.414f, 1.0f, 1.414f }; // 십자 방향은 1.0, 대각선 방향은 루트2 (약 1.414)

    // Nav::Dir의 방향과 일치하도록 dx, dz 배열을 정의 (N, NE, E, SE, S, SW, W, NW)
    static constexpr float dxf[8] = { 0,  0.7071f,  1,  0.7071f,  0, -0.7071f, -1, -0.7071f };
    static constexpr float dzf[8] = { 1,  0.7071f,  0, -0.7071f, -1, -0.7071f,  0,  0.7071f };
}

