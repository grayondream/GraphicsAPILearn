#include "GameTimer.hpp"
#include <mutex>

double GetSecondsPreCount() {
	static std::once_flag flags{};
	static double v{};
	std::call_once(flags, [](double& v) {
		__int64 count{};
		QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&count));
		v = 1.0 / count;
	}, v);

	return v;
}

__int64 GetCurrentTimeCount() {
	__int64 tt{};
	QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&tt));
	return tt;
}

float GameTimer::deltaTime() const {
	return static_cast<float>(_deltaTime);
}

float GameTimer::totalTime() const {
	if (_stopped) {
		return static_cast<float>(_stopTime - _pauseTime - _baseTime) * GetSecondsPreCount();
	}

	return static_cast<float>(_curTime - _pauseTime - _baseTime) * GetSecondsPreCount();
}

GameTimer::GameTimer() {

}

GameTimer& GameTimer::start() {
	if (!_stopped) {
		return *this;
	}
	const auto curTime = GetCurrentTimeCount();
	_pauseTime += (curTime - _stopTime);
	_prevTime = _curTime;
	_stopTime = {};
	_stopped = false;
	return *this;
}

GameTimer& GameTimer::stop() {
	if (_stopped) {
		return *this;
	}

	_stopTime = GetCurrentTimeCount();
	_stopped = true;
	return *this;
}

GameTimer& GameTimer::reset() {
	const auto curTime = GetCurrentTimeCount();
	_baseTime = curTime;
	_prevTime = curTime;
	_stopTime = 0;
	_stopped = false;
	return *this;
}

GameTimer& GameTimer::tick() {
	if (_stopped) {
		_deltaTime = {};
		return *this;
	}

	_curTime = GetCurrentTimeCount();
	_deltaTime = (_curTime - _prevTime) * GetSecondsPreCount();
	_prevTime = _curTime;
	if (_deltaTime < 0){
		_deltaTime = 0;
	}

	return *this;
}