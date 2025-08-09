#include "EventUtils.hpp"
#include <GLFW/glfw3.h>

namespace Utils{
namespace Event{

    Key ConvertKeyCode(int key){
        switch(key){
            case GLFW_KEY_W: return Key::W;
            case GLFW_KEY_A: return Key::A;
            case GLFW_KEY_S: return Key::S;
            case GLFW_KEY_D: return Key::D;
            case GLFW_KEY_ESCAPE: return Key::Esc;
            case GLFW_KEY_SPACE: return Key::Space;
            default: return Key::None;
        }
    }

    KeyAction ConvertKeyAction(int action){
        switch(action){
            case GLFW_PRESS: return KeyAction::Press;
            case GLFW_RELEASE: return KeyAction::Release;
            default: return KeyAction::None;
        }
    }

    MouseButton ConvertMouseButton(int button){
        switch(button){
            case GLFW_MOUSE_BUTTON_LEFT: return MouseButton::Left;
            case GLFW_MOUSE_BUTTON_RIGHT: return MouseButton::Right;
            case GLFW_MOUSE_BUTTON_MIDDLE: return MouseButton::Middle;
            default: return MouseButton::None;
        }
    }

    MouseAction ConvertMouseAction(int action){
        switch(action){
            case GLFW_PRESS: return MouseAction::Press;
            case GLFW_RELEASE: return MouseAction::Release;
            default: return MouseAction::None;
        }
    }

}
}
