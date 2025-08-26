#include "GameTimer.h"

GameTimer::GameTimer()
: delta_time(0.0), paused_time(0), stopped(false) {
    Reset();
}

float GameTimer::TotalTime() const {
    if (stopped) {
        return std::chrono::duration<float>((stop_time - paused_time) - base_time).count();
    }
    else {
        auto now = clock::now();
        return std::chrono::duration<float>((now - paused_time) - base_time).count();
    }
}

float GameTimer::DeltaTime() const {
    return delta_time.count();
}

void GameTimer::Reset() {
    base_time = clock::now();
    prev_time = base_time;
    stop_time = clock::time_point();
    paused_time = std::chrono::nanoseconds::zero();
    stopped = false;
}

void GameTimer::Start() {
    if (stopped) {
        auto start_time = clock::now();
        paused_time += std::chrono::duration_cast<std::chrono::nanoseconds>(start_time - stop_time);
        prev_time = start_time;
        stopped = false;
    }
}

void GameTimer::Stop() {
    if (!stopped) {
        stop_time = clock::now();
        stopped = true;
    }
}

void GameTimer::Tick() {
    if (stopped) {
        delta_time = std::chrono::duration<float>(0);
        return;
    }

    auto curr = clock::now();
    delta_time = std::chrono::duration<float>(curr - prev_time);
    prev_time = curr;

    if (delta_time.count() < 0.0f)
        delta_time = std::chrono::duration<float>(0);
}
