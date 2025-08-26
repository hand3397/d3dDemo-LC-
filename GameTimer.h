/*
    GameTimer
*/
#pragma once

#include <chrono>

class GameTimer {
public:
    using clock = std::chrono::high_resolution_clock;

    GameTimer();

    float TotalTime()const;
    float DeltaTime()const;

    void Reset();
    void Start();
    void Stop();
    void Tick();

private:
    clock::time_point base_time;
    clock::time_point prev_time;
    clock::time_point stop_time;

    std::chrono::nanoseconds paused_time;
    std::chrono::duration<float> delta_time;

    bool stopped;
};