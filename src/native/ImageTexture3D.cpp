
#include "ImageTexture3D.hpp"
#include "glad/glad.h"
#include <cassert>
#include <filesystem>

std::vector<std::string> GetBoxFacesFiles(const std::string &path) {
	const std::array<std::string, 6> faces{
		"right.jpg",
		"left.jpg",
		"top.jpg",
		"bottom.jpg",
		"front.jpg",
		"back.jpg"
	};

	std::vector<std::string> faceFiles{};
	for (auto name : faces) {
		faceFiles.push_back((std::filesystem::path(path) / name).string());
	}

	return faceFiles;
}

ImageTexture3D::ImageTexture3D(const std::string& path) {
	const std::vector<std::string> faceFiles = GetBoxFacesFiles(path);
	for(auto i = 0;i < faceFiles.size();i ++){
		_imgs[i] = Image(faceFiles[i]);
	}

	_coord = {
		Point2D{1.0, 1.0},
		Point2D{1.0, 0.0},
		Point2D{0.0, 0.0},
		Point2D{0.0, 1.0},
	};
}

ImageTexture3D& ImageTexture3D::load() {
	for (auto && img : _imgs) {
		img.load(false);
	}

	Texture3DDataView datas;
	for (int i = 0; i < _imgs.size(); i++) {
		auto& img = _imgs[i];
		const Texture2DDataView data = { img.data(), img.size().size(), img.format(), img.size() };
		datas.push_back(data);
	}

	_texture->init(datas);
	return *this;
}

float* ImageTexture3D::coord() {
	return reinterpret_cast<float*>(_coord.data());
}

std::size_t ImageTexture3D::coordSize() {
	return _coord.size() * sizeof(Point2D);
}
