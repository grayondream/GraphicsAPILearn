#pragma once
#include <Native/ImageTexture2D.hpp>
#include <vector>

class GLImageTexture2D : public ImageTexture2D{
public:
	GLImageTexture2D(const std::string& file, const TextureOption& option = {});

	virtual GLImageTexture2D& load() override ;
};