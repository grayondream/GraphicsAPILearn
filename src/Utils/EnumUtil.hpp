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
#ifdef __clang__
    return std::string_view(str.data() + 99, str.size() - 106);
#elif defined(__GNUC__)
    return std::string_view(str.data() + 49, str.size() - 50);
#elif defined(_MSC_VER)
    return std::string_view(str.data() + 49, str.size() - 50);
#else
    return "";
#endif

}

namespace Detail{
#define kEnumMaxIndex 256
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