#include "DX11AppRegister.hpp"
#include "App/AppRegister.hpp"
#include "DX11App.hpp"
#include "DX11TriangleApp.hpp"
#include "DX11RectApp.hpp"
#include "DX11SimpleTextureApp.hpp"

void RegisterDX11Apps() {
	AppRegister::instance()->push("DX11_Base", std::make_shared<DX11App>());
	AppRegister::instance()->push("DX11_Triangle", std::make_shared<DX11TriangleApp>());
	AppRegister::instance()->push("DX11_Rect", std::make_shared<DX11RectApp>());
	AppRegister::instance()->push("DX11_SimpleTexture", std::make_shared<DX11SimpleTextureApp>());
}