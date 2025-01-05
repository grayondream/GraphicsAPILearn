#include "GLAppRegister.hpp"
#include "App/AppRegister.hpp"
#include "GLTriangleApp.hpp"
#include "GLRectApp.hpp"
#include "GLSimpleTextureApp.hpp"
#include "GLCubeApp.hpp"

void RegisterGLApps(){
	AppRegister::instance()->push("GL_Base", std::make_shared<GLApp>());
	AppRegister::instance()->push("GL_Triangle", std::make_shared<GLTriangleApp>());
	AppRegister::instance()->push("GL_Rect", std::make_shared<GLRectApp>());
	AppRegister::instance()->push("GL_SimpleTexture", std::make_shared<GLSimpleTextureApp>());
	AppRegister::instance()->push("GL_Box", std::make_shared<GLCubeApp>());
}