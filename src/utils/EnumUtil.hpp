#pragma once

namespace Utils{
namespace Enum{
namespace Detail {
    template<auto V>
    constexpr std::string_view sig() {
#ifdef __clang__
        return __PRETTY_FUNCTION__;
#elif defined(__GNUC__)
        return __PRETTY_FUNCTION__;
#elif defined(_MSC_VER) 
        return __FUNCSIG__;
#else
        return "";
#endif
    }
}

template<auto V>
constexpr std::string_view EnumName() {
    auto str = Detail::sig<V>();
#if defined(__GNUC__)
    constexpr std::string_view tag = "[with auto V = ";
    const auto start = str.find(tag);
    if (start == str.npos) return str;
    const auto begin = start + tag.size();
    const auto end = str.find(';', begin);
    return str.substr(begin, end - begin);
#elif defined(__clang__)
    constexpr std::string_view tag = " [V = ";
    const auto pos = str.find(tag);
    if (pos == str.npos) return str;
    const auto begin = pos + tag.size();
    const auto end = str.find(']', begin);
    return str.substr(begin, end - begin);
#elif defined(_MSC_VER)
    // MSVC: "...Detail::sig<Namespace::Name>(void)..."，取 <...> 内的符号名
    // （限定前缀由调用侧 StripEnumPrefix 剥离）
    const auto begin = str.find('<');
    if (begin == str.npos) return str;
    const auto end = str.rfind('>');
    if (end == str.npos || end < begin) return str;
    return str.substr(begin + 1, end - begin - 1);
#else
    return str;
#endif
}

namespace Detail{
#define kEnumMaxIndex 128
#define kEnumMinIndex -10

constexpr auto EnumMaxIndex() {
    return kEnumMaxIndex;
}
constexpr auto EnumMinIndex() {
    return kEnumMinIndex;
}

constexpr auto EnumDefaultSize() {
    return kEnumMaxIndex - kEnumMinIndex;
}

template<class T>
constexpr auto EnumNames() {
    constexpr auto sz = EnumDefaultSize();
    std::array<std::string_view, sz> arr;
    [&arr] <std::size_t... I>(std::index_sequence<I...>) {
        ((arr[I] = EnumName<static_cast<T>(I + kEnumMinIndex)>()), ...);
    }(std::make_index_sequence<sz>{});

    return arr;
}

}//Detail

template<typename T>
constexpr std::string_view EnumName(const T v) {
    return Detail::EnumNames<T>()[static_cast<std::size_t>(v) - kEnumMinIndex];
}

}
}