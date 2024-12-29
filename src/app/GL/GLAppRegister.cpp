#include "GLAppRegister.hpp"
#include "App/AppRegister.hpp"
#include "App/GL/GLTriangleApp.hpp"

void RegisterGLApps(){
#if ENABLE_OPENGL
	AppRegister::instance()->push("GL_Triangle", std::make_shared<GLTriangleApp>());
#endif
}