#include "GLAppRegister.hpp"
#include "App/AppRegister.hpp"
#include "App/GL/GLTriangleApp.hpp"
#include "App/GL/GLRectApp.hpp"

void RegisterGLApps(){
	AppRegister::instance()->push("GL_Base", std::make_shared<GLApp>());
	AppRegister::instance()->push("GL_Triangle", std::make_shared<GLTriangleApp>());
	AppRegister::instance()->push("GL_Rect", std::make_shared<GLRectApp>());
}