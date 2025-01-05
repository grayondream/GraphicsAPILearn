#pragma once
#include <windows.h>
#include <format>
#include <string>
#include <cassert>
namespace ErrorHandle {
	template <class... T>
	void MessageError(const std::format_string<T...> fmt, T&&... args) {
		MessageBox(nullptr, std::format(fmt, std::forward<T>(args)...).c_str(), 0, 0);
	}

	template <class... T>
	void PrintLog(const std::format_string<T...> fmt, T&&... args) {
		printf("%s\n", std::format(fmt, std::forward<T>(args)...).c_str());
	}

	template<class T, class... Args>
	void ExitIfFailed(const T value, const std::format_string<Args...> fmt, Args&&... args) {
		if constexpr (std::is_same_v<T, HRESULT>) {
			if (FAILED(value)) {
				PrintLog(fmt, std::forward<Args>(args)...);
				assert(0);
				exit(-1);
			}
		}
		else {
			if (!value) {
				PrintLog(fmt, std::forward<Args>(args)...);
				assert(0);
				exit(-1);
			}
		}
	}
}