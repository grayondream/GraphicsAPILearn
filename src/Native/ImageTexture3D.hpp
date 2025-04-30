
#include <Geometry/Image.hpp>
#include "ITexture3D.hpp"
#include "Geometry/Vertex.hpp"

#include <vector>
#include <array>
#include <memory>

using GLTextureType = unsigned int;
class ImageTexture3D {
public:
	ImageTexture3D(const std::string& path);

	virtual ImageTexture3D& load();

	float* coord();

	std::size_t coordSize();

	std::shared_ptr<ITexture3D>& texture() {
		return _texture;
	}
private:
	std::array<Image, 6> _imgs{};
	std::vector<Point2D> _coord{};

protected:
	std::shared_ptr<ITexture3D> _texture{};
};