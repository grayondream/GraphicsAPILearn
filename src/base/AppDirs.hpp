#pragma once
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <string>

namespace Utils {
namespace AppDirs {

// 可执行文件绝对路径（多配置构建目录下按 exe 自身位置反查资源树，
// 替代对固定 "build/" 目录名的假设）
inline std::string ExePath() {
#if defined(_WIN32)
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    int bytes = WideCharToMultiByte(CP_UTF8, 0, buf, static_cast<int>(n), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(bytes), '\0');
    WideCharToMultiByte(CP_UTF8, 0, buf, static_cast<int>(n), out.data(), bytes, nullptr, nullptr);
    return out;
#elif defined(__linux__)
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return {};
    buf[n] = '\0';
    return std::string(buf);
#else
    return {};
#endif
}

} // namespace AppDirs
} // namespace Utils
