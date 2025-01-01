#include "GLAppRegister.hpp"
#include "App/AppRegister.hpp"
#include "App/GL/GLTriangleApp.hpp"

void RegisterGLApps(){
	AppRegister::instance()->push("GL_Base", std::make_shared<GLApp>());
	AppRegister::instance()->push("GL_Triangle", std::make_shared<GLTriangleApp>());
}