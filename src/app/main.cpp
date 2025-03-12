
#include "EH/ErrorHandle.hpp"
#include "Base/GameTimer.hpp"
#include "Tracy/ConsoleDebugger.hpp"
#include "Base/Log.hpp"
#include "App/AppFactory.hpp"
#include "App/IApplication.hpp"

static inline constexpr int GAME_WIN_WIDTH = 720;
static inline constexpr int GAME_WIN_HEIGHT = 480;
static inline constexpr int GAME_ENABLE_MSAA = true;

/*
 * Application List:
 * Base: Draw a empty window
 *  Base:                       clear window's color into one color by OpenGL
 *  Triangle                    draw a colored triangle by OpenGL
 *  Rect                        draw a colored rect by OpenGL
 *  SimpleTexture               read a image into texture and draw it on a rect by OpenGL
 *  Cube                        draw a cube by OpenGL
 *  Camera                      create a virtual camera
 *  SimpleLight_Ambinatioin     Global Illumination
 *  SimpleLight_Diffuse         diffuse light
 *  SimpleLight_Specular        Specular light
 *  SimpleLight_Material        Material
 *  SimpleLight_Map             Light Map
 *  SimpleLight_Source_Direction Direction Light Source
 *  SimpleLight_Source_Point    Point Light Source
 *  SimpleLight_Source_Spot     Spot Light Source
 */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nShowCmd) {
    ConsoleDebugger consoleDebugger{};
    
    auto app = AppFactory::create(GraphicsType::GL, AppType::SimpleLight_Source_Direction);
    assert(app);
    app->init(hInstance, { {GAME_WIN_WIDTH, GAME_WIN_HEIGHT, "Hello Grapgic!"}, GAME_ENABLE_MSAA});
    return app->run(nShowCmd);
}