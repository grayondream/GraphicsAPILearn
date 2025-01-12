#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>



class Camera {
public:
	using Vec3 = glm::vec3;
public:
	enum class Movement {
		Forward,
		Backward,
		Left,
		Right
	};

	struct Option {
		float speed{};
		float sensitivity{};
		float zoom{};
	};

	struct Attribute {
		Vec3 pos{};
		Vec3 front{};
		Vec3 up{};
		Vec3 right{};
		Vec3 worldUp{};
	};

	struct EulerAngle{
		float yaw{};
		float pitch{};
	};
public:
	Camera(const Vec3 pos = Vec3(0.0f, 0.0f, 0.0f), const Vec3 up = Vec3(0.0f, 1.0f, 0.0f), const float yam = -90.0f, const float pitch = 0.0f);

	glm::mat4 getViewMatrix() const {
		return glm::lookAt(_attr.pos, _attr.pos + _attr.front, _attr.up);
	}

	Camera& processKeyboardEvent(const Movement direction, const float delta);

	Camera& processMouseScrool(const float offset);

private:
	Camera& update();

private:
	Attribute _attr{};
	Option _opt{};
	EulerAngle _angle{};
};