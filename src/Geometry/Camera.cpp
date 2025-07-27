#include "Camera.hpp"

Camera::Camera(const Vec3 pos, const Vec3 up, const float yam, const float pitch) {
	_attr.pos = pos;
	_attr.front = glm::vec3(0.0f, 0.0f, -1.0f);
	_attr.worldUp = up;

	_opt.speed = 0.3f;
	_opt.sensitivity = 0.01f;
	_opt.zoom = 60.f;
	_angle.pitch = pitch;
	_angle.yaw = yam;
	update();
}

glm::mat4 Camera::getViewMatrix() const{
	return glm::lookAt(_attr.pos, _attr.pos + _attr.front, _attr.up);
}

Camera& Camera::processKeyboardEvent(const Movement direction, const float delta) {
	const float velocity = _opt.speed * delta;
	switch (direction) {
	case Movement::Forward: {
		_attr.pos += _attr.front * velocity;
	}break;
	case Movement::Backward: {
		_attr.pos -= _attr.front * velocity;
	}break;
	case Movement::Left: {
		_attr.pos -= _attr.right * velocity;
	}break;
	case Movement::Right: {
		_attr.pos += _attr.right * velocity;
	}break;
	default:
		break;
	}

	return *this;
}

Camera& Camera::processMouseMove(const float xoff, const float yoff) {
	const auto xoffset = xoff * _opt.sensitivity;
	const auto yoffset = yoff * _opt.sensitivity;
	_angle.yaw += xoffset;
	_angle.pitch += yoffset;
	_angle.pitch = std::max(-89.0f, std::min(89.0f, _angle.pitch));
	update();
	return *this;
}

Camera& Camera::processMouseScrool(const float offset) {
	_opt.zoom -= offset / 20.0;
	_opt.zoom = std::max(std::min(_opt.zoom, 200.0f), 0.1f);
	return *this;
}

Camera& Camera::update() {
	glm::vec3 front;
	front.x = cos(glm::radians(_angle.yaw)) * cos(glm::radians(_angle.pitch));
	front.y = sin(glm::radians(_angle.pitch));
	front.z = sin(glm::radians(_angle.yaw)) * cos(glm::radians(_angle.pitch));
	_attr.front = glm::normalize(front);
	// also re-calculate the Right and Up vector
	_attr.right = glm::normalize(glm::cross(_attr.front, _attr.worldUp));  // normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
	_attr.up = glm::normalize(glm::cross(_attr.right, _attr.front));
	return *this;
}