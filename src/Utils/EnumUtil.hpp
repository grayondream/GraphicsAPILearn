#pragma once

namespace Utils{
namespace Enum{
namespace Detail {
    template<auto V>
    constexpr std::string_view sig() {
        return __FUNCSIG__;
    }
}

template<auto V>
constexpr std::string_view EnumName() {
    auto str = Detail::sig<V>();
    return std::string_view(str.data() + 99, str.size() - 106);
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