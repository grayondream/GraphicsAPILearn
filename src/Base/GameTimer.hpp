#pragma once

#include <windows.h>

double GetSecondsPreCount();

class GameTimer{
public:
    using ValueType = __int64;
public:
    GameTimer();

    float gameTime() const;
    float deltaTime() const;
    GameTimer& reset();
    GameTimer& start();
    GameTimer& stop();
    GameTimer& tick();

private:
    double _deltaTime{};
    ValueType _baseTime{};
    ValueType _puseTime{};
    ValueType _stopTime{};
    ValueType _prevTime{};
    ValueType _curTime{};
    bool _stopped{};
};