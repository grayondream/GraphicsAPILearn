
#include "app/Application.hpp"
#include "EH/ErrorHandle.hpp"
#include "Base/GameTimer.hpp"
#include "Tracy/ConsoleDebugger.hpp"
#include "Base/Log.hpp"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nShowCmd) {
    ConsoleDebugger consoleDebugger{};
    Application app{};
    app.init(hInstance);
    return app.run(nShowCmd);
}