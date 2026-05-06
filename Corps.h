#pragma once
#include <vector>
#include <memory>
#include <DirectXMath.h>
#include "Character.h"
#include <algorithm>
#include "TStaeg.h"

using namespace std;
using namespace DirectX;

constexpr uint32_t MAX_CORPS_SIZE = 16; // 군단 하나에 최대 몇 명의 유닛이 속할 수 있는지

// 군단 전체의 현재 목표/상태를 정의
enum class CorpsState
{
    IDLE,
    MOVING,
};

class Corps
{
public:
    Corps();
    ~Corps();

    void Update(float dt);

    void SetUnits(int numUnits, vector<Character*>& units);
    void SetStage(TStaeg* stage);

    void CommandMove(const XMFLOAT3& destination);
    void CommandAttack(Corps* targetEnemy);
    void CommandStop();

    int GetNumUnits() const;
    bool IsEmpty() const;
    XMFLOAT3 GetPosition() const; // 군단의 현재 위치 반환

private:
    // 진형(Formation) 계산: 목적지를 기준으로 각 유닛이 서야 할 위치를 정해줌
    void CalculateFormationPositions(const XMFLOAT3& targetDestination);

private:
    vector<Character*> units_; // 군단에 속한 유닛들
    TStaeg* stage_ = nullptr; // 군단이 속한 스테이지 (필드 정보 접근용)
    uint32_t numUnits_ = 0;

    CorpsState currentState_ = CorpsState::IDLE;
    
    XMFLOAT3 currentPosition_ = XMFLOAT3(0.0f, 0.0f, 0.0f); // 군단의 현재 위치
    XMFLOAT3 currentDestination_ = XMFLOAT3(0.0f, 0.0f, 0.0f); // 군단이 현재 이동 중인 목적지 (마우스 클릭으로 설정됨)

    // 진형 관련 설정 (예: 유닛 간의 간격)
    //float formationSpacing_ = 2.0f;
};