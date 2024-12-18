#pragma once

#include <windows.h>

double GetSecondsPreCount();

__int64 GetCurrentTimeCount();

class GameTimer{
public:
    using ValueType = __int64;
public:
    GameTimer();

    float deltaTime() const;
    float totalTime() const;
    GameTimer& reset();
    GameTimer& start();
    GameTimer& stop();
    GameTimer& tick();
    
private:
    double _deltaTime{};
    ValueType _baseTime{};
    ValueType _pauseTime{};
    ValueType _stopTime{};
    ValueType _prevTime{};
    ValueType _curTime{};
    bool _stopped{};
};