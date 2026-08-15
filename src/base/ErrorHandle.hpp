#pragma once

#include <string>
#include <cstdlib>
#include <utility>
#include <type_traits>
#include <string_view>
#include <fmt/core.h>
#include "base/Log.hpp"
namespace ErrorHandle {

	namespace detail {
		template<class T>
		std::string FormatArgs(const T& t) {
			if constexpr (std::is_convertible_v<T, std::string_view> ||
			              std::is_constructible_v<std::string, T>) {
				return std::string(t);
			} else {
				return fmt::format("{}", t);
			}
		}
	}

	template<class T, class... Args>
	void ExitIfFailed(const T value, Args&&... args) {
		if (!value) {
			// 打印失败原因后干净退出（不用 assert(0)，避免 abort 崩溃）。
			if constexpr (sizeof...(Args) > 0) {
				// 拼接所有参数为字符串（不依赖 fmt 编译期格式串校验）
				std::string msg;
				int n = 0;
				auto append = [&](const auto& a) {
					if (n++) msg += " ";
					msg += detail::FormatArgs(a);
				};
				(append(args), ...);
				spdlog::error("{}", msg);
			} else {
				spdlog::error("ExitIfFailed");
			}
			std::exit(-1);
		}
	}

}