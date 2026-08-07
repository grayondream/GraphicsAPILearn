#pragma once

#include <string>
#include <cassert>
#include "Base/Log.hpp"
namespace ErrorHandle {
	

	template<class T, class... Args>
	void ExitIfFailed(const T value, Args&&... args) {
		if (!value) {
			assert(0);
			exit(-1);
		}
	}

}