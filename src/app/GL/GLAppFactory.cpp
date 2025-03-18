#include "GLAppFactory.hpp"
#include "App/GL/Base/GLTriangleApp.hpp"
#include "App/GL/Base/GLRectApp.hpp"
#include "App/GL/Base/GLSimpleTextureApp.hpp"
#include "App/GL/Base/GLCubeApp.hpp"
#include "App/GL/Base/GLCameraApp.hpp"
#include "App/GL/Light/GLSimpleLightAmbination.hpp"
#include "App/GL/Light/GLSimpleLightDiffuse.hpp"
#include "App/GL/Light/GLSimpleLightSpecular.hpp"
#include "App/GL/Light/GLSimpleLightMaterial.hpp"
#include "App/GL/Light/GLSimpleLightMap.hpp"
#include "App/GL/Light/LightSource/GLLightSourceDirection.hpp"
#include "App/GL/Light/LightSource/GLLightSourcePoint.hpp"
#include "App/GL/Light/LightSource/GLLightSourceSpot.hpp"
#include "App/GL/Light/LightSource/GLLightSourceMult.hpp"
#include "App/GL/Model/GLLoadModelApp.hpp"

std::shared_ptr<GLApp> GLAppFactory::create(const AppType type){
	switch(type){
		case AppType::Base:
			return std::make_shared<GLApp>();
		case AppType::Triangle:
			return std::make_shared<GLTriangleApp>();
		case AppType::Rect:
			return std::make_shared<GLRectApp>();
		case AppType::SimpleTexture:
			return std::make_shared<GLSimpleTextureApp>();
		case AppType::Cube:
			return std::make_shared<GLCubeApp>();
		case AppType::Camera:
			return std::make_shared<GLCameraApp>();
		case AppType::SimpleLight_Ambination:
			return std::make_shared<GLSimpleLightAmbination>();
		case AppType::SimpleLight_Diffuse:
			return std::make_shared<GLSimpleLightDiffuse>();
		case AppType::SimpleLight_Specular:
			return std::make_shared<GLSimpleLightSpecular>();
		case AppType::SimpleLight_Material:
			return std::make_shared<GLSimpleLightMaterial>();
		case AppType::SimpleLight_Map:
			return std::make_shared<GLSimpleLightMap>();
		case AppType::SimpleLight_Source_Direction:
			return std::make_shared<GLLightSourceDirection>();
		case AppType::SimpleLight_Source_Point:
			return std::make_shared<GLLightSourcePoint>();
		case AppType::SimpleLight_Source_Spot:
			return std::make_shared<GLLightSourceSpot>();
		case AppType::SimpleLight_Source_Mult:
			return std::make_shared<GLLightSourceMult>();
		case AppType::LoadModel:
			return std::make_shared<GLLoadModelApp>();
		default:
			break;	
	}

	return nullptr;
}