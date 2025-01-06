#include "Shape/Image.hpp"
#include "Shape/Position.hpp"
#include <vector>

class GLImageTexture2D {
public:
	GLImageTexture2D(const std::string& file);

	GLImageTexture2D& load();

	unsigned int texture();

	float* coord();

	std::size_t coordSize();

	GLImageTexture2D& multiSurface(const int cnt = 1);

private:
	unsigned int generateTextureFrom(const uint8_t* data, const int width, const int height);

private:
	Image _img{};
	unsigned int _texture{};
	std::vector<Position2D> _coord{};
};