#include "GLAppRegister.hpp"
#include "App/AppRegister.hpp"
#include "App/GL/Base/GLTriangleApp.hpp"
#include "App/GL/Base/GLRectApp.hpp"
#include "App/GL/Base/GLSimpleTextureApp.hpp"
#include "App/GL/Base/GLCubeApp.hpp"
#include "App/GL/Base/GLCameraApp.hpp"
#include "App/GL/Light/GLSimpleLightGlobalIllum.hpp"
#include "App/GL/Light/GLSimpleLightDiffuse.hpp"
#include "App/GL/Light/GLSimpleLightSpecular.hpp"
#include "App/GL/Light/GLSimpleLightMaterial.hpp"

void RegisterGLApps(){
	AppRegister::instance()->push("GL_Base", std::make_shared<GLApp>());
	AppRegister::instance()->push("GL_Triangle", std::make_shared<GLTriangleApp>());
	AppRegister::instance()->push("GL_Rect", std::make_shared<GLRectApp>());
	AppRegister::instance()->push("GL_SimpleTexture", std::make_shared<GLSimpleTextureApp>());
	AppRegister::instance()->push("GL_Cube", std::make_shared<GLCubeApp>());
	AppRegister::instance()->push("GL_Camera", std::make_shared<GLCameraApp>());
	AppRegister::instance()->push("GL_SimpleLight_GL", std::make_shared<GLSimpleLightGlobalIllum>());
	AppRegister::instance()->push("GL_SimpleLight_Diffuse", std::make_shared<GLSimpleLightDiffuse>());
	AppRegister::instance()->push("GL_SimpleLight_Specular", std::make_shared<GLSimpleLightSpecular>());
	AppRegister::instance()->push("GL_SimpleLight_Material", std::make_shared<GLSimpleLightMaterial>());
}