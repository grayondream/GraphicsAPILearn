
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
 * Base: 空窗口
 * GL:
 *  -   GL_Base:    空窗口使用GL ClearColor
 * DX11:
 *  -   DX11_Base:  空窗口使用DX11 ClearColor
 *  -   DX11_Triangle   DX11渲染空三角形
 * DX12:
 */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nShowCmd) {
    ConsoleDebugger consoleDebugger{};
    AppRegister::instance()->run();

    const std::string appName = "DX11_Triangle";
    LOGI("Select {} Application", appName);
    auto app = AppRegister::instance()->get(appName);
    assert(app);
    app->init(hInstance, { {GAME_WIN_WIDTH, GAME_WIN_HEIGHT, "Hello DX11!"}, GAME_ENABLE_MSAA});
    return app->run(nShowCmd);
}