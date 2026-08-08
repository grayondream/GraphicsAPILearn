#pragma once
#include <native/ImageTexture3D.hpp>
#include <vector>

class GLImageTexture3D : public ImageTexture3D{
public:
	GLImageTexture3D(const std::string& path);

	virtual GLImageTexture3D& load() override ;
};