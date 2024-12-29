
#include "EH/ErrorHandle.hpp"
#include "Base/GameTimer.hpp"
#include "Tracy/ConsoleDebugger.hpp"
#include "Base/Log.hpp"
#include "App/AppRegister.hpp"
#include "App/IApplication.hpp"

static inline constexpr int GAME_WIN_WIDTH = 720;
static inline constexpr int GAME_WIN_HEIGHT = 480;
static inline constexpr int GAME_ENABLE_MSAA = true;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nShowCmd) {
    ConsoleDebugger consoleDebugger{};
    AppRegister::instance()->run();

    const std::string appName = "GL_Triangle";
    auto app = AppRegister::instance()->get(appName);
    assert(app);
    app->init(hInstance, { {GAME_WIN_WIDTH, GAME_WIN_HEIGHT, "Hello DX11!"}, GAME_ENABLE_MSAA});
    return app->run(nShowCmd);
}