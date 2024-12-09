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

GameTimer::GameTimer() {}

GameTimer& GameTimer::start() {
	return *this;
}

GameTimer& GameTimer::stop() {
	return *this;
}

GameTimer& GameTimer::reset() {
	return *this;
}

GameTimer& GameTimer::tick() {
	return *this;
}