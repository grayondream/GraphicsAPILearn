#include "Shape/Image.hpp"
#include "Shape/Position.hpp"
#include <array>

class GLImageTexture2D {
public:
	GLImageTexture2D(const std::string& file);

	GLImageTexture2D& load();

	unsigned int texture();

	float* coord();

	std::size_t coordSize();

private:
	unsigned int generateTextureFrom(const uint8_t* data, const int width, const int height);

private:
	Image _img{};
	unsigned int _texture{};
	std::array<Position2D, 4> _coord{};
};