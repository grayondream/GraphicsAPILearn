#include <windows.h>
#include <format>
#include <string>

namespace ErrorHandle {
	template <class... T>
	void MessageError(const std::format_string<T...> fmt, T&&... args) {
		MessageBox(nullptr, std::format(fmt, args...).c_str(), 0, 0);
	}

	template<class T, class... Args>
	void ExitIfFailed(const T value, const std::format_string<Args...> fmt, Args&&... args) {
		if constexpr (std::is_same_v<T, HRESULT>) {
			if (FAILED(value)) {
				MessageError(fmt, args...);
				exit(-1);
			}
		}
		else {
			if (!value) {
				MessageError(fmt, args...);
				exit(-1);
			}
		}
	}
}