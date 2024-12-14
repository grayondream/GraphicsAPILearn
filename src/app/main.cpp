
#include "app/Application.hpp"
#include "EH/ErrorHandle.hpp"
#include "Base/GameTimer.hpp"
#include "Tracy/ConsoleDebugger.hpp"
#include "Base/Log.hpp"

static inline constexpr int GAME_WIN_WIDTH = 720;
static inline constexpr int GAME_WIN_HEIGHT = 480;
static inline constexpr int GAME_ENABLE_MSAA = true;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nShowCmd) {
    ConsoleDebugger consoleDebugger{};
    Application app{};
    app.init(hInstance, { {GAME_WIN_WIDTH, GAME_WIN_HEIGHT, "Hello DX11!"}, GAME_ENABLE_MSAA});
    return app.run(nShowCmd);
}