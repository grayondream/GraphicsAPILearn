
#include "EH/ErrorHandle.hpp"
#include "Base/GameTimer.hpp"
#include "Tracy/ConsoleDebugger.hpp"
#include "Base/Log.hpp"
#include "App/AppRegister.hpp"
#include "App/IApplication.hpp"

static inline constexpr int GAME_WIN_WIDTH = 720;
static inline constexpr int GAME_WIN_HEIGHT = 480;
static inline constexpr int GAME_ENABLE_MSAA = true;

/*
 * Application List:
 * Base: Draw a empty window
 * GL:
 *  -   GL_Base:                clear window's color into one color by OpenGL
 *  -   GL_Triangle             draw a colored triangle by OpenGL
 *  -   GL_Rect                 draw a colored rect by OpenGL
 *  -   GL_SimpleTexture        read a image into texture and draw it on a rect by OpenGL
 *  -   GL_Cube                 draw a cube by OpenGL
 *  -   GL_Camera               create a virtual camera
 *  -   GL_SimpleLight_GL       Global Illumination
 *  -   GL_SimpleLight_Diffuse  diffuse light
 *  -   GL_SimpleLight_Specular Specular light
 * DX11:
 *  -   DX11_Base:          clear window's color into one color by DX11
 *  -   DX11_Triangle       draw a colored triangle by DX11
 *  -   DX11_Rect           draw a colored rect by DX11
 *  -   DX11_SimpleTexture  TODO
 *  -   DX11_Cube             TODO
 * DX12:
 */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nShowCmd) {
    ConsoleDebugger consoleDebugger{};
    AppRegister::instance()->run();

    const std::string appName = "GL_SimpleLight_Specular";
    LOGI("Select {} Application", appName);
    auto app = AppRegister::instance()->get(appName);
    assert(app);
    app->init(hInstance, { {GAME_WIN_WIDTH, GAME_WIN_HEIGHT, "Hello Grapgic!"}, GAME_ENABLE_MSAA});
    return app->run(nShowCmd);
}