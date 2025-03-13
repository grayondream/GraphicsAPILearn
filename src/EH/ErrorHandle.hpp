#pragma once
#include <windows.h>
#include <format>
#include <string>
#include <cassert>
#include "Base/Log.hpp"
namespace ErrorHandle {
	

	template<class T, class... Args>
	void ExitIfFailed(const T value, Args&&... args) {
		if constexpr (std::is_same_v<T, HRESULT>) {
			if (FAILED(value)) {
				assert(0);
				exit(-1);
			}
		}
		else {
			if (!value) {
				assert(0);
				exit(-1);
			}
		}
	}
}