#include "BaseSample.hpp"
#include "base/ErrorHandle.hpp"
#include "base/Log.hpp"
#include "utils/EventUtils.hpp"
#include "base/Log.hpp"
#include <glm/glm.hpp>

using namespace ErrorHandle;
using namespace Utils::Event;

bool BaseSample::load(std::shared_ptr<rhi::IRenderer> renderer) {
    if (!Sample::load(std::move(renderer))) return false;
    _camera = Camera(glm::vec3(0.0f, 0.0f, 3.0f));
    return true;
}

void BaseSample::onKeyPress(int key, int scancode, int action, int mods) {
    const auto keycode = ConvertKeyCode(key);
    switch (keycode) {
    case Key::W: _camera.processKeyboardEvent(Camera::Movement::Forward, 0.5); break;
    case Key::S: _camera.processKeyboardEvent(Camera::Movement::Backward, 0.5); break;
    case Key::D: _camera.processKeyboardEvent(Camera::Movement::Right, 0.5); break;
    case Key::A: _camera.processKeyboardEvent(Camera::Movement::Left, 0.5); break;
    default: break;
    }
    Sample::onKeyPress(key, scancode, action, mods);
}

void BaseSample::onMouseButton(int button, int action, int mods) {
    const auto btnState = ConvertMouseAction(action);
    _mouseClicked = (btnState == MouseAction::Press);
}

void BaseSample::onMouseMove(double x, double y) {
    if (!_mouseClicked) {
        _lastPos.x = x;
        _lastPos.y = y;
        return Sample::onMouseMove(x, y);
    }
    const float offx = x - _lastPos.x;
    const float offy = y - _lastPos.y;
    _camera.processMouseMove(offx, offy);
    _lastPos.x = x;
    _lastPos.y = y;
    Sample::onMouseMove(x, y);
}

void BaseSample::onMouseScroll(double xoffset, double yoffset) {
    _camera.processMouseScrool(yoffset);
    Sample::onMouseScroll(xoffset, yoffset);
}
