#pragma once
#include <cstdint>
template<class T>
class VectorTraits{
public:
    using ValueType = T::ValueType;
    constexpr static const std::size_t Size = T::Size;
};