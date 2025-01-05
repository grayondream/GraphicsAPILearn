#pragma once
#include <string>

struct Size2D {
	int width{};
	int height{};
	int channel{};
};

class Image {
public:
	Image(const std::string& file = {});
	~Image();

	Image& load();

	uint8_t* data();

	Size2D size();

private:
	std::string _file{};
	uint8_t* _pdata{};
	Size2D _size{};
};