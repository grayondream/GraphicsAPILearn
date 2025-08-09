#pragma once
#include <cstdint>

enum class Key : int32_t{
    None,
    W,
    A,
    S,
    D,
    Esc,
    Space
};

enum class KeyAction : int32_t{
    None,
    Press,
    Release
};

enum class MouseButton : int32_t{
    None,
    Left,
    Right,
    Middle
};

enum class MouseAction : int32_t{
    None,
    Press,
    Release
};

