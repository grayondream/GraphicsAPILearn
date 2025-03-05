#include "DX11AppFactory.hpp"
#include "DX11App.hpp"
#include "DX11TriangleApp.hpp"
#include "DX11RectApp.hpp"
#include "DX11SimpleTextureApp.hpp"

std::shared_ptr<DX11App> DX11AppFactory::create(const AppType type) {
	switch(type){
		case AppType::Base:
			return std::make_shared<DX11App>();
		case AppType::Triangle:
			return std::make_shared<DX11TriangleApp>();	
		case AppType::Rect:
			return std::make_shared<DX11RectApp>();	
		case AppType::SimpleTexture:
			return std::make_shared<DX11SimpleTextureApp>();
		default:
			return std::make_shared<DX11App>();
	}

	return nullptr;
}