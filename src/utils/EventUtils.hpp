#pragma once
#include "Base/InputEvent.hpp"

namespace Utils{
namespace Event{

    Key ConvertKeyCode(int key);
    KeyAction ConvertKeyAction(int action);
    MouseButton ConvertMouseButton(int button);
    MouseAction ConvertMouseAction(int action);

}
}


